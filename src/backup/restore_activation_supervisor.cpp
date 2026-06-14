#include "backup/restore_activation_supervisor.hpp"

#include <fstream>
#include <sstream>
#include <stdexcept>

#include <nlohmann/json.hpp>

namespace koinos::node::backup {
namespace {

std::string read_text_file( const std::filesystem::path& path )
{
  std::ifstream input( path, std::ios::binary );
  if( !input )
    throw std::runtime_error( "failed to read restore activation intent: " + path.string() );
  return std::string( ( std::istreambuf_iterator< char >( input ) ),
                      std::istreambuf_iterator< char >() );
}

std::string json_escape( const std::string& value )
{
  std::ostringstream out;
  for( unsigned char ch: value )
  {
    switch( ch )
    {
      case '"': out << "\\\""; break;
      case '\\': out << "\\\\"; break;
      case '\b': out << "\\b"; break;
      case '\f': out << "\\f"; break;
      case '\n': out << "\\n"; break;
      case '\r': out << "\\r"; break;
      case '\t': out << "\\t"; break;
      default:
        if( ch < 0x20 )
        {
          static const char* hex = "0123456789abcdef";
          out << "\\u00" << hex[ ch >> 4 ] << hex[ ch & 0x0f ];
        }
        else
        {
          out << static_cast< char >( ch );
        }
    }
  }
  return out.str();
}

std::filesystem::path absolute_normalized( const std::filesystem::path& path )
{
  std::error_code ec;
  auto normalized = std::filesystem::weakly_canonical( path, ec );
  if( ec )
    normalized = std::filesystem::absolute( path, ec );
  if( ec )
    normalized = path;
  return normalized.lexically_normal();
}

} // anonymous namespace

std::filesystem::path restore_activation_intent_path( const std::filesystem::path& basedir )
{
  return basedir / ".teleno-restore-activation-request.json";
}

std::optional< RestoreActivationIntent > read_pending_restore_activation_request(
  const std::filesystem::path& basedir )
{
  const auto intent_path = restore_activation_intent_path( basedir );
  if( !std::filesystem::exists( intent_path ) )
    return std::nullopt;

  const auto intent_json = nlohmann::json::parse( read_text_file( intent_path ) );
  const auto format = intent_json.value( "format", std::string{} );
  if( format != "teleno-native-restore-activation-request" )
    throw std::runtime_error( "invalid restore activation intent format: " + intent_path.string() );
  if( intent_json.value( "version", 0 ) != 1 )
    throw std::runtime_error( "unsupported restore activation intent version: " + intent_path.string() );

  RestoreActivationIntent intent;
  intent.intent_path = intent_path;
  intent.target_basedir = intent_json.value( "target_basedir", std::string{} );
  intent.staging_dir = intent_json.value( "staging_dir", std::string{} );
  intent.requires_node_stop = intent_json.value( "requires_node_stop", true );

  if( intent.target_basedir.empty() )
    throw std::runtime_error( "restore activation intent is missing target_basedir: " + intent_path.string() );
  if( intent.staging_dir.empty() )
    throw std::runtime_error( "restore activation intent is missing staging_dir: " + intent_path.string() );
  if( absolute_normalized( intent.target_basedir ) != absolute_normalized( basedir ) )
    throw std::runtime_error( "restore activation intent target does not match this basedir: "
                              + intent.target_basedir.string() );

  return intent;
}

bool has_pending_restore_activation_request( const std::filesystem::path& basedir )
{
  return std::filesystem::exists( restore_activation_intent_path( basedir ) );
}

RestoreActivationResult activate_pending_restore_activation_request(
  const std::filesystem::path& basedir )
{
  auto intent = read_pending_restore_activation_request( basedir );
  if( !intent )
    throw std::runtime_error( "no pending restore activation request exists in basedir: " + basedir.string() );

  auto result = activate_staged_restore_snapshot( intent->staging_dir, basedir );
  std::filesystem::remove( intent->intent_path );
  return result;
}

std::string restore_activation_intent_to_json( const RestoreActivationIntent& intent )
{
  std::ostringstream out;
  out << "{\n";
  out << "  \"target_basedir\": \"" << json_escape( intent.target_basedir.string() ) << "\",\n";
  out << "  \"staging_dir\": \"" << json_escape( intent.staging_dir.string() ) << "\",\n";
  out << "  \"intent_path\": \"" << json_escape( intent.intent_path.string() ) << "\",\n";
  out << "  \"requires_node_stop\": " << ( intent.requires_node_stop ? "true" : "false" ) << "\n";
  out << "}\n";
  return out.str();
}

} // namespace koinos::node::backup
