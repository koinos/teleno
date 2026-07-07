#include "block_store/block_store.hpp"
#include "koinos/chain/chain.pb.h"
#include "koinos/chain/state.hpp"
#include "koinos/state_db/backends/rocksdb/rocksdb_backend.hpp"
#include "koinos/state_db/state_db.hpp"
#include "koinos/state_db/state_delta.hpp"
#include "storage/rocksdb_manager.hpp"

#include <google/protobuf/util/json_util.h>

#include <koinos/crypto/merkle_tree.hpp>
#include <koinos/crypto/multihash.hpp>
#include <koinos/log.hpp>
#include <koinos/protocol/protocol.pb.h>
#include <koinos/rpc/block_store/block_store_rpc.pb.h>
#include <koinos/util/conversion.hpp>

#include <rocksdb/db.h>
#include <rocksdb/options.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstdint>
#include <ctime>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <cstring>
#include <limits>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

#if defined( _WIN32 )
#include <io.h>
#else
#include <sys/resource.h>
#include <unistd.h>
#endif

namespace {

using koinos::node::storage::ColumnFamily;

struct Args
{
  std::filesystem::path source_basedir;
  std::filesystem::path source_db;
  std::filesystem::path scratch_state_dir;
  std::filesystem::path genesis_file;
  std::filesystem::path log_file;
  std::filesystem::path height_index_dir;
  std::filesystem::path journal_dir;
  uint64_t from_height = 0;
  uint64_t to_height = 0;
  uint64_t progress_every = 10'000;
  uint32_t batch_size = koinos::node::block_store::BlockStore::max_block_request;
  bool reset_scratch = false;
  bool json = false;
  bool normal_removes = false;
  bool no_log_file = false;
  bool rebuild_height_index = false;
  bool sync_scratch_writes = false;
  bool state_db_replay = false;
  bool rebuild_journal = false;
  bool help = false;
};

struct ReplayStats
{
  uint64_t source_head_height = 0;
  uint64_t scratch_start_height = 0;
  uint64_t from_height = 0;
  uint64_t to_height = 0;
  uint64_t final_height = 0;
  uint64_t blocks_checked = 0;
  uint64_t receipt_delta_entries = 0;
  uint64_t receipt_puts = 0;
  uint64_t receipt_removes = 0;
  uint64_t receipts_without_state_root = 0;
  std::string final_block_id;
  std::string final_state_merkle_root;
};

std::string json_escape( const std::string& value )
{
  std::ostringstream out;
  for( unsigned char ch: value )
  {
    switch( ch )
    {
      case '\\':
        out << "\\\\";
        break;
      case '"':
        out << "\\\"";
        break;
      case '\n':
        out << "\\n";
        break;
      case '\r':
        out << "\\r";
        break;
      case '\t':
        out << "\\t";
        break;
      default:
        if( ch < 0x20 )
          out << "\\u" << std::hex << std::setw( 4 ) << std::setfill( '0' ) << static_cast< int >( ch );
        else
          out << static_cast< char >( ch );
    }
  }
  return out.str();
}

std::string bytes_to_hex( const std::string& bytes )
{
  static constexpr char hex[] = "0123456789abcdef";
  std::string out;
  out.reserve( bytes.size() * 2 + 2 );
  out = "0x";
  for( unsigned char ch: bytes )
  {
    out.push_back( hex[ ch >> 4 ] );
    out.push_back( hex[ ch & 0x0f ] );
  }
  return out;
}

std::string multihash_string( const koinos::crypto::multihash& value )
{
  return koinos::util::converter::as< std::string >( value );
}

std::string zero_multihash_string()
{
  return multihash_string( koinos::crypto::multihash::zero( koinos::crypto::multicodec::sha2_256 ) );
}

uint64_t parse_u64( const std::string& name, const std::string& value )
{
  std::size_t parsed = 0;
  uint64_t result = 0;
  try
  {
    result = std::stoull( value, &parsed, 10 );
  }
  catch( const std::exception& e )
  {
    throw std::runtime_error( "invalid " + name + " value '" + value + "': " + e.what() );
  }
  if( parsed != value.size() )
    throw std::runtime_error( "invalid " + name + " value '" + value + "'" );
  return result;
}

std::filesystem::path absolute_normal( const std::filesystem::path& path )
{
  return std::filesystem::absolute( path ).lexically_normal();
}

std::string timestamp()
{
  const auto now = std::time( nullptr );
  std::tm tm{};
#if defined( _WIN32 )
  localtime_s( &tm, &now );
#else
  localtime_r( &now, &tm );
#endif

  std::ostringstream out;
  out << std::put_time( &tm, "%Y-%m-%dT%H:%M:%S%z" );
  return out.str();
}

bool stderr_is_terminal()
{
#if defined( _WIN32 )
  return _isatty( _fileno( stderr ) ) != 0;
#else
  return isatty( STDERR_FILENO ) != 0;
#endif
}

void raise_open_file_limit( rlim_t requested )
{
#if !defined( _WIN32 )
  struct rlimit limit;
  if( getrlimit( RLIMIT_NOFILE, &limit ) != 0 )
    return;

  if( limit.rlim_cur >= requested )
    return;

  limit.rlim_cur = std::min( requested, limit.rlim_max );
  setrlimit( RLIMIT_NOFILE, &limit );
#else
  (void)requested;
#endif
}

std::string format_duration( double seconds )
{
  if( seconds < 0 || seconds != seconds )
    return "unknown";

  const auto total = static_cast< uint64_t >( seconds + 0.5 );
  const auto hours = total / 3600;
  const auto minutes = ( total % 3600 ) / 60;
  const auto secs = total % 60;

  std::ostringstream out;
  if( hours )
    out << hours << "h " << std::setw( 2 ) << std::setfill( '0' ) << minutes << "m";
  else if( minutes )
    out << minutes << "m " << std::setw( 2 ) << std::setfill( '0' ) << secs << "s";
  else
    out << secs << "s";
  return out.str();
}

class AuditLogger
{
public:
  explicit AuditLogger( const std::filesystem::path& path ):
      _path( path )
  {
    const auto parent = path.parent_path();
    if( !parent.empty() )
      std::filesystem::create_directories( parent );

    _stream.open( path, std::ios::out | std::ios::app );
    if( !_stream )
      throw std::runtime_error( "could not open audit log file: " + path.string() );
  }

  const std::filesystem::path& path() const
  {
    return _path;
  }

  void line( const std::string& message )
  {
    _stream << timestamp() << ' ' << message << '\n';
    _stream.flush();
  }

private:
  std::filesystem::path _path;
  std::ofstream _stream;
};

class ProgressReporter
{
public:
  ProgressReporter( std::string phase,
                    uint64_t start_height,
                    uint64_t end_height,
                    uint64_t progress_every,
                    AuditLogger* logger ):
      _phase( std::move( phase ) ),
      _start_height( start_height ),
      _end_height( end_height ),
      _total( end_height >= start_height ? end_height - start_height + 1 : 0 ),
      _progress_every( progress_every ),
      _interactive( progress_every != 0 && stderr_is_terminal() ),
      _started( std::chrono::steady_clock::now() ),
      _last_update( _started ),
      _logger( logger )
  {
    if( _interactive && _total )
      update( start_height - 1, ReplayStats{} );
  }

  ~ProgressReporter()
  {
    if( _interactive && !_finished && _last_width )
      std::cerr << '\n';
  }

  ProgressReporter( const ProgressReporter& ) = delete;
  ProgressReporter& operator=( const ProgressReporter& ) = delete;

  void maybe_update( uint64_t current_height, const ReplayStats& stats )
  {
    if( !_progress_every || !_total )
      return;

    const uint64_t completed = current_height < _start_height ? 0 : std::min< uint64_t >( current_height - _start_height + 1, _total );
    const auto now = std::chrono::steady_clock::now();
    const bool height_interval = completed % _progress_every == 0;
    const bool time_interval = now - _last_update >= std::chrono::seconds( 5 );
    if( current_height != _end_height && !height_interval && !time_interval )
      return;

    update( current_height, stats );
  }

