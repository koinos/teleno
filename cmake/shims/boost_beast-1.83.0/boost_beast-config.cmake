# Boost.Beast is header-only. Hunter's Boost 1.83 install used by koinos-node
# does not ship a component config for it, but cpp-libp2p asks for one in
# find_package(Boost COMPONENTS ... beast ...). Satisfy that component without
# adding another Boost build into the link graph.
if(NOT TARGET Boost::beast)
  add_library(Boost::beast INTERFACE IMPORTED)
  target_link_libraries(Boost::beast INTERFACE Boost::boost)
endif()

set(boost_beast_FOUND TRUE)
