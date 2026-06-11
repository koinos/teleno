#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <openssl/sha.h>
#include <rocksdb/db.h>
#include <rocksdb/options.h>

namespace {

constexpr const char* magic = "KBS1\n";
const std::string meta_key( 1, '\x01' );

uint32_t read_le32( const std::array< char, 12 >& header )
{
  return static_cast< uint32_t >( static_cast< unsigned char >( header[ 0 ] ) )
       | ( static_cast< uint32_t >( static_cast< unsigned char >( header[ 1 ] ) ) << 8 )
       | ( static_cast< uint32_t >( static_cast< unsigned char >( header[ 2 ] ) ) << 16 )
       | ( static_cast< uint32_t >( static_cast< unsigned char >( header[ 3 ] ) ) << 24 );
}

uint64_t read_le64( const std::array< char, 12 >& header )
{
  uint64_t value = 0;
  for( int i = 0; i < 8; ++i )
    value |= static_cast< uint64_t >( static_cast< unsigned char >( header[ 4 + i ] ) ) << ( i * 8 );
  return value;
}

bool read_exact( std::istream& in, char* data, std::size_t size )
{
  in.read( data, static_cast< std::streamsize >( size ) );
  return static_cast< std::size_t >( in.gcount() ) == size;
}

void usage( const char* argv0 )
{
  std::cerr << "Usage: " << argv0
            << " --db /path/to/monolith/basedir/db [--progress-every N]"
            << " [--hash-manifest PATH --hash-every N [--hash-limit N]]\n";
}

std::string sha256_hex( const std::string& data )
{
  unsigned char digest[ SHA256_DIGEST_LENGTH ];
  SHA256( reinterpret_cast< const unsigned char* >( data.data() ), data.size(), digest );

  std::ostringstream out;
  out << std::hex << std::setfill( '0' );
  for( unsigned char byte: digest )
    out << std::setw( 2 ) << static_cast< int >( byte );
  return out.str();
}

class HashManifest
{
public:
  HashManifest( std::filesystem::path path, uint64_t every, uint64_t limit )
      : _path( std::move( path ) ), _every( every ), _limit( limit )
  {
    if( !_path.empty() && _every == 0 )
      throw std::runtime_error( "--hash-every must be greater than 0 when --hash-manifest is set" );

    if( !_path.empty() )
    {
      _out.open( _path );
      if( !_out )
        throw std::runtime_error( "could not open hash manifest: " + _path.string() );
    }
  }

  void maybe_write( uint64_t record_index,
                    uint64_t block_index,
                    const std::string& column_family,
                    const std::string& key,
                    const std::string& value )
  {
    if( !_out )
      return;

    if( column_family != "block_meta" )
    {
      if( block_index == 0 || block_index % _every != 0 )
        return;
      if( _limit > 0 && _selected_blocks >= _limit )
        return;
      ++_selected_blocks;
    }

    _out << record_index << '\t'
         << column_family << '\t'
         << key.size() << '\t'
         << value.size() << '\t'
         << sha256_hex( key ) << '\t'
         << sha256_hex( value ) << '\n';
  }

private:
  std::filesystem::path _path;
  std::ofstream _out;
  uint64_t _every = 0;
  uint64_t _limit = 0;
  uint64_t _selected_blocks = 0;
};

std::string read_back_value( rocksdb::DB& db,
                             rocksdb::ColumnFamilyHandle* cf,
                             const std::string& key )
{
  std::string value;
  auto status = db.Get( rocksdb::ReadOptions(), cf, key, &value );
  if( !status.ok() )
    throw std::runtime_error( "rocksdb readback: " + status.ToString() );
  return value;
}

} // namespace