  void finish()
  {
    if( _interactive && _last_width )
    {
      std::cerr << '\n';
      _finished = true;
    }
  }

private:
  void update( uint64_t current_height, const ReplayStats& stats )
  {
    const uint64_t completed = current_height < _start_height ? 0 : std::min< uint64_t >( current_height - _start_height + 1, _total );
    const double ratio = _total ? static_cast< double >( completed ) / static_cast< double >( _total ) : 1.0;

    const auto now = std::chrono::steady_clock::now();
    const auto elapsed = std::chrono::duration< double >( now - _started ).count();
    const double blocks_per_second = elapsed > 0 ? static_cast< double >( completed ) / elapsed : 0.0;
    const double remaining_seconds = blocks_per_second > 0
      ? static_cast< double >( _total - completed ) / blocks_per_second
      : -1.0;

    std::ostringstream log_line;
    log_line << "progress phase=" << _phase << ' '
             << std::fixed << std::setprecision( 1 ) << ( ratio * 100.0 ) << "% "
             << "height=" << current_height
             << " of " << _end_height
             << " blocks_checked=" << stats.blocks_checked
             << " rate=" << blocks_per_second << " blk/s"
             << " eta=" << format_duration( remaining_seconds );
    if( !stats.final_state_merkle_root.empty() )
      log_line << " final_root=" << stats.final_state_merkle_root;

    if( _logger )
      _logger->line( log_line.str() );
    _last_update = now;

    if( _interactive )
    {
      constexpr std::size_t bar_width = 36;
      const auto filled = static_cast< std::size_t >( ratio * static_cast< double >( bar_width ) + 0.5 );

      std::ostringstream line;
      line << "[state_delta_replay_audit] " << _phase << " [";
      for( std::size_t i = 0; i < bar_width; ++i )
        line << ( i < filled ? '#' : '-' );
      line << "] "
           << std::fixed << std::setprecision( 1 ) << ( ratio * 100.0 ) << "% "
           << "height=" << current_height << "/" << _end_height
           << " checked=" << stats.blocks_checked
           << " rate=" << std::setprecision( 1 ) << blocks_per_second << " blk/s"
           << " eta=" << format_duration( remaining_seconds );

      const auto rendered = line.str();
      std::cerr << '\r' << rendered;
      if( rendered.size() < _last_width )
        std::cerr << std::string( _last_width - rendered.size(), ' ' );
      std::cerr.flush();
      _last_width = rendered.size();
      return;
    }

    std::cerr << "[state_delta_replay_audit] " << log_line.str().substr( std::string( "progress " ).size() ) << '\n';
  }

  std::string _phase;
  uint64_t _start_height = 0;
  uint64_t _end_height = 0;
  uint64_t _total = 0;
  uint64_t _progress_every = 0;
  bool _interactive = false;
  bool _finished = false;
  std::chrono::steady_clock::time_point _started;
  std::chrono::steady_clock::time_point _last_update;
  AuditLogger* _logger = nullptr;
  std::size_t _last_width = 0;
};

void usage( const char* argv0 )
{
  std::cerr
    << "Usage:\n"
    << "  " << argv0 << " --source-basedir PATH --scratch-state-dir PATH [options]\n\n"
    << "Options:\n"
    << "  --source-basedir PATH     Restored/copy basedir containing db/ block-store data.\n"
    << "  --source-db PATH          Source unified RocksDB path. Default: SOURCE_BASEDIR/db.\n"
    << "  --scratch-state-dir PATH  Scratch/log directory. Used as a state DB only\n"
    << "                            with --state-db-replay.\n"
    << "  --genesis PATH            Genesis JSON. Default: SOURCE_BASEDIR/chain/genesis_data.json,\n"
    << "                            then SOURCE_BASEDIR/genesis_data.json.\n"
    << "  --from-height N           Start height. Default: 1, or scratch head + 1\n"
    << "                            with --state-db-replay.\n"
    << "  --to-height N             Stop height. Default: source block-store head.\n"
    << "  --batch-size N            Accepted for compatibility; the height-indexed\n"
    << "                            replay path no longer uses block-store batches.\n"
    << "  --progress-every N        Progress interval on stderr. Renders a progress bar\n"
    << "                            when stderr is a terminal. 0 disables progress.\n"
    << "  --log-file PATH           Append audit log records to PATH. Default:\n"
    << "                            SCRATCH_STATE_DIR/state-delta-replay-audit.log.\n"
    << "  --no-log-file             Disable audit log file output.\n"
    << "  --journal-dir PATH        Local bucketed flat-file header+receipt journal.\n"
    << "                            Default: SCRATCH_STATE_DIR.delta-journal.\n"
    << "  --rebuild-journal         Force rebuilding the local replay journal.\n"
    << "  --height-index-dir PATH   Local height->block_id index. Default:\n"
    << "                            SCRATCH_STATE_DIR.height-index. Used by\n"
    << "                            --state-db-replay only.\n"
    << "  --rebuild-height-index    Force rebuilding the local height index.\n"
    << "  --state-db-replay         Use the legacy height-index source DB replay path\n"
    << "                            instead of the default flat-journal replay path.\n"
    << "  --sync-scratch-writes     Fsync every scratch state commit. Slower; default\n"
    << "                            uses async writes because scratch is rebuildable.\n"
    << "  --reset-scratch           Delete scratch directory before replay.\n"
    << "  --normal-removes          Replay remove entries with normal remove semantics.\n"
    << "                            This is useful for reproducing the old bug.\n"
    << "  --json                    Print final result as JSON.\n"
    << "  -h, --help                Show this help.\n\n"
    << "The source RocksDB is opened read-only. The tool does not start P2P, JSON-RPC,\n"
    << "gRPC, mempool, producer, or any other node service.\n";
}

Args parse_args( int argc, char** argv )
{
  Args args;

  for( int i = 1; i < argc; ++i )
  {
    const std::string arg = argv[ i ];
    auto require_value = [&]( const std::string& name ) -> std::string
    {
      if( i + 1 >= argc )
        throw std::runtime_error( name + " requires a value" );
      return argv[ ++i ];
    };

    if( arg == "-h" || arg == "--help" )
      args.help = true;
    else if( arg == "--source-basedir" || arg == "--basedir" )
      args.source_basedir = require_value( arg );
    else if( arg == "--source-db" )
      args.source_db = require_value( arg );
    else if( arg == "--scratch-state-dir" )
      args.scratch_state_dir = require_value( arg );
    else if( arg == "--genesis" )
      args.genesis_file = require_value( arg );
    else if( arg == "--from-height" )
      args.from_height = parse_u64( arg, require_value( arg ) );
    else if( arg == "--to-height" )
      args.to_height = parse_u64( arg, require_value( arg ) );
    else if( arg == "--batch-size" )
    {
      const auto parsed = parse_u64( arg, require_value( arg ) );
      if( parsed == 0 || parsed > koinos::node::block_store::BlockStore::max_block_request )
        throw std::runtime_error( "--batch-size must be between 1 and 1000" );
      args.batch_size = static_cast< uint32_t >( parsed );
    }
    else if( arg == "--progress-every" )
      args.progress_every = parse_u64( arg, require_value( arg ) );
    else if( arg == "--log-file" )
      args.log_file = require_value( arg );
    else if( arg == "--no-log-file" )
      args.no_log_file = true;
    else if( arg == "--journal-dir" )
      args.journal_dir = require_value( arg );
    else if( arg == "--rebuild-journal" )
      args.rebuild_journal = true;
    else if( arg == "--height-index-dir" )
      args.height_index_dir = require_value( arg );
    else if( arg == "--rebuild-height-index" )
      args.rebuild_height_index = true;
    else if( arg == "--state-db-replay" )
      args.state_db_replay = true;
    else if( arg == "--sync-scratch-writes" )
      args.sync_scratch_writes = true;
    else if( arg == "--reset-scratch" )
      args.reset_scratch = true;
    else if( arg == "--json" )
      args.json = true;
    else if( arg == "--normal-removes" )
      args.normal_removes = true;
    else
      throw std::runtime_error( "unknown option: " + arg );
  }

  if( args.help )
    return args;

  if( args.source_basedir.empty() && args.source_db.empty() )
    throw std::runtime_error( "--source-basedir is required unless --source-db is provided" );
  if( args.scratch_state_dir.empty() )
    throw std::runtime_error( "--scratch-state-dir is required" );
  if( args.no_log_file && !args.log_file.empty() )
    throw std::runtime_error( "--log-file cannot be used together with --no-log-file" );

  if( !args.source_basedir.empty() )
    args.source_basedir = absolute_normal( args.source_basedir );
  if( args.source_db.empty() )
    args.source_db = args.source_basedir / "db";
  args.source_db = absolute_normal( args.source_db );
  args.scratch_state_dir = absolute_normal( args.scratch_state_dir );
  if( !args.genesis_file.empty() )
    args.genesis_file = absolute_normal( args.genesis_file );
  if( !args.no_log_file )
  {
    if( args.log_file.empty() )
      args.log_file = args.scratch_state_dir / "state-delta-replay-audit.log";
    args.log_file = absolute_normal( args.log_file );
  }
  if( args.height_index_dir.empty() )
    args.height_index_dir = std::filesystem::path( args.scratch_state_dir.string() + ".height-index" );
  args.height_index_dir = absolute_normal( args.height_index_dir );
  if( args.journal_dir.empty() )
    args.journal_dir = std::filesystem::path( args.scratch_state_dir.string() + ".delta-journal" );
  args.journal_dir = absolute_normal( args.journal_dir );

  return args;
}

std::string height_index_key( uint64_t height )
{
  std::string key;
  key.resize( 9 );
  key[ 0 ] = 'b';
  for( int i = 7; i >= 0; --i )
  {
    key[ 8 - i ] = static_cast< char >( ( height >> ( i * 8 ) ) & 0xff );
  }
  return key;
}

std::string meta_key( const std::string& name )
{
  return "m:" + name;
}

void append_u64( std::string& out, uint64_t value )
{
  for( int i = 7; i >= 0; --i )
    out.push_back( static_cast< char >( ( value >> ( i * 8 ) ) & 0xff ) );
}

uint64_t read_u64( const std::string& value, std::size_t& offset )
{
  if( value.size() - offset < 8 )
    throw std::runtime_error( "truncated journal record" );

  uint64_t result = 0;
  for( int i = 0; i < 8; ++i )
  {
    result <<= 8;
    result |= static_cast< unsigned char >( value[ offset++ ] );
  }
  return result;
}

void append_field( std::string& out, const std::string& value )
{
  append_u64( out, value.size() );
  out.append( value );
}

std::string read_field( const std::string& value, std::size_t& offset )
{
  const auto size = read_u64( value, offset );
  if( size > value.size() - offset )
    throw std::runtime_error( "truncated journal field" );

  std::string result( value.data() + offset, static_cast< std::size_t >( size ) );
  offset += static_cast< std::size_t >( size );
  return result;
}

void write_u64_stream( std::ostream& out, uint64_t value )
{
  char bytes[ 8 ];
  for( int i = 7; i >= 0; --i )
    bytes[ 7 - i ] = static_cast< char >( ( value >> ( i * 8 ) ) & 0xff );
  out.write( bytes, sizeof( bytes ) );
}

uint64_t read_u64_stream( std::istream& in, const std::string& context )
{
  char bytes[ 8 ];
  in.read( bytes, sizeof( bytes ) );
  if( in.gcount() != static_cast< std::streamsize >( sizeof( bytes ) ) )
    throw std::runtime_error( "truncated " + context );

  uint64_t result = 0;
  for( unsigned char ch: bytes )
  {
    result <<= 8;
    result |= ch;
  }
  return result;
}

class ReadOnlyUnifiedDB
{
public:
  ReadOnlyUnifiedDB() = default;
  ~ReadOnlyUnifiedDB()
  {
    close();
  }

