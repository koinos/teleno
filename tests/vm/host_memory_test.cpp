#include <koinos/chain/host_api.hpp>
#include <koinos/chain/state.hpp>
#include <koinos/chain/system_call_ids.pb.h>
#include <koinos/chain/thunk_dispatcher.hpp>
#include <koinos/state_db/backends/map/map_backend.hpp>
#include <koinos/state_db/state_db.hpp>
#include <koinos/vm_manager/fizzy/fizzy_vm_backend.hpp>
#include <koinos/vm_manager/fizzy/exceptions.hpp>
#include <koinos/util/conversion.hpp>
#include "core/owned_memory_budget.hpp"

#include <array>
#include <cstring>
#include <functional>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
using namespace koinos;
namespace vm = vm_manager;
namespace fv = vm::fizzy;
void require(bool condition, const char* message)
{
  if (!condition) throw std::runtime_error(message);
}
uint32_t load_count(const void* source)
{
  uint32_t value;
  std::memcpy(&value, source, sizeof(value));
  return value;
}
std::string hex(const char* data, size_t size)
{
  static constexpr char digits[] = "0123456789abcdef";
  std::string output;
  for (size_t i = 0; i < size; ++i)
  {
    const auto byte = static_cast<unsigned char>(data[i]);
    output += digits[byte >> 4]; output += digits[byte & 15];
  }
  return output;
}
class OverrideBackend final : public vm::vm_backend
{
public:
  std::string observed_args;
  unsigned calls = 0;
  std::string backend_name() override { return "host-memory-fixture"; }
  void initialize() override {}
  void run(vm::abstract_host_api& api, const std::string&, const std::string&) override
  {
    // Read arguments through the real host entry, which adds the syscall frame
    // expected by get_arguments. A bare context read would select its caller.
    char response[256]{}; uint32_t written = 0;
    const auto code = api.invoke_system_call(chain::system_call_id::get_arguments,
      response, sizeof(response), "", 0, &written);
    chain::get_arguments_result arguments;
    require(code == chain::success && arguments.ParseFromArray(response, written),
            "override could not read its arguments");
    ++calls; observed_args = arguments.value().arguments();
    chain::execution_result result;
    result.res.set_object("override-result"); ctx->set_result(std::move(result));
  }
  chain::execution_context* ctx = nullptr;
};
class Fixture
{
public:
  state_db::database db;
  std::shared_ptr<OverrideBackend> backend = std::make_shared<OverrideBackend>();
  chain::execution_context ctx{backend, chain::intent::read_only};
  chain::host_api api{ctx};
  Fixture()
  {
    auto lock = db.get_unique_lock();
    db.open(std::make_shared<state_db::backends::map::map_backend>(),
      [](state_db::state_node_ptr root) {
        chain::compute_bandwidth_registry registry;
        for (const auto* name : {"deserialize_message_per_byte", "pre_block_callback",
                                "get_chain_id", "get_arguments", "get_object", "object_serialization_per_byte"})
        {
          auto* item = registry.add_entries(); item->set_name(name); item->set_compute(1);
        }
        const auto encoded = registry.SerializeAsString();
        root->put_object(chain::state::space::metadata(), chain::state::key::compute_bandwidth_registry, &encoded);
        const std::string chain_id = "host-memory-chain-fixture";
        root->put_object(chain::state::space::metadata(), chain::state::key::chain_id, &chain_id);
        protocol::system_call_target target;
        target.mutable_system_call_bundle()->set_contract_id("override-fixture");
        target.mutable_system_call_bundle()->set_entry_point(7);
        const auto dispatch = target.SerializeAsString();
        const auto sid = util::converter::as<std::string>(uint32_t(1000000));
        root->put_object(chain::state::space::system_call_dispatch(), sid, &dispatch);
        chain::contract_metadata_object meta; meta.set_system(true); meta.set_hash("fixture-id");
        const auto metadata = meta.SerializeAsString();
        root->put_object(chain::state::space::contract_metadata(), "override-fixture", &metadata);
        const std::string bytecode = "fixture-not-executed-as-wasm";
        root->put_object(chain::state::space::contract_bytecode(), "override-fixture", &bytecode);
      }, state_db::pob_comparator, lock);
    const auto root = db.get_root(lock);
    ctx.set_state_node(root->create_anonymous_node(), root);
    ctx.push_frame({.call_privilege = chain::privilege::kernel_mode});
    backend->ctx = &ctx;
  }
};

