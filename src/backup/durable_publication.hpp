#pragma once
#include <filesystem>
#include <functional>
#include <string>
namespace koinos::node::backup {
using PublicationCheckpoint=std::function<void(const std::string&,const std::filesystem::path&)>;
void sync_publication_directory(const std::filesystem::path&);
void sync_publication_file(const std::filesystem::path&);
void create_publication_directories(const std::filesystem::path&);
void durable_atomic_write(const std::filesystem::path&,const std::string&,const PublicationCheckpoint& = {});
}