  ReadOnlyUnifiedDB( const ReadOnlyUnifiedDB& ) = delete;
  ReadOnlyUnifiedDB& operator=( const ReadOnlyUnifiedDB& ) = delete;

  void open( const std::filesystem::path& path )
  {
    close();
    if( !std::filesystem::exists( path / "CURRENT" ) )
      throw std::runtime_error( "source RocksDB does not exist: " + path.string() );

    rocksdb::Options options;
    options.create_if_missing = false;
    options.create_missing_column_families = false;

    std::vector< rocksdb::ColumnFamilyDescriptor > descriptors;
    for( std::size_t i = 0; i <= static_cast< std::size_t >( ColumnFamily::storage_metadata ); ++i )
    {
      descriptors.emplace_back(
        koinos::node::storage::column_family_name( static_cast< ColumnFamily >( i ) ),
        rocksdb::ColumnFamilyOptions() );
    }

    rocksdb::DB* raw = nullptr;
    auto status = rocksdb::DB::OpenForReadOnly( options, path.string(), descriptors, &_handles, &raw );
    if( !status.ok() )
      throw std::runtime_error( "failed to open source RocksDB read-only at " + path.string() + ": "
                                + status.ToString() );
    _db.reset( raw );
  }

  void close()
  {
    for( auto* handle: _handles )
      delete handle;
    _handles.clear();
    _db.reset();
  }

  rocksdb::DB* db() const
  {
    return _db.get();
  }

  rocksdb::ColumnFamilyHandle* handle( ColumnFamily cf ) const
  {
    const auto index = static_cast< std::size_t >( cf );
    if( index >= _handles.size() )
      throw std::out_of_range( "column family handle index out of range" );
    return _handles[ index ];
  }

private:
  std::unique_ptr< rocksdb::DB > _db;
  std::vector< rocksdb::ColumnFamilyHandle* > _handles;
};

class HeightRecordIndex
{
public:
  HeightRecordIndex() = default;
  ~HeightRecordIndex()
  {
    close();
  }

  HeightRecordIndex( const HeightRecordIndex& ) = delete;
  HeightRecordIndex& operator=( const HeightRecordIndex& ) = delete;

  void open( const std::filesystem::path& path )
  {
    close();
    _path = path;
    std::filesystem::create_directories( path );

    rocksdb::Options options;
    options.create_if_missing = true;
    options.IncreaseParallelism();
    options.OptimizeLevelStyleCompaction();

    rocksdb::DB* raw = nullptr;
    auto status = rocksdb::DB::Open( options, path.string(), &raw );
    if( !status.ok() )
      throw std::runtime_error( "failed to open height index RocksDB at " + path.string() + ": " + status.ToString() );
    _db.reset( raw );
  }

  void close()
  {
    _db.reset();
  }

  bool valid_for( const Args& args,
                  uint64_t source_head_height,
                  const std::string& source_head_id,
                  uint64_t required_height ) const
  {
    const auto indexed_height = parse_u64_meta( "indexed_height" );
    return get_meta( "format" ) == "state-delta-replay-height-index-v2"
           && get_meta( "source_db" ) == args.source_db.string()
           && get_meta( "source_head_height" ) == std::to_string( source_head_height )
           && get_meta( "source_head_id" ) == bytes_to_hex( source_head_id )
           && indexed_height >= required_height;
  }

  void reset()
  {
    close();
    if( !_path.empty() && std::filesystem::exists( _path ) )
      std::filesystem::remove_all( _path );
    open( _path );
  }

  void build( const Args& args,
              rocksdb::DB* source_db,
              rocksdb::ColumnFamilyHandle* source_blocks,
              uint64_t source_head_height,
              const std::string& source_head_id,
              uint64_t required_height,
              AuditLogger* logger )
  {
    reset();
    if( logger )
      logger->line( "height_index_build_start dir=" + _path.string() );

    ProgressReporter progress( "index", 1, required_height, args.progress_every, logger );

    rocksdb::ReadOptions read_options;
    read_options.fill_cache = false;

    std::unique_ptr< rocksdb::Iterator > it( source_db->NewIterator( read_options, source_blocks ) );
    rocksdb::WriteOptions write_options;
    rocksdb::WriteBatch batch;

    ReplayStats progress_stats;
    uint64_t scanned = 0;
    uint64_t indexed = 0;
    std::size_t batch_bytes = 0;
    std::vector< bool > indexed_heights( required_height + 1, false );

    auto flush_batch = [&]()
    {
      if( batch.Count() == 0 )
        return;
      auto status = _db->Write( write_options, &batch );
      if( !status.ok() )
        throw std::runtime_error( "failed to write height index batch: " + status.ToString() );
      batch.Clear();
      batch_bytes = 0;
    };

    for( it->SeekToFirst(); it->Valid(); it->Next() )
    {
      ++scanned;
      koinos::block_store::block_record record;
      if( !record.ParseFromArray( it->value().data(), static_cast< int >( it->value().size() ) ) )
        throw std::runtime_error( "failed to parse block record while building height index" );

      const auto height = record.block_height();
      if( height == 0 || height > required_height )
        continue;
      if( indexed_heights[ height ] )
        continue;

      const auto index_key = height_index_key( height );
      const auto block_id = record.block_id().empty() ? it->key().ToString() : record.block_id();
      batch.Put( index_key, block_id );
      batch_bytes += index_key.size() + block_id.size() + 16;
      indexed_heights[ height ] = true;
      ++indexed;

      if( batch.Count() >= 10'000 || batch_bytes >= 64 * 1024 * 1024 )
        flush_batch();

      progress_stats.blocks_checked = indexed;
      progress.maybe_update( std::min( indexed, required_height ), progress_stats );

      if( required_height < source_head_height && indexed >= required_height )
        break;
    }

    if( !it->status().ok() )
      throw std::runtime_error( "height index source scan failed: " + it->status().ToString() );

    batch.Put( meta_key( "format" ), "state-delta-replay-height-index-v2" );
    batch.Put( meta_key( "source_db" ), args.source_db.string() );
    batch.Put( meta_key( "source_head_height" ), std::to_string( source_head_height ) );
    batch.Put( meta_key( "source_head_id" ), bytes_to_hex( source_head_id ) );
    batch.Put( meta_key( "indexed_height" ), std::to_string( required_height ) );
    batch.Put( meta_key( "indexed_records" ), std::to_string( indexed ) );
    flush_batch();
    progress.finish();

    if( indexed < required_height )
    {
      throw std::runtime_error( "height index incomplete: indexed " + std::to_string( indexed )
                                + " records for required height " + std::to_string( required_height ) );
    }

    if( logger )
      logger->line( "height_index_build_complete scanned_records=" + std::to_string( scanned )
                    + " indexed_records=" + std::to_string( indexed ) );
  }

  std::string get_block_id( uint64_t height ) const
  {
    std::string value;
    auto status = _db->Get( rocksdb::ReadOptions(), height_index_key( height ), &value );
    if( status.IsNotFound() )
      return {};
    if( !status.ok() )
      throw std::runtime_error( "height index get failed at height " + std::to_string( height ) + ": "
                                + status.ToString() );
    return value;
  }

private:
  std::string get_meta( const std::string& name ) const
  {
    std::string value;
    auto status = _db->Get( rocksdb::ReadOptions(), meta_key( name ), &value );
    if( status.IsNotFound() )
      return {};
    if( !status.ok() )
      throw std::runtime_error( "height index metadata get failed: " + status.ToString() );
    return value;
  }

  uint64_t parse_u64_meta( const std::string& name ) const
  {
    const auto value = get_meta( name );
    if( value.empty() )
      return 0;
    return parse_u64( "height index metadata " + name, value );
  }

  std::filesystem::path _path;
  std::unique_ptr< rocksdb::DB > _db;
};

struct JournalRecord
{
  std::string block_id;
  koinos::protocol::block_header header;
  koinos::protocol::block_receipt receipt;
};

std::string encode_journal_record( const JournalRecord& record )
{
  std::string header_bytes;
  if( !record.header.SerializeToString( &header_bytes ) )
    throw std::runtime_error( "failed to serialize journal block header" );

  std::string receipt_bytes;
  if( !record.receipt.SerializeToString( &receipt_bytes ) )
    throw std::runtime_error( "failed to serialize journal receipt" );

  std::string out = "SDRJ1";
  append_field( out, record.block_id );
  append_field( out, header_bytes );
  append_field( out, receipt_bytes );
  return out;
}

JournalRecord decode_journal_record( const std::string& value )
{
  if( value.size() < 5 || value.compare( 0, 5, "SDRJ1" ) != 0 )
    throw std::runtime_error( "invalid journal record magic" );

  std::size_t offset = 5;
  JournalRecord record;
  record.block_id = read_field( value, offset );

  const auto header_bytes = read_field( value, offset );
  if( !record.header.ParseFromString( header_bytes ) )
    throw std::runtime_error( "failed to parse journal block header" );

  const auto receipt_bytes = read_field( value, offset );
  if( !record.receipt.ParseFromString( receipt_bytes ) )
    throw std::runtime_error( "failed to parse journal receipt" );

  if( offset != value.size() )
    throw std::runtime_error( "journal record has trailing bytes" );

  return record;
}

class ReplayJournal
{
public:
  ReplayJournal() = default;
  ~ReplayJournal()
  {
    close();
  }