void direct_thunks()
{
  chain::execution_context ctx(nullptr, chain::intent::read_only);
  chain::get_chain_id_arguments args;
  const std::function<chain::get_chain_id_result(chain::execution_context&)> thunk =
    [](auto&) { chain::get_chain_id_result ret; ret.set_value("abc"); return ret; };
  const std::function<void(chain::execution_context&)> nothing = [](auto&) {};
  // Aligned base makes offsets 1, 2 and 3 genuinely unaligned, independent of
  // stack placement. Only byte copies read the counter in the test itself.
  for (size_t offset : {size_t(0), size_t(1), size_t(2), size_t(3)})
  {
    alignas(uint32_t) std::array<char, 32> buffer; buffer.fill('Z');
    auto* count = reinterpret_cast<uint32_t*>(buffer.data() + offset);
    char ret[32]{};
    chain::detail::call_thunk_impl<chain::get_chain_id_arguments, chain::get_chain_id_result>(
      thunk, ctx, ret, sizeof(ret), args, count);
    require(load_count(count) == 5 && std::string(ret, 5) == std::string("\x0a\x03" "abc", 5),
            "serialized thunk result/count changed");
    for (size_t i = 0; i < buffer.size(); ++i)
      if (i < offset || i >= offset + 4) require(buffer[i] == 'Z', "count store crossed four bytes");
    chain::detail::call_thunk_impl<chain::get_chain_id_arguments, chain::get_chain_id_result>(
      nothing, ctx, ret, sizeof(ret), args, count);
    require(load_count(count) == 0, "void thunk count changed");
  }
  // Serialize first, then count: a deferred copy or rejection changes aliasing.
  alignas(uint32_t) std::array<char, 32> overlap; overlap.fill('Z');
  chain::detail::call_thunk_impl<chain::get_chain_id_arguments, chain::get_chain_id_result>(
    thunk, ctx, overlap.data(), overlap.size(), args, reinterpret_cast<uint32_t*>(overlap.data() + 1));
  require(overlap[0] == '\x0a' && load_count(overlap.data() + 1) == 5 && overlap[5] == 'Z',
          "result/count alias ordering changed");
  std::cout << "thunk_overlap=" << hex(overlap.data(), 8) << '\n';
}

void native_hosts(bool syscall_only = false)
{
  Fixture fixture;
  for (bool syscall : (syscall_only ? std::vector<bool>{true} : std::vector<bool>{false, true}))
    for (size_t offset : {size_t(0), size_t(1), size_t(2), size_t(3)})
      for (uint32_t id : {uint32_t(chain::system_call_id::pre_block_callback),
                         uint32_t(chain::system_call_id::get_chain_id), uint32_t(999999)})
        for (uint32_t ret_size : {uint32_t(0), uint32_t(256)})
        {
          alignas(uint32_t) std::array<char, 8> count_bytes; count_bytes.fill('Z');
          auto* count = reinterpret_cast<uint32_t*>(count_bytes.data() + offset);
          std::array<char, 256> ret{};
          auto invoke = [&](uint32_t* result_count) {
            return syscall ? fixture.api.invoke_system_call(id, ret.data(), ret_size, "", 0, result_count)
                           : fixture.api.invoke_thunk(id, ret.data(), ret_size, "", 0, result_count);
          };
          const auto code = invoke(count);
          const auto size = load_count(count);
          require(size <= ret_size, "host wrote count beyond response capacity");
          uint32_t aligned_count = 0;
          const auto aligned_code = invoke(&aligned_count);
          require(code == aligned_code && size == aligned_count, "alignment changed native result/code");
          std::cout << "host=" << syscall << ',' << offset << ',' << id << ',' << ret_size
                    << ',' << code << ',' << size << ',' << hex(ret.data(), size) << '\n';
        }
  // Initial zero precedes copying override arguments. Preserve that observable
  // behavior when the count overlaps the caller's argument representation.
  alignas(uint32_t) std::array<char, 16> arg; arg.fill('a');
  std::array<char, 64> ret{};
  const auto code = fixture.api.invoke_system_call(1000000, ret.data(), ret.size(), arg.data(), 8,
    reinterpret_cast<uint32_t*>(arg.data() + 1));
  require(code == chain::success && fixture.backend->calls == 1 &&
          fixture.backend->observed_args == std::string("a\0\0\0\0aaa", 8) &&
          load_count(arg.data() + 1) == 15 && std::string(ret.data(), 15) == "override-result",
          "override argument/count ordering changed");
  std::cout << "override_args=" << hex(fixture.backend->observed_args.data(), 8) << '\n';
}

