#pragma once

#include <algorithm>
#include <exception>
#include <functional>
#include <stdexcept>
#include <string>
#include <vector>

#include <koinos/log.hpp>

namespace koinos::node {

/**
 * Manages component lifecycle: init → start → stop (reverse order).
 */
class ServiceRegistry
{
public:
  struct Component
  {
    std::string name;
    std::function< void() > start;
    std::function< void() > stop;
    bool started = false;
  };

  void add( const std::string& name, std::function< void() > start_fn, std::function< void() > stop_fn )
  {
    _components.push_back( { name, std::move( start_fn ), std::move( stop_fn ) } );
  }

  void start_all()
  {
    for( auto& comp: _components )
    {
      if( !comp.started )
        start_component( comp );
    }
  }

  void start( const std::string& name )
  {
    auto it = std::find_if(
      _components.begin(), _components.end(), [&]( const Component& component ) { return component.name == name; } );
    if( it == _components.end() )
      throw std::invalid_argument( "unknown service component: " + name );
    if( !it->started )
      start_component( *it );
  }

  void stop_all()
  {
    for( auto it = _components.rbegin(); it != _components.rend(); ++it )
    {
      if( !it->started )
        continue;

      LOG( info ) << "[" << it->name << "] Stopping...";
      try
      {
        it->stop();
        it->started = false;
        LOG( info ) << "[" << it->name << "] Stopped";
      }
      catch( const std::exception& e )
      {
        LOG( warning ) << "[" << it->name << "] Error during stop: " << e.what();
      }
      catch( ... )
      {
        LOG( warning ) << "[" << it->name << "] Unknown error during stop";
      }
    }
  }

  const std::vector< Component >& components() const { return _components; }

private:
  void start_component( Component& comp )
  {
    LOG( info ) << "[" << comp.name << "] Starting...";
    try
    {
      comp.start();
      comp.started = true;
      LOG( info ) << "[" << comp.name << "] Started";
    }
    catch( const std::exception& e )
    {
      LOG( error ) << "[" << comp.name << "] Start failed: " << e.what();
      stop_all();
      throw;
    }
    catch( ... )
    {
      LOG( error ) << "[" << comp.name << "] Start failed: unknown exception";
      stop_all();
      throw;
    }
  }

  std::vector< Component > _components;
};

} // namespace koinos::node