  ReplayJournal( const ReplayJournal& ) = delete;
  ReplayJournal& operator=( const ReplayJournal& ) = delete;

  void open( const std::filesystem::path& path )
  {
    close();
    _path = path;
    std::filesystem::create_directories( path );
  }

  void close()
  {
    _bucket_records.clear();
    _loaded_bucket = std::numeric_limits< uint64_t >::max();
    _indexed_height = 0;
    _bucket_size = _default_bucket_size;
    _bucket_count = 0;
  }

  bool valid_for( const Args& args,
                  uint64_t source_head_height,
                  const std::string& source_head_id,
                  uint64_t required_height ) const
  {
    try
    {
      const auto metadata = read_metadata();
      const auto indexed_height = parse_u64(
        "replay journal metadata indexed_height",
        get_meta( metadata, "indexed_height" ) );
      return get_meta( metadata, "format" ) == _format
             && get_meta( metadata, "source_db" ) == args.source_db.string()
             && get_meta( metadata, "source_head_height" ) == std::to_string( source_head_height )
             && get_meta( metadata, "source_head_id" ) == bytes_to_hex( source_head_id )
             && indexed_height >= required_height
             && bucket_files_exist( metadata );
    }
    catch( const std::exception& )
    {
      return false;
    }
  }

  bool upgrade_flat_if_valid( const Args& args,
                              uint64_t source_head_height,
                              const std::string& source_head_id,
                              uint64_t required_height,
                              AuditLogger* logger )
  {
    Metadata metadata;
    try
    {
      metadata = read_metadata();
      const auto indexed_height = parse_u64(
        "replay journal metadata indexed_height",
        get_meta( metadata, "indexed_height" ) );
      if( get_meta( metadata, "format" ) != _flat_format
          || get_meta( metadata, "source_db" ) != args.source_db.string()
          || get_meta( metadata, "source_head_height" ) != std::to_string( source_head_height )
          || get_meta( metadata, "source_head_id" ) != bytes_to_hex( source_head_id )
          || indexed_height < required_height
          || !std::filesystem::exists( flat_records_path() ) )
      {
        return false;
      }
    }
    catch( const std::exception& )
    {
      return false;
    }

    if( logger )
      logger->line( "journal_upgrade_start from=flat-file to=bucketed-flat-file dir=" + _path.string() );

    raise_open_file_limit( 2048 );
    _bucket_size = _default_bucket_size;
    _bucket_count = required_height == 0 ? 0 : ( ( required_height - 1 ) / _bucket_size ) + 1;

    for( const auto& entry: std::filesystem::directory_iterator( _path ) )
    {
      if( entry.path().filename().string().rfind( "bucket-", 0 ) == 0 )
        std::filesystem::remove( entry.path() );
    }

    std::vector< std::ofstream > buckets( static_cast< std::size_t >( _bucket_count ) );
    for( uint64_t bucket = 0; bucket < _bucket_count; ++bucket )
    {
      buckets[ static_cast< std::size_t >( bucket ) ].open(
        bucket_path( bucket ),
        std::ios::binary | std::ios::out | std::ios::trunc );
      if( !buckets[ static_cast< std::size_t >( bucket ) ] )
        throw std::runtime_error( "could not create replay journal bucket: " + bucket_path( bucket ).string() );
      buckets[ static_cast< std::size_t >( bucket ) ].write(
        _bucket_magic,
        static_cast< std::streamsize >( std::strlen( _bucket_magic ) ) );
    }

    std::ifstream input( flat_records_path(), std::ios::binary | std::ios::in );
    if( !input )
      throw std::runtime_error( "could not open flat replay journal records: " + flat_records_path().string() );

    std::string magic( std::strlen( _flat_records_magic ), '\0' );
    input.read( magic.data(), static_cast< std::streamsize >( magic.size() ) );
    if( magic != _flat_records_magic )
      throw std::runtime_error( "invalid flat replay journal records magic" );

    ReplayStats progress_stats;
    ProgressReporter progress( "journal-upgrade", 1, required_height, args.progress_every, logger );
    std::vector< bool > indexed_heights( required_height + 1, false );
    uint64_t scanned = 0;
    uint64_t stored = 0;
    uint64_t unique_heights = 0;

    while( input.peek() != std::char_traits< char >::eof() )
    {
      std::string record_magic( std::strlen( _record_magic ), '\0' );
      input.read( record_magic.data(), static_cast< std::streamsize >( record_magic.size() ) );
      if( input.gcount() == 0 )
        break;
      if( input.gcount() != static_cast< std::streamsize >( record_magic.size() ) || record_magic != _record_magic )
        throw std::runtime_error( "invalid flat replay journal record magic during upgrade" );

      const auto height = read_u64_stream( input, "flat replay journal record height" );
      (void)read_u64_stream( input, "flat replay journal record next offset" );
      const auto payload_size = read_u64_stream( input, "flat replay journal record size" );
      std::string payload( static_cast< std::size_t >( payload_size ), '\0' );
      if( payload_size )
      {
        input.read( payload.data(), static_cast< std::streamsize >( payload.size() ) );
        if( input.gcount() != static_cast< std::streamsize >( payload.size() ) )
          throw std::runtime_error( "truncated flat replay journal record during upgrade" );
      }

      ++scanned;
      if( height == 0 || height > required_height )
        continue;

      auto& bucket = buckets[ static_cast< std::size_t >( bucket_for_height( height ) ) ];
      bucket.write( _record_magic, static_cast< std::streamsize >( std::strlen( _record_magic ) ) );
      write_u64_stream( bucket, height );
      write_u64_stream( bucket, payload.size() );
      bucket.write( payload.data(), static_cast< std::streamsize >( payload.size() ) );
      if( !bucket )
        throw std::runtime_error( "failed to write upgraded replay journal record at height "
                                  + std::to_string( height ) );
      ++stored;

      if( !indexed_heights[ height ] )
      {
        indexed_heights[ height ] = true;
        ++unique_heights;
        progress_stats.blocks_checked = unique_heights;
        progress.maybe_update( unique_heights, progress_stats );
      }

      if( required_height < source_head_height && unique_heights >= required_height )
        break;
    }

    for( uint64_t bucket = 0; bucket < _bucket_count; ++bucket )
    {
      auto& stream = buckets[ static_cast< std::size_t >( bucket ) ];
      stream.close();
      if( !stream )
        throw std::runtime_error( "failed to close replay journal bucket: " + bucket_path( bucket ).string() );
    }

    write_metadata( {
      { "format", _format },
      { "source_db", args.source_db.string() },
      { "source_head_height", std::to_string( source_head_height ) },
      { "source_head_id", bytes_to_hex( source_head_id ) },
      { "indexed_height", std::to_string( required_height ) },
      { "bucket_size", std::to_string( _bucket_size ) },
      { "bucket_count", std::to_string( _bucket_count ) },
      { "stored_records", std::to_string( stored ) },
      { "unique_heights", std::to_string( unique_heights ) },
      { "upgraded_from", _flat_format },
    } );

    _indexed_height = required_height;
    _loaded_bucket = std::numeric_limits< uint64_t >::max();
    progress.finish();

    if( unique_heights < required_height )
    {
      throw std::runtime_error( "replay journal upgrade incomplete: indexed " + std::to_string( unique_heights )
                                + " heights for required height " + std::to_string( required_height ) );
    }

    if( logger )
      logger->line( "journal_upgrade_complete scanned_records=" + std::to_string( scanned )
                    + " stored_records=" + std::to_string( stored )
                    + " unique_heights=" + std::to_string( unique_heights ) );
    return true;
  }

  void reset()
  {
    close();
    if( !_path.empty() && std::filesystem::exists( _path ) )
      std::filesystem::remove_all( _path );
    open( _path );
  }