void uleb(std::string& out, uint32_t value)
{
  do { const auto byte = value & 127; value >>= 7; out += char(byte | (value ? 128 : 0)); } while(value);
}
void sleb(std::string& out, int32_t value)
{
  bool more;
  do { const auto byte = value & 127; value >>= 7;
       more = !((value == 0 && !(byte & 64)) || (value == -1 && (byte & 64)));
       out += char(byte | (more ? 128 : 0)); } while(more);
}
void section(std::string& out, char id, const std::string& bytes)
{ out += id; uleb(out, bytes.size()); out += bytes; }
std::string callback(bool syscall, uint32_t count_offset)
{
  std::string out("\0asm\1\0\0\0", 8);
  std::string types("\2\x60\6", 3); types.append(6, '\x7f'); types.append("\1\x7f\x60\0\0", 5);
  section(out, 1, types);
  std::string imports("\1\3env", 5);
  const std::string name = syscall ? "invoke_system_call" : "invoke_thunk";
  uleb(imports, name.size()); imports += name; imports.append("\0\0", 2); section(out, 2, imports);
  section(out, 3, std::string("\1\1", 2)); section(out, 5, std::string("\1\0\1", 3));
  section(out, 7, std::string("\1\6_start\0\1", 10));
  std::string body(1, '\0');
  for (uint32_t arg : {uint32_t(chain::system_call_id::get_chain_id),
                       0u, 64u, 100u, 0u, count_offset})
  { body += '\x41'; sleb(body, static_cast<int32_t>(arg)); }
  body.append("\x10\0\x1a\x0b", 4);
  std::string code(1, '\1'); uleb(code, body.size()); code += body; section(out, 10, code);
  return out;
}
class ObservingHost : public vm::abstract_host_api
{
public:
  Fixture fixture;
  uint32_t count = 0;
  std::string returned;
  unsigned calls = 0;
  int32_t invoke(bool syscall, uint32_t id, char* ret, uint32_t ret_size,
                 const char* arg, uint32_t arg_size, uint32_t* written)
  {
    ++calls;
    const auto code = syscall ? fixture.api.invoke_system_call(id, ret, ret_size, arg, arg_size, written)
                              : fixture.api.invoke_thunk(id, ret, ret_size, arg, arg_size, written);
    count = load_count(written); require(count <= ret_size, "VM result count out of bounds");
    returned.assign(ret, count); return code;
  }
  int32_t invoke_thunk(uint32_t id, char* ret, uint32_t size, const char* arg, uint32_t n, uint32_t* count) override
  { return invoke(false, id, ret, size, arg, n, count); }
  int32_t invoke_system_call(uint32_t id, char* ret, uint32_t size, const char* arg, uint32_t n, uint32_t* count) override
  { return invoke(true, id, ret, size, arg, n, count); }
  int64_t get_meter_ticks() const override { return fixture.api.get_meter_ticks(); }
  void use_meter_ticks(uint64_t n) override { fixture.api.use_meter_ticks(n); }
};
void actual_vm()
{
  fv::fizzy_vm_backend backend;
  for (bool syscall : {false, true})
  {
    uint64_t expected_ticks = 0;
    for (uint32_t offset : {0u, 1u, 2u, 3u, 65531u, 65532u})
    {
      ObservingHost host;
      const auto before = host.get_meter_ticks();
      backend.run(host, callback(syscall, offset), "");
      chain::get_chain_id_result result;
      result.set_value("host-memory-chain-fixture");
      const auto encoded = result.SerializeAsString();
      auto expected = encoded;
      // A count at offsets 0..3 aliases the actual return bytes by design.
      if (offset < encoded.size())
      {
        const uint32_t n = encoded.size(); std::memcpy(expected.data() + offset, &n, sizeof(n));
      }
      require(host.calls == 1 && host.count == encoded.size() && host.returned == expected,
              "real VM host output/count changed");
      const auto used = uint64_t(before - host.get_meter_ticks());
      if (!expected_ticks) expected_ticks = used;
      require(used == expected_ticks, "count alignment changed VM metering");
      std::cout << "vm=" << syscall << ',' << offset << ',' << used << ',' << hex(host.returned.data(), host.returned.size()) << '\n';
    }
    for (uint32_t offset : {65533u, 65536u, UINT32_MAX})
    {
      ObservingHost host; bool refused = false;
      try { backend.run(host, callback(syscall, offset), ""); }
      catch (const fv::wasm_memory_exception&) { refused = true; }
      require(refused && !host.calls, "out-of-bounds count entered native host");
    }
    // Valid execution still works after the rejected instances are destroyed.
    ObservingHost retry; backend.run(retry, callback(syscall, 17), "retry");
    require(retry.calls == 1 && retry.count, "bounds rejection prevented clean retry");
  }
}

