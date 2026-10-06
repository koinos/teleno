// Reuse the repository's deterministic, test-only genesis and block signer.
// No operator key material is read or written by this fixture.
#define main inherited_controller_delta_main
#include "../chain/controller_delta_test.cpp"
#undef main

#include <koinos/chain/system_calls.pb.h>
#include <koinos/util/base64.hpp>
#include "storage/rocksdb_manager.hpp"
#include <google/protobuf/util/json_util.h>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace {
void require(bool value, const char* message) {
  if (!value) throw std::runtime_error(message);
}
std::string json(const google::protobuf::Message& message) {
  google::protobuf::util::JsonPrintOptions options;
  options.preserve_proto_field_names = true;
  std::string result;
  require(google::protobuf::util::MessageToJsonString(message, &result, options).ok(), "fixture JSON encoding");
  return result;
}
void write(const std::filesystem::path& path, const std::string& value) {
  std::ofstream stream(path, std::ios::binary);
  stream << value;
  require(stream.good(), "fixture output write");
}
std::string wire(const std::string& payload) {
  chain::get_object_result result;
  result.mutable_value()->set_exists(true);
  result.mutable_value()->set_value(payload);
  return result.SerializeAsString();
}
}

int main(int argc, char** argv) {
  try {
    require(argc == 2 || argc == 3, "fresh owned fixture directory and optional bytecode path required");
    const std::filesystem::path root(argv[1]);
    require(root.is_absolute() && std::filesystem::is_directory(root), "owned fixture directory required");
    require(!std::filesystem::exists(root / "genesis_data.json"), "fixture must be fresh");
    std::string payload(109002, 'q');
    if (argc == 3) {
      std::ifstream stream(argv[2], std::ios::binary);
      require(stream.good(), "selected bytecode open");
      payload.assign(std::istreambuf_iterator<char>(stream), {});
      require(payload.size() == 109002 && payload.substr(0, 4) == std::string("\0asm", 4), "selected bytecode format/size");
    }
    std::string boundary(131072, 'b');
    while (wire(boundary).size() > 131072) boundary.pop_back();
    require(wire(boundary).size() == 131072, "exact return-wire boundary");
    auto genesis = make_genesis();
    for (const auto& [slot, value] : std::vector<std::pair<std::string,std::string>>{
      {"large", payload}, {"boundary", boundary}, {"oversize", boundary + "b"}, {"small", "read-after-refusal"}}) {
      auto* entry = genesis.add_entries();
      *entry->mutable_space() = chain::state::space::contract_bytecode();
      entry->set_key(slot);
      entry->set_value(value);
      chain::get_object_arguments args;
      *args.mutable_space() = entry->space();
      args.set_key(slot);
      rpc::chain::invoke_system_call_request request;
      request.set_name("get_object");
      request.set_args(args.SerializeAsString());
      write(root / (slot + "-request.json"), json(request));
      write(root / (slot + "-expected-wire.bin"), wire(value));
    }
    write(root / "genesis_data.json", json(genesis));
    write(root / "selected-bytecode.bin", payload);

    // Reproduce the old capacity failure with the same public/test payload.
    chain::controller old(10'000'000, 64'000, {});
    old.open(std::make_shared<state_db::backends::map::map_backend>(), genesis, chain::fork_resolution_algorithm::pob, false);
    chain::get_object_arguments args;
    *args.mutable_space() = chain::state::space::contract_bytecode();
    args.set_key("large");
    rpc::chain::invoke_system_call_request read;
    read.set_name("get_object"); read.set_args(args.SerializeAsString());
    bool refused = false;
    try { old.invoke_system_call(read); }
    catch (const std::exception& e) {
      const std::string error(e.what());
      require(error.find("buffer") != std::string::npos && error.find("return") != std::string::npos, "unexpected old-capacity refusal");
      refused = true;
    }
    require(refused, "old capacity unexpectedly returned selected bytecode");
    old.close();

    // Match the native node's persisted genesis metadata and shared CF layout.
    node::storage::RocksDBManager reference_storage;
    node::NodeConfig reference_config;
    reference_storage.open(root / "reference-state", reference_config);
    auto backend = std::make_shared<state_db::backends::rocksdb::rocksdb_backend>();
    backend->open(*reference_storage.db(),
                  *reference_storage.handle(node::storage::ColumnFamily::default_state),
                  *reference_storage.handle(node::storage::ColumnFamily::chain_state),
                  *reference_storage.handle(node::storage::ColumnFamily::chain_metadata));
    chain::controller reference(10'000'000, 131'072, {});
    reference.open(std::move(backend), genesis, chain::fork_resolution_algorithm::pob, false);
    auto previous = util::converter::as<std::string>(crypto::multihash::zero(crypto::multicodec::sha2_256));
    const auto now = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::system_clock::now().time_since_epoch()).count();
    for (uint64_t height = 1; height <= 3; ++height) {
      const auto block = make_signed_block(height, previous, reference.get_head_info().head_state_merkle_root(), now + (height - 1) * 1000);
      rpc::chain::submit_block_request request;
      *request.mutable_block() = block;
      const auto response = reference.submit_block(request, 3);
      require(response.has_receipt() && response.receipt().state_merkle_root().size() == 34, "reference finalized receipt required");
      const auto prefix = "block-" + std::to_string(height);
      write(root / (prefix + "-request.json"), json(request));
      write(root / (prefix + "-id.bin"), block.id());
      write(root / (prefix + "-root.bin"), response.receipt().state_merkle_root());
      previous = block.id();
    }
    reference.close();
    reference_storage.close();
    std::cout << "Prepared owned native fixture; old-capacity refusal reproduced; three reference roots generated.\n";
    return 0;
  } catch (const std::exception& e) {
    std::cerr << e.what() << '\n';
    return 1;
  }
}