  void build( const Args& args,
              rocksdb::DB* source_db,
              rocksdb::ColumnFamilyHandle* source_blocks,
              uint64_t source_head_height,
              const std::string& source_head_id,
              uint64_t required_height,
              AuditLogger* logger )
  {
    reset();
    raise_open_file_limit( 2048 );
    if( logger )
      logger->line( "journal_build_start format=bucketed-flat-file dir=" + _path.string() );

    ProgressReporter progress( "journal", 1, required_height, args.progress_every, logger );

    rocksdb::ReadOptions read_options;
    read_options.fill_cache = false;

    std::unique_ptr< rocksdb::Iterator > it( source_db->NewIterator( read_options, source_blocks ) );

    ReplayStats progress_stats;
    std::vector< bool > indexed_heights( required_height + 1, false );
    uint64_t scanned = 0;
    uint64_t stored = 0;
    uint64_t unique_heights = 0;

    _bucket_size = _default_bucket_size;
    _bucket_count = required_height == 0 ? 0 : ( ( required_height - 1 ) / _bucket_size ) + 1;
    std::vector< std::ofstream > buckets( static_cast< std::size_t >( _bucket_count ) );
    for( uint64_t bucket = 0; bucket < _bucket_count; ++bucket )
    {
      buckets[ static_cast< std::size_t >( bucket ) ].open(
        bucket_path( bucket ),
        std::ios::binary | std::ios::out | std::ios::trunc );
      if( !buckets[ static_cast< std::size_t >( bucket ) ] )
        throw std::runtime_error( "could not create replay journal bucket: " + bucket_path( bucket ).string() );
      buckets[ static_cast< std::size_t >( bucket ) ].write(
        _bucket_magic,
        static_cast< std::streamsize >( std::strlen( _bucket_magic ) ) );
    }

    for( it->SeekToFirst(); it->Valid(); it->Next() )
    {
      ++scanned;
      koinos::block_store::block_record record;
      if( !record.ParseFromArray( it->value().data(), static_cast< int >( it->value().size() ) ) )
        throw std::runtime_error( "failed to parse block record while building replay journal" );

      const auto height = record.block_height();
      if( height == 0 || height > required_height )
        continue;
      if( !record.has_block() || !record.block().has_header() || !record.has_receipt() )
        continue;

      JournalRecord journal_record;
      journal_record.block_id = record.block_id().empty() ? record.block().id() : record.block_id();
      if( journal_record.block_id.empty() )
        journal_record.block_id = it->key().ToString();
      journal_record.header = record.block().header();
      journal_record.receipt = record.receipt();

      if( journal_record.header.height() != height )
        throw std::runtime_error( "journal block height mismatch while building at height " + std::to_string( height ) );

      const auto value = encode_journal_record( journal_record );
      auto& bucket = buckets[ static_cast< std::size_t >( bucket_for_height( height ) ) ];
      bucket.write( _record_magic, static_cast< std::streamsize >( std::strlen( _record_magic ) ) );
      write_u64_stream( bucket, height );
      write_u64_stream( bucket, value.size() );
      bucket.write( value.data(), static_cast< std::streamsize >( value.size() ) );
      if( !bucket )
        throw std::runtime_error( "failed to write replay journal record at height " + std::to_string( height ) );

      ++stored;

      if( !indexed_heights[ height ] )
      {
        indexed_heights[ height ] = true;
        ++unique_heights;
        progress_stats.blocks_checked = unique_heights;
        progress.maybe_update( unique_heights, progress_stats );
      }

      if( required_height < source_head_height && unique_heights >= required_height )
        break;
    }

    if( !it->status().ok() )
      throw std::runtime_error( "replay journal source scan failed: " + it->status().ToString() );

    for( uint64_t bucket = 0; bucket < _bucket_count; ++bucket )
    {
      auto& stream = buckets[ static_cast< std::size_t >( bucket ) ];
      stream.close();
      if( !stream )
        throw std::runtime_error( "failed to close replay journal bucket: " + bucket_path( bucket ).string() );
    }

    write_metadata( {
      { "format", _format },
      { "source_db", args.source_db.string() },
      { "source_head_height", std::to_string( source_head_height ) },
      { "source_head_id", bytes_to_hex( source_head_id ) },
      { "indexed_height", std::to_string( required_height ) },
      { "bucket_size", std::to_string( _bucket_size ) },
      { "bucket_count", std::to_string( _bucket_count ) },
      { "stored_records", std::to_string( stored ) },
      { "unique_heights", std::to_string( unique_heights ) },
    } );

    _indexed_height = required_height;
    _loaded_bucket = std::numeric_limits< uint64_t >::max();
    progress.finish();

    if( unique_heights < required_height )
    {
      throw std::runtime_error( "replay journal incomplete: indexed " + std::to_string( unique_heights )
                                + " heights for required height " + std::to_string( required_height ) );
    }

    if( logger )
      logger->line( "journal_build_complete scanned_records=" + std::to_string( scanned )
                    + " stored_records=" + std::to_string( stored )
                    + " unique_heights=" + std::to_string( unique_heights ) );
  }

  std::vector< JournalRecord > records_at_height( uint64_t height )
  {
    ensure_ready_for_read();
    if( height == 0 || height > _indexed_height )
      return {};

    const auto bucket = bucket_for_height( height );
    if( bucket != _loaded_bucket )
      load_bucket( bucket );

    const auto index = static_cast< std::size_t >( height - bucket_start_height( bucket ) );
    if( index >= _bucket_records.size() )
      return {};
    return _bucket_records[ index ];
  }

private:
  using Metadata = std::vector< std::pair< std::string, std::string > >;

  std::filesystem::path metadata_path() const
  {
    return _path / "metadata.txt";
  }

  std::filesystem::path bucket_path( uint64_t bucket ) const
  {
    std::ostringstream name;
    name << "bucket-" << std::setw( 6 ) << std::setfill( '0' ) << bucket << ".bin";
    return _path / name.str();
  }

  std::filesystem::path flat_records_path() const
  {
    return _path / "records.bin";
  }

  static std::string get_meta( const Metadata& metadata, const std::string& name )
  {
    for( const auto& item: metadata )
    {
      if( item.first == name )
        return item.second;
    }
    return {};
  }

  Metadata read_metadata() const
  {
    std::ifstream input( metadata_path() );
    if( !input )
      throw std::runtime_error( "could not open replay journal metadata: " + metadata_path().string() );

    Metadata metadata;
    std::string line;
    while( std::getline( input, line ) )
    {
      if( line.empty() )
        continue;
      const auto separator = line.find( '=' );
      if( separator == std::string::npos )
        throw std::runtime_error( "invalid replay journal metadata line: " + line );
      metadata.emplace_back( line.substr( 0, separator ), line.substr( separator + 1 ) );
    }
    return metadata;
  }

  void write_metadata( const Metadata& metadata ) const
  {
    const auto tmp_path = metadata_path().string() + ".tmp";
    std::ofstream output( tmp_path, std::ios::out | std::ios::trunc );
    if( !output )
      throw std::runtime_error( "could not create replay journal metadata: " + tmp_path );
    for( const auto& item: metadata )
      output << item.first << '=' << item.second << '\n';
    output.close();
    if( !output )
      throw std::runtime_error( "failed to write replay journal metadata: " + tmp_path );
    std::filesystem::rename( tmp_path, metadata_path() );
  }

  bool bucket_files_exist( const Metadata& metadata ) const
  {
    const auto count = parse_u64( "replay journal metadata bucket_count", get_meta( metadata, "bucket_count" ) );
    for( uint64_t bucket = 0; bucket < count; ++bucket )
    {
      if( !std::filesystem::exists( bucket_path( bucket ) ) )
        return false;
    }
    return true;
  }

  void ensure_ready_for_read()
  {
    if( _indexed_height )
      return;

    const auto metadata = read_metadata();
    if( get_meta( metadata, "format" ) != _format )
      throw std::runtime_error( "unsupported replay journal format: " + get_meta( metadata, "format" ) );

    _indexed_height = parse_u64( "replay journal metadata indexed_height", get_meta( metadata, "indexed_height" ) );
    _bucket_size = parse_u64( "replay journal metadata bucket_size", get_meta( metadata, "bucket_size" ) );
    _bucket_count = parse_u64( "replay journal metadata bucket_count", get_meta( metadata, "bucket_count" ) );
  }

  uint64_t bucket_for_height( uint64_t height ) const
  {
    return ( height - 1 ) / _bucket_size;
  }

  uint64_t bucket_start_height( uint64_t bucket ) const
  {
    return bucket * _bucket_size + 1;
  }