class CapacityHost final : public vm::abstract_host_api
{
public:
  std::string retained;
  unsigned calls = 0;
  int64_t ticks = 1000000;
  node::OwnedMemoryBudget* refusal_budget = nullptr;
  int32_t invoke_thunk(uint32_t, char*, uint32_t, const char*, uint32_t, uint32_t* written) override
  {
    ++calls;
    if (refusal_budget)
    {
      const auto usage = refusal_budget->snapshot();
      auto occupied = refusal_budget->reserve(usage.limit_bytes - usage.accounted_bytes);
      retained.assign(8192, 'r');
    }
    else retained.assign(8192, 'r');
    const uint32_t zero = 0;
    std::memcpy(written, &zero, sizeof(zero));
    return chain::success;
  }
  int32_t invoke_system_call(uint32_t id, char* out, uint32_t size, const char* args,
                            uint32_t n, uint32_t* written) override
  { return invoke_thunk(id, out, size, args, n, written); }
  int64_t get_meter_ticks() const override { return ticks; }
  void use_meter_ticks(uint64_t n) override { ticks -= n; }
};

void actual_vm_local_capacity()
{
  for (bool syscall : {false, true})
  {
    fv::fizzy_vm_backend backend;
    const auto code = callback(syscall, 17);
    CapacityHost warm; backend.run(warm, code, "");
    node::OwnedMemoryBudget memory(1 << 20);
    const auto before = memory.snapshot().accounted_bytes;
    CapacityHost refused; refused.refusal_budget = &memory;
    bool local_refusal = false;
    {
      node::OwnedMemoryBudget::Scope scope(memory);
      try { backend.run(refused, code, ""); }
      catch (const node::LocalCapacityExceeded& e)
      { local_refusal = std::strcmp(e.reason_code(), "mempool_memory_capacity") == 0; }
    }
    require(local_refusal && refused.calls == 1 && refused.retained.empty(),
            "real VM swallowed, reverted or terminated on native callback capacity refusal");
    CapacityHost retry;
    {
      node::OwnedMemoryBudget::Scope scope(memory);
      backend.run(retry, code, "");
    }
    require(retry.calls == 1 && retry.retained.size() == 8192 &&
            memory.snapshot().accounted_bytes > before + 8192,
            "real VM retry lost native callback ownership or failed to restore allocation capture");
    std::string released; released.swap(retry.retained);
    require(memory.snapshot().accounted_bytes > before + 8192,
            "moving VM callback output returned its credits before physical deletion");
    std::string().swap(released);
    require(memory.snapshot().accounted_bytes == before,
            "real VM callback output did not return its credits on final deletion");
  }
}
} // namespace
int main(int argc, char** argv)
{
  try
  {
    const std::string mode = argc == 3 && std::string(argv[1]) == "--case" ? argv[2] : "all";
    if (mode == "all" || mode == "thunk") direct_thunks();
    if (mode == "all" || mode == "host") native_hosts();
    if (mode == "system") native_hosts(true);
    if (mode == "all" || mode == "vm") { actual_vm(); actual_vm_local_capacity(); }
    require(mode == "all" || mode == "thunk" || mode == "host" || mode == "system" || mode == "vm", "unknown test case");
    return 0;
  }
  catch (const std::exception& error) { std::cerr << error.what() << '\n'; return 1; }
}
