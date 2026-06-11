# Compatibility aliases for building cpp-libp2p against koinos-node's Hunter
# package set. The packages are ABI-compatible, but some exported target names
# differ from what cpp-libp2p transitive configs reference.

find_package(ZLIB REQUIRED)
if(TARGET ZLIB::ZLIB AND NOT TARGET ZLIB::zlib)
  add_library(ZLIB::zlib ALIAS ZLIB::ZLIB)
endif()

find_package(yaml-cpp CONFIG REQUIRED)
if(TARGET yaml-cpp AND NOT TARGET yaml-cpp::yaml-cpp)
  add_library(yaml-cpp::yaml-cpp ALIAS yaml-cpp)
endif()