  void load_bucket( uint64_t bucket )
  {
    if( bucket >= _bucket_count )
      throw std::runtime_error( "replay journal bucket out of range: " + std::to_string( bucket ) );

    const auto start_height = bucket_start_height( bucket );
    const auto end_height = std::min< uint64_t >( _indexed_height, start_height + _bucket_size - 1 );
    _bucket_records.clear();
    _bucket_records.resize( static_cast< std::size_t >( end_height - start_height + 1 ) );

    std::ifstream input( bucket_path( bucket ), std::ios::binary | std::ios::in );
    if( !input )
      throw std::runtime_error( "could not open replay journal bucket: " + bucket_path( bucket ).string() );

    std::string magic( std::strlen( _bucket_magic ), '\0' );
    input.read( magic.data(), static_cast< std::streamsize >( magic.size() ) );
    if( magic != _bucket_magic )
      throw std::runtime_error( "invalid replay journal bucket magic: " + bucket_path( bucket ).string() );

    while( input.peek() != std::char_traits< char >::eof() )
    {
      std::string record_magic( std::strlen( _record_magic ), '\0' );
      input.read( record_magic.data(), static_cast< std::streamsize >( record_magic.size() ) );
      if( input.gcount() == 0 )
        break;
      if( input.gcount() != static_cast< std::streamsize >( record_magic.size() ) || record_magic != _record_magic )
        throw std::runtime_error( "invalid replay journal record magic in bucket " + std::to_string( bucket ) );

      const auto height = read_u64_stream( input, "replay journal bucket record height" );
      const auto payload_size = read_u64_stream( input, "replay journal bucket record size" );
      std::string payload( static_cast< std::size_t >( payload_size ), '\0' );
      if( payload_size )
      {
        input.read( payload.data(), static_cast< std::streamsize >( payload.size() ) );
        if( input.gcount() != static_cast< std::streamsize >( payload.size() ) )
          throw std::runtime_error( "truncated replay journal bucket record payload at height "
                                    + std::to_string( height ) );
      }
      if( height < start_height || height > end_height )
        throw std::runtime_error( "replay journal bucket contains out-of-range height " + std::to_string( height ) );
      _bucket_records[ static_cast< std::size_t >( height - start_height ) ].emplace_back(
        decode_journal_record( payload ) );
    }

    _loaded_bucket = bucket;
  }

  static constexpr uint64_t _default_bucket_size = 100'000;
  static constexpr const char* _format = "state-delta-replay-bucket-journal-v1";
  static constexpr const char* _flat_format = "state-delta-replay-flat-journal-v1";
  static constexpr const char* _flat_records_magic = "SDJF2";
  static constexpr const char* _bucket_magic = "SDJB3";
  static constexpr const char* _record_magic = "SDJR2";

  std::filesystem::path _path;
  std::vector< std::vector< JournalRecord > > _bucket_records;
  uint64_t _loaded_bucket = std::numeric_limits< uint64_t >::max();
  uint64_t _indexed_height = 0;
  uint64_t _bucket_size = _default_bucket_size;
  uint64_t _bucket_count = 0;
};

std::filesystem::path default_genesis_path( const Args& args )
{
  if( !args.genesis_file.empty() )
    return args.genesis_file;

  if( args.source_basedir.empty() )
    throw std::runtime_error( "--genesis is required when --source-basedir is not provided" );

  auto candidate = args.source_basedir / "chain" / "genesis_data.json";
  if( std::filesystem::exists( candidate ) )
    return candidate;

  candidate = args.source_basedir / "genesis_data.json";
  if( std::filesystem::exists( candidate ) )
    return candidate;

  throw std::runtime_error( "genesis data not found; pass --genesis PATH" );
}

koinos::chain::genesis_data load_genesis( const Args& args )
{
  const auto path = default_genesis_path( args );
  std::ifstream input( path );
  if( !input )
    throw std::runtime_error( "could not open genesis data: " + path.string() );

  const std::string json( ( std::istreambuf_iterator< char >( input ) ), std::istreambuf_iterator< char >() );
  koinos::chain::genesis_data genesis;
  auto status = google::protobuf::util::JsonStringToMessage( json, &genesis );
  if( !status.ok() )
    throw std::runtime_error( "could not parse genesis data " + path.string() + ": " + status.ToString() );
  return genesis;
}

koinos::state_db::genesis_init_function make_genesis_initializer( const koinos::chain::genesis_data& data )
{
  return [ data ]( koinos::state_db::state_node_ptr root )
  {
    for( const auto& entry: data.entries() )
    {
      if( root->get_object( entry.space(), entry.key() ) )
        throw std::runtime_error( "encountered unexpected object in initial state" );
      root->put_object( entry.space(), entry.key(), &entry.value() );
    }

    if( !root->get_object( koinos::chain::state::space::metadata(), koinos::chain::state::key::genesis_key ) )
      throw std::runtime_error( "could not find genesis public key in database" );

    auto chain_id = koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, data );
    auto chain_id_str = koinos::util::converter::as< std::string >( chain_id );
    if( root->get_object( koinos::chain::state::space::metadata(), koinos::chain::state::key::chain_id ) )
      throw std::runtime_error( "encountered unexpected chain id in initial state" );
    root->put_object( koinos::chain::state::space::metadata(), koinos::chain::state::key::chain_id, &chain_id_str );
  };
}

koinos::chain::object_space chain_space( const koinos::protocol::object_space& space )
{
  koinos::chain::object_space result;
  result.set_system( space.system() );
  result.set_zone( space.zone() );
  result.set_id( space.id() );
  return result;
}

std::string database_key_string( const koinos::chain::object_space& space, const std::string& key )
{
  koinos::chain::database_key db_key;
  *db_key.mutable_space() = space;
  db_key.set_key( key );
  return koinos::util::converter::as< std::string >( db_key );
}

std::string compute_delta_entries_merkle_root(
  const google::protobuf::RepeatedPtrField< koinos::protocol::state_delta_entry >& entries )
{
  std::vector< std::pair< std::string, std::string > > merkle_entries;
  merkle_entries.reserve( entries.size() );

  for( const auto& entry: entries )
  {
    merkle_entries.emplace_back(
      database_key_string( chain_space( entry.object_space() ), entry.key() ),
      entry.has_value() ? entry.value() : std::string() );
  }

  std::sort(
    merkle_entries.begin(),
    merkle_entries.end(),
    []( const auto& lhs, const auto& rhs )
    {
      return lhs.first < rhs.first;
    } );

  std::vector< koinos::crypto::multihash > merkle_leafs;
  merkle_leafs.reserve( merkle_entries.size() * 2 );
  for( const auto& [ key, value ]: merkle_entries )
  {
    merkle_leafs.emplace_back( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, key ) );
    merkle_leafs.emplace_back( koinos::crypto::hash( koinos::crypto::multicodec::sha2_256, value ) );
  }

  return multihash_string(
    koinos::crypto::merkle_tree( koinos::crypto::multicodec::sha2_256, merkle_leafs ).root()->hash() );
}

void prepare_scratch_dir( const Args& args )
{
  const auto legacy_chain_dir = args.source_basedir.empty()
    ? std::filesystem::path()
    : absolute_normal( args.source_basedir / "chain" / "blockchain" );

  if( args.scratch_state_dir == args.source_db )
    throw std::runtime_error( "--scratch-state-dir must not be the source unified DB" );
  if( !legacy_chain_dir.empty() && args.scratch_state_dir == legacy_chain_dir )
    throw std::runtime_error( "--scratch-state-dir must not be the source legacy chain state DB" );

  if( args.reset_scratch && std::filesystem::exists( args.scratch_state_dir ) )
    std::filesystem::remove_all( args.scratch_state_dir );

  std::filesystem::create_directories( args.scratch_state_dir );
}

void apply_block_receipt( koinos::state_db::database& replay_db,
                          const koinos::protocol::block& block,
                          const koinos::protocol::block_receipt& receipt,
                          bool normal_removes,
                          ReplayStats& stats )
{
  if( !block.has_header() )
    throw std::runtime_error( "block is missing header" );
  if( block.id().empty() )
    throw std::runtime_error( "block at height " + std::to_string( block.header().height() ) + " is missing id" );
  if( receipt.height() != 0 && receipt.height() != block.header().height() )
    throw std::runtime_error( "receipt height mismatch at block " + std::to_string( block.header().height() ) );
  if( !receipt.id().empty() && receipt.id() != block.id() )
    throw std::runtime_error( "receipt id mismatch at block " + std::to_string( block.header().height() ) );

  const auto block_id = koinos::util::converter::to< koinos::crypto::multihash >( block.id() );
  const auto parent_id = koinos::util::converter::to< koinos::crypto::multihash >( block.header().previous() );

  auto lock = replay_db.get_unique_lock();
  auto parent_node = replay_db.get_head( lock );
  if( !parent_node )
    throw std::runtime_error( "scratch state has no head" );
  if( parent_node->id() != parent_id )
  {
    throw std::runtime_error( "scratch head id does not match block parent at height "
                              + std::to_string( block.header().height() )
                              + ": scratch=" + bytes_to_hex( multihash_string( parent_node->id() ) )
                              + " parent=" + bytes_to_hex( block.header().previous() ) );
  }

  const auto parent_root = multihash_string( parent_node->merkle_root() );
  if( block.header().previous_state_merkle_root() != parent_root )
  {
    throw std::runtime_error( "block previous state merkle mismatch at height "
                              + std::to_string( block.header().height() )
                              + ": expected=" + bytes_to_hex( block.header().previous_state_merkle_root() )
                              + " actual=" + bytes_to_hex( parent_root ) );
  }

  if( replay_db.get_node( block_id, lock ) )
    throw std::runtime_error( "scratch state already contains block " + bytes_to_hex( block.id() ) );

  auto block_node = replay_db.create_writable_node( parent_id, block_id, block.header(), lock );
  if( !block_node )
    throw std::runtime_error( "failed to create writable state node at height "
                              + std::to_string( block.header().height() ) );

  for( const auto& delta_entry: receipt.state_delta_entries() )
  {
    const auto space = chain_space( delta_entry.object_space() );
    if( delta_entry.has_value() )
    {
      block_node->put_object( space, delta_entry.key(), &delta_entry.value() );
      ++stats.receipt_puts;
    }
    else
    {
      if( normal_removes )
        block_node->remove_object( space, delta_entry.key() );
      else
        block_node->remove_object_preserve_tombstone( space, delta_entry.key() );
      ++stats.receipt_removes;
    }
    ++stats.receipt_delta_entries;
  }

  const auto computed_root = multihash_string( block_node->pending_merkle_root() );
  if( receipt.state_merkle_root().empty() )
  {
    ++stats.receipts_without_state_root;
  }
  else if( receipt.state_merkle_root() != computed_root )
  {
    throw std::runtime_error( "block receipt state merkle mismatch at height "
                              + std::to_string( block.header().height() )
                              + ": expected=" + bytes_to_hex( receipt.state_merkle_root() )
                              + " actual=" + bytes_to_hex( computed_root ) );
  }

  block_node.reset();
  parent_node.reset();

  replay_db.finalize_node( block_id, lock );
  replay_db.commit_node( block_id, lock );

  stats.final_height = block.header().height();
  stats.final_block_id = bytes_to_hex( block.id() );
  stats.final_state_merkle_root = bytes_to_hex( computed_root );
  ++stats.blocks_checked;
}

