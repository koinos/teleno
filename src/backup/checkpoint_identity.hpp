#pragma once
#include <filesystem>
#include <nlohmann/json.hpp>

namespace koinos::node::backup {
// Reads the checkpoint, never the live database. No historical replay claim.
nlohmann::json read_checkpoint_identity(const std::filesystem::path& db,
                                        const std::filesystem::path& genesis);
// Schema/consistency gate for native publication, not publisher authentication.
void validate_public_checkpoint_identity(const nlohmann::json& source,const std::string& network);
}