int main( int argc, char** argv )
{
  std::filesystem::path db_path;
  std::filesystem::path hash_manifest_path;
  uint64_t progress_every = 100000;
  uint64_t hash_every = 0;
  uint64_t hash_limit = 0;

  for( int i = 1; i < argc; ++i )
  {
    std::string arg = argv[ i ];
    if( arg == "--db" && i + 1 < argc )
    {
      db_path = argv[ ++i ];
    }
    else if( arg == "--progress-every" && i + 1 < argc )
    {
      progress_every = std::stoull( argv[ ++i ] );
    }
    else if( arg == "--hash-manifest" && i + 1 < argc )
    {
      hash_manifest_path = argv[ ++i ];
    }
    else if( arg == "--hash-every" && i + 1 < argc )
    {
      hash_every = std::stoull( argv[ ++i ] );
    }
    else if( arg == "--hash-limit" && i + 1 < argc )
    {
      hash_limit = std::stoull( argv[ ++i ] );
    }
    else if( arg == "-h" || arg == "--help" )
    {
      usage( argv[ 0 ] );
      return 0;
    }
    else
    {
      usage( argv[ 0 ] );
      return 2;
    }
  }

  if( db_path.empty() )
  {
    usage( argv[ 0 ] );
    return 2;
  }

  std::unique_ptr< HashManifest > hash_manifest;
  try
  {
    hash_manifest = std::make_unique< HashManifest >( hash_manifest_path, hash_every, hash_limit );
  }
  catch( const std::exception& e )
  {
    std::cerr << e.what() << "\n";
    return 2;
  }

  std::filesystem::create_directories( db_path );

  rocksdb::Options options;
  options.create_if_missing = true;
  options.create_missing_column_families = true;
  options.max_background_jobs = 4;
  options.bytes_per_sync = 1048576;

  std::vector< rocksdb::ColumnFamilyDescriptor > descriptors = {
    { rocksdb::kDefaultColumnFamilyName, rocksdb::ColumnFamilyOptions() },
    { "blocks", rocksdb::ColumnFamilyOptions() },
    { "block_meta", rocksdb::ColumnFamilyOptions() },
    { "contract_meta", rocksdb::ColumnFamilyOptions() },
    { "transaction_index", rocksdb::ColumnFamilyOptions() },
    { "account_history", rocksdb::ColumnFamilyOptions() }
  };

  rocksdb::DB* raw_db = nullptr;
  std::vector< rocksdb::ColumnFamilyHandle* > handles;
  auto status = rocksdb::DB::Open( options, db_path.string(), descriptors, &handles, &raw_db );
  if( !status.ok() )
  {
    std::cerr << "open rocksdb: " << status.ToString() << "\n";
    return 1;
  }
  std::unique_ptr< rocksdb::DB > db( raw_db );

  std::array< char, 5 > got_magic{};
  if( !read_exact( std::cin, got_magic.data(), got_magic.size() ) || std::string( got_magic.data(), got_magic.size() ) != magic )
  {
    std::cerr << "invalid stream magic\n";
    return 1;
  }

  std::array< char, 12 > header{};
  uint64_t records = 0;
  uint64_t block_records = 0;
  uint64_t meta_records = 0;
  uint64_t bytes = 0;

  rocksdb::WriteOptions write_options;
  write_options.sync = false;

  while( true )
  {
    std::cin.read( header.data(), static_cast< std::streamsize >( header.size() ) );
    auto read_count = std::cin.gcount();
    if( read_count == 0 && std::cin.eof() )
      break;
    if( read_count != static_cast< std::streamsize >( header.size() ) )
    {
      std::cerr << "truncated stream header\n";
      return 1;
    }

    auto key_size = read_le32( header );
    auto value_size = read_le64( header );
    std::string key( key_size, '\0' );
    std::string value( value_size, '\0' );
    if( !read_exact( std::cin, key.data(), key.size() ) || !read_exact( std::cin, value.data(), value.size() ) )
    {
      std::cerr << "truncated stream record\n";
      return 1;
    }

    const bool is_meta = key == meta_key;
    auto* cf = is_meta ? handles[ 2 ] : handles[ 1 ];
    status = db->Put( write_options, cf, key, value );
    if( !status.ok() )
    {
      std::cerr << "rocksdb put: " << status.ToString() << "\n";
      return 1;
    }

    records++;
    bytes += key.size() + value.size();
    if( is_meta )
      meta_records++;
    else
      block_records++;

    try
    {
      if( hash_manifest )
        hash_manifest->maybe_write(
          records,
          block_records,
          is_meta ? "block_meta" : "blocks",
          key,
          read_back_value( *db, cf, key ) );
    }
    catch( const std::exception& e )
    {
      std::cerr << e.what() << "\n";
      return 1;
    }

    if( progress_every > 0 && records % progress_every == 0 )
    {
      std::cerr << "imported records=" << records
                << " blocks=" << block_records
                << " meta=" << meta_records
                << " bytes=" << bytes << "\n";
    }
  }

  for( auto* handle: handles )
    db->Flush( rocksdb::FlushOptions(), handle );

  for( auto* handle: handles )
    delete handle;

  std::cerr << "import complete records=" << records
            << " blocks=" << block_records
            << " meta=" << meta_records
            << " bytes=" << bytes << "\n";

  return 0;
}