ReplayStats run_replay( const Args& args )
{
  std::unique_ptr< AuditLogger > logger;
  try
  {
    prepare_scratch_dir( args );

    if( !args.log_file.empty() )
    {
      logger = std::make_unique< AuditLogger >( args.log_file );
      logger->line( "run_start" );
      logger->line( "source_db=" + args.source_db.string() );
      logger->line( "scratch_state_dir=" + args.scratch_state_dir.string() );
      logger->line( "height_index_dir=" + args.height_index_dir.string() );
      logger->line( "remove_mode=" + std::string( args.normal_removes ? "normal" : "preserve-tombstone" ) );
      logger->line( "reset_scratch=" + std::string( args.reset_scratch ? "true" : "false" ) );
      logger->line( "scratch_write_mode=" + std::string( args.sync_scratch_writes ? "sync" : "async" ) );
    }

    ReadOnlyUnifiedDB source_db;
    source_db.open( args.source_db );
    koinos::node::block_store::BlockStore block_store(
      source_db.db(),
      source_db.handle( ColumnFamily::blocks ),
      source_db.handle( ColumnFamily::block_meta ) );

    const auto genesis = load_genesis( args );

    const auto highest = block_store.get_highest_block( koinos::rpc::block_store::get_highest_block_request{} );
    if( !highest.has_topology() || highest.topology().height() == 0 || highest.topology().id().empty() )
      throw std::runtime_error( "source block store has no usable highest block metadata" );

    ReplayStats stats;
    stats.source_head_height = highest.topology().height();

    if( !args.state_db_replay )
    {
      if( args.normal_removes )
        throw std::runtime_error( "--normal-removes requires --state-db-replay" );

      stats.scratch_start_height = 0;
      const uint64_t start_height = args.from_height ? args.from_height : 1;
      if( start_height != 1 )
        throw std::runtime_error( "direct delta replay currently starts from genesis; use --from-height 1" );

      const uint64_t end_height = args.to_height ? args.to_height : stats.source_head_height;
      if( end_height > stats.source_head_height )
        throw std::runtime_error( "--to-height exceeds source head height " + std::to_string( stats.source_head_height ) );

      stats.from_height = start_height;
      stats.to_height = end_height;
      stats.final_height = 0;
      stats.final_block_id = bytes_to_hex( zero_multihash_string() );
      stats.final_state_merkle_root = bytes_to_hex( zero_multihash_string() );

      if( logger )
      {
        logger->line( "replay_mode=direct-delta-root" );
        logger->line( "journal_dir=" + args.journal_dir.string() );
        logger->line( "source_head_height=" + std::to_string( stats.source_head_height ) );
        logger->line( "scratch_start_height=0" );
        logger->line( "from_height=" + std::to_string( stats.from_height ) );
        logger->line( "to_height=" + std::to_string( stats.to_height ) );
        logger->line( "initial_state_merkle_root=" + stats.final_state_merkle_root );
      }

      if( start_height > end_height )
      {
        if( logger )
          logger->line( "run_complete blocks_checked=0 final_height=" + std::to_string( stats.final_height ) );
        return stats;
      }

      ReplayJournal journal;
      journal.open( args.journal_dir );
      if( args.rebuild_journal
          || !journal.valid_for( args, stats.source_head_height, highest.topology().id(), end_height ) )
      {
        if( args.rebuild_journal
            || !journal.upgrade_flat_if_valid(
              args,
              stats.source_head_height,
              highest.topology().id(),
              end_height,
              logger.get() ) )
        {
          journal.build(
            args,
            source_db.db(),
            source_db.handle( ColumnFamily::blocks ),
            stats.source_head_height,
            highest.topology().id(),
            end_height,
            logger.get() );
        }
      }
      else if( logger )
      {
        logger->line( "journal_reused dir=" + args.journal_dir.string() );
      }

      ProgressReporter progress( "replay", start_height, end_height, args.progress_every, logger.get() );
      std::string current_block_id = zero_multihash_string();
      std::string current_state_root = zero_multihash_string();

      for( uint64_t height = start_height; height <= end_height; ++height )
      {
        bool saw_height = false;
        bool matched_parent = false;
        JournalRecord selected;

        for( auto& candidate: journal.records_at_height( height ) )
        {
          saw_height = true;
          if( candidate.header.previous() == current_block_id )
          {
            selected = std::move( candidate );
            matched_parent = true;
          }
        }

        if( !saw_height )
          throw std::runtime_error( "replay journal is missing block candidates at height " + std::to_string( height ) );
        if( !matched_parent )
          throw std::runtime_error( "replay journal has no canonical parent match at height " + std::to_string( height ) );

        if( selected.header.height() != height )
          throw std::runtime_error( "journal block height mismatch at height " + std::to_string( height ) );
        if( selected.header.previous_state_merkle_root() != current_state_root )
        {
          throw std::runtime_error( "block previous state merkle mismatch at height "
                                    + std::to_string( height )
                                    + ": expected=" + bytes_to_hex( selected.header.previous_state_merkle_root() )
                                    + " actual=" + bytes_to_hex( current_state_root ) );
        }
        if( selected.receipt.height() != 0 && selected.receipt.height() != height )
          throw std::runtime_error( "receipt height mismatch at block " + std::to_string( height ) );
        if( !selected.receipt.id().empty() && selected.receipt.id() != selected.block_id )
          throw std::runtime_error( "receipt id mismatch at block " + std::to_string( height ) );

        const auto computed_root = compute_delta_entries_merkle_root( selected.receipt.state_delta_entries() );
        if( selected.receipt.state_merkle_root().empty() )
        {
          ++stats.receipts_without_state_root;
        }
        else if( selected.receipt.state_merkle_root() != computed_root )
        {
          throw std::runtime_error( "block receipt state merkle mismatch at height "
                                    + std::to_string( height )
                                    + ": expected=" + bytes_to_hex( selected.receipt.state_merkle_root() )
                                    + " actual=" + bytes_to_hex( computed_root ) );
        }

        for( const auto& delta_entry: selected.receipt.state_delta_entries() )
        {
          if( delta_entry.has_value() )
            ++stats.receipt_puts;
          else
            ++stats.receipt_removes;
          ++stats.receipt_delta_entries;
        }

        current_block_id = selected.block_id;
        current_state_root = computed_root;
        stats.final_height = height;
        stats.final_block_id = bytes_to_hex( current_block_id );
        stats.final_state_merkle_root = bytes_to_hex( current_state_root );
        ++stats.blocks_checked;
        progress.maybe_update( height, stats );
      }

      progress.finish();
      if( logger )
      {
        logger->line( "run_complete blocks_checked=" + std::to_string( stats.blocks_checked )
                      + " receipt_delta_entries=" + std::to_string( stats.receipt_delta_entries )
                      + " final_height=" + std::to_string( stats.final_height )
                      + " final_block_id=" + stats.final_block_id
                      + " final_state_merkle_root=" + stats.final_state_merkle_root );
      }
      return stats;
    }

    auto replay_backend = std::make_shared< koinos::state_db::backends::rocksdb::rocksdb_backend >();
    replay_backend->force_async_writes( !args.sync_scratch_writes );
    replay_backend->open( args.scratch_state_dir );

    koinos::state_db::database replay_db;
    replay_db.open(
      std::move( replay_backend ),
      make_genesis_initializer( genesis ),
      koinos::state_db::pob_comparator,
      replay_db.get_unique_lock() );

    {
      auto lock = replay_db.get_shared_lock();
      auto head = replay_db.get_head( lock );
      if( !head )
        throw std::runtime_error( "scratch state has no head after open" );
      stats.scratch_start_height = head->revision();
    }

    const uint64_t start_height = args.from_height ? args.from_height : stats.scratch_start_height + 1;
    if( start_height == 0 )
      throw std::runtime_error( "from height must be greater than 0" );
    if( stats.scratch_start_height + 1 != start_height )
    {
      throw std::runtime_error( "scratch head is at height " + std::to_string( stats.scratch_start_height )
                                + " but replay would start at " + std::to_string( start_height )
                                + "; use --reset-scratch or choose --from-height "
                                + std::to_string( stats.scratch_start_height + 1 ) );
    }

    const uint64_t end_height = args.to_height ? args.to_height : stats.source_head_height;
    if( end_height > stats.source_head_height )
      throw std::runtime_error( "--to-height exceeds source head height " + std::to_string( stats.source_head_height ) );

    stats.from_height = start_height;
    stats.to_height = end_height;
    stats.final_height = stats.scratch_start_height;

    if( logger )
    {
      logger->line( "source_head_height=" + std::to_string( stats.source_head_height ) );
      logger->line( "scratch_start_height=" + std::to_string( stats.scratch_start_height ) );
      logger->line( "from_height=" + std::to_string( stats.from_height ) );
      logger->line( "to_height=" + std::to_string( stats.to_height ) );
    }

    if( start_height > end_height )
    {
      if( logger )
        logger->line( "run_complete blocks_checked=0 final_height=" + std::to_string( stats.final_height ) );
      return stats;
    }

    HeightRecordIndex height_index;
    height_index.open( args.height_index_dir );
    if( args.rebuild_height_index
        || !height_index.valid_for( args, stats.source_head_height, highest.topology().id(), end_height ) )
    {
      height_index.build(
        args,
        source_db.db(),
        source_db.handle( ColumnFamily::blocks ),
        stats.source_head_height,
        highest.topology().id(),
        end_height,
        logger.get() );
    }
    else if( logger )
    {
      logger->line( "height_index_reused dir=" + args.height_index_dir.string() );
    }

    ProgressReporter progress( "replay", start_height, end_height, args.progress_every, logger.get() );
    rocksdb::ReadOptions source_read_options;
    source_read_options.fill_cache = false;

    for( uint64_t height = start_height; height <= end_height; ++height )
    {
      const auto block_id = height_index.get_block_id( height );
      if( block_id.empty() )
        throw std::runtime_error( "height index is missing block at height " + std::to_string( height ) );

      std::string record_bytes;
      auto status = source_db.db()->Get(
        source_read_options,
        source_db.handle( ColumnFamily::blocks ),
        block_id,
        &record_bytes );
      if( status.IsNotFound() )
        throw std::runtime_error( "source block store is missing indexed block at height " + std::to_string( height ) );
      if( !status.ok() )
        throw std::runtime_error( "source block-store get failed at height " + std::to_string( height ) + ": "
                                  + status.ToString() );

      koinos::block_store::block_record record;
      if( !record.ParseFromString( record_bytes ) )
        throw std::runtime_error( "failed to parse height-index block record at height " + std::to_string( height ) );

      if( record.block_height() != height )
        throw std::runtime_error( "height-index block height mismatch: expected " + std::to_string( height )
                                  + " got " + std::to_string( record.block_height() ) );
      if( record.block_id() != block_id )
        throw std::runtime_error( "height-index block id mismatch at height " + std::to_string( height ) );
      if( !record.has_block() )
        throw std::runtime_error( "height-index block record is missing block at height " + std::to_string( height ) );
      if( !record.has_receipt() )
        throw std::runtime_error( "height-index block record is missing receipt at height " + std::to_string( height ) );

      apply_block_receipt( replay_db, record.block(), record.receipt(), args.normal_removes, stats );
      progress.maybe_update( height, stats );
    }

    progress.finish();
    if( logger )
    {
      logger->line( "run_complete blocks_checked=" + std::to_string( stats.blocks_checked )
                    + " receipt_delta_entries=" + std::to_string( stats.receipt_delta_entries )
                    + " final_height=" + std::to_string( stats.final_height )
                    + " final_block_id=" + stats.final_block_id
                    + " final_state_merkle_root=" + stats.final_state_merkle_root );
    }
    return stats;
  }
  catch( const std::exception& e )
  {
    if( logger )
      logger->line( std::string( "run_failed error=" ) + e.what() );
    throw;
  }
}

void print_text_result( const Args& args, const ReplayStats& stats )
{
  const auto replay_mode = args.state_db_replay ? "height-index-state-db" : "direct-delta-root";
  std::cout << "state delta replay audit: ok\n"
            << "source_db: " << args.source_db.string() << '\n'
            << "scratch_state_dir: " << args.scratch_state_dir.string() << '\n'
            << "replay_mode: " << replay_mode << '\n'
            << "journal_dir: " << args.journal_dir.string() << '\n'
            << "height_index_dir: " << args.height_index_dir.string() << '\n'
            << "log_file: " << ( args.log_file.empty() ? "disabled" : args.log_file.string() ) << '\n'
            << "remove_mode: " << ( args.normal_removes ? "normal" : "preserve-tombstone" ) << '\n'
            << "scratch_write_mode: " << ( args.sync_scratch_writes ? "sync" : "async" ) << '\n'
            << "source_head_height: " << stats.source_head_height << '\n'
            << "scratch_start_height: " << stats.scratch_start_height << '\n'
            << "from_height: " << stats.from_height << '\n'
            << "to_height: " << stats.to_height << '\n'
            << "blocks_checked: " << stats.blocks_checked << '\n'
            << "receipt_delta_entries: " << stats.receipt_delta_entries << '\n'
            << "receipt_puts: " << stats.receipt_puts << '\n'
            << "receipt_removes: " << stats.receipt_removes << '\n'
            << "receipts_without_state_root: " << stats.receipts_without_state_root << '\n'
            << "final_height: " << stats.final_height << '\n'
            << "final_block_id: " << stats.final_block_id << '\n'
            << "final_state_merkle_root: " << stats.final_state_merkle_root << '\n';
}

void print_json_result( const Args& args, const ReplayStats& stats )
{
  const auto replay_mode = args.state_db_replay ? "height-index-state-db" : "direct-delta-root";
  std::cout << "{\n"
            << "  \"ok\": true,\n"
            << "  \"source_db\": \"" << json_escape( args.source_db.string() ) << "\",\n"
            << "  \"scratch_state_dir\": \"" << json_escape( args.scratch_state_dir.string() ) << "\",\n"
            << "  \"replay_mode\": \"" << replay_mode << "\",\n"
            << "  \"journal_dir\": \"" << json_escape( args.journal_dir.string() ) << "\",\n"
            << "  \"height_index_dir\": \"" << json_escape( args.height_index_dir.string() ) << "\",\n"
            << "  \"log_file\": ";
  if( args.log_file.empty() )
    std::cout << "null,\n";
  else
    std::cout << "\"" << json_escape( args.log_file.string() ) << "\",\n";

  std::cout
            << "  \"remove_mode\": \"" << ( args.normal_removes ? "normal" : "preserve-tombstone" ) << "\",\n"
            << "  \"scratch_write_mode\": \"" << ( args.sync_scratch_writes ? "sync" : "async" ) << "\",\n"
            << "  \"source_head_height\": " << stats.source_head_height << ",\n"
            << "  \"scratch_start_height\": " << stats.scratch_start_height << ",\n"
            << "  \"from_height\": " << stats.from_height << ",\n"
            << "  \"to_height\": " << stats.to_height << ",\n"
            << "  \"blocks_checked\": " << stats.blocks_checked << ",\n"
            << "  \"receipt_delta_entries\": " << stats.receipt_delta_entries << ",\n"
            << "  \"receipt_puts\": " << stats.receipt_puts << ",\n"
            << "  \"receipt_removes\": " << stats.receipt_removes << ",\n"
            << "  \"receipts_without_state_root\": " << stats.receipts_without_state_root << ",\n"
            << "  \"final_height\": " << stats.final_height << ",\n"
            << "  \"final_block_id\": \"" << json_escape( stats.final_block_id ) << "\",\n"
            << "  \"final_state_merkle_root\": \"" << json_escape( stats.final_state_merkle_root ) << "\"\n"
            << "}\n";
}

} // namespace

int main( int argc, char** argv )
{
  Args args;
  try
  {
    args = parse_args( argc, argv );
    if( args.help )
    {
      usage( argv[ 0 ] );
      return EXIT_SUCCESS;
    }

    koinos::initialize_logging( "state_delta_replay_audit", {}, "warning" );

    const auto stats = run_replay( args );
    if( args.json )
      print_json_result( args, stats );
    else
      print_text_result( args, stats );
    return EXIT_SUCCESS;
  }
  catch( const std::exception& e )
  {
    if( args.json )
    {
      std::cout << "{\n"
                << "  \"ok\": false,\n"
                << "  \"error\": \"" << json_escape( e.what() ) << "\"";
      if( !args.log_file.empty() )
        std::cout << ",\n  \"log_file\": \"" << json_escape( args.log_file.string() ) << "\"\n";
      else
        std::cout << "\n";
      std::cout
                << "}\n";
    }
    else
    {
      std::cerr << "state delta replay audit failed: " << e.what() << '\n';
      usage( argv[ 0 ] );
    }
    return EXIT_FAILURE;
  }
}
