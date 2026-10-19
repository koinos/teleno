#include "backup/direct_restore_cli.hpp"
#include "backup/direct_restore.hpp"
#include "core/storage_lease.hpp"

#include <csignal>
#include <limits>
#include <stdexcept>
#include <nlohmann/json.hpp>
#if defined(__unix__) || defined(__APPLE__)
#include <sys/stat.h>
#endif

namespace koinos::node::backup {
namespace {
namespace fs = std::filesystem;
using json = nlohmann::json;
volatile std::sig_atomic_t interrupted = 0;
void interrupt_handler(int) { interrupted = 1; }
class Signals
{
  using Handler = void(*)(int);
  Handler _int, _term;
public:
  Signals(): _int(std::signal(SIGINT,interrupt_handler)), _term(std::signal(SIGTERM,interrupt_handler))
  { interrupted = 0; }
  ~Signals() { std::signal(SIGINT,_int); std::signal(SIGTERM,_term); }
};
void require(bool ok) { if(!ok) throw std::runtime_error("unsupported direct CLI arguments or policy"); }
uint64_t allocated_bytes(const fs::path& root)
{
  uint64_t total = 0;
#if defined(__unix__) || defined(__APPLE__)
  if(!fs::exists(fs::symlink_status(root))) return 0;
  for(const auto& entry: fs::recursive_directory_iterator(root))
  {
    struct stat st{};
    require(::lstat(entry.path().c_str(),&st) == 0 && !S_ISLNK(st.st_mode) && st.st_blocks >= 0);
    require(uint64_t(st.st_blocks) <= (std::numeric_limits<uint64_t>::max() - total) / 512);
    total += uint64_t(st.st_blocks) * 512;
  }
#else
  (void)root; require(false);
#endif
  return total;
}
}

DirectRestoreCliResult run_direct_restore_cli(const DirectRestoreCliOptions& o)
{
  interrupted = 0;
  json report{{"format","teleno-direct-restore-cli"},{"version",1},{"mode","direct"},
    {"action",o.action},{"ok",false},{"attempt_state","unknown"},{"observer_hold",nullptr},
    {"ready_to_start_observer",false},{"capacity_40gb_qualified",false},
    {"limits",{{"metadata_file_bytes",8ULL*1024*1024},{"inventory_entries",100000},
      {"transfer_chunk_bytes",64*1024},{"maximum_retries",10},{"maximum_timeout_seconds",300}}},
    {"historical_replay_validated",false},{"runtime_admin_supported",false},
    {"limitations",{"trusted-backup structural validation, not historical execution",
      "Linux, power-loss, representative memory/disk and 40 GB qualification pending",
      "activation/recovery requires available pinned source metadata; no payload replay after validation",
      "SIGINT/SIGTERM cancellation is cooperative; kernel/filesystem I/O timing remains unqualified"}}};
  int code = 1; std::string phase = "arguments";
  try
  {
    require(o.arguments_valid && (o.action == "preflight" || o.action == "stage" || o.action == "activate"));
    require(!o.metadata.empty() && (o.action != "preflight" || (o.attempt.empty() && !o.resume)));
    require(o.action == "preflight" || (!o.attempt.empty() && !o.expected_chain_id.empty()));
    require(!o.resume || o.action == "stage");
    const auto reserve_profile = parse_restore_reserve_profile(o.reserve_profile);
    report["recovery_reserve_profile"] = restore_reserve_profile_name(reserve_profile);
    report["experimental_reserve"] = reserve_profile != RestoreReserveProfile::standard;
    phase = "configuration";
    require(fs::is_regular_file(o.config));
    auto cfg = load_config(o.config,{}, {},true);
    if(!o.source_url.empty()) { cfg.backup.public_restore.enabled = true; cfg.backup.public_restore.base_url = o.source_url; }
    auto& source = cfg.backup.public_restore;
    require(source.enabled && !source.base_url.empty());
    validate_direct_planning_paths(o.target,o.metadata,source.base_url);
    if(o.action != "preflight")
    {
      validate_direct_planning_paths(o.target,o.attempt,source.base_url);
      validate_direct_planning_paths(o.attempt,o.metadata,source.base_url);
      require(fs::absolute(o.target).lexically_normal().parent_path() == fs::absolute(o.attempt).lexically_normal().parent_path());
      // The engine cannot create an absent shared parent without changing the
      // target's ancestry. Require the operator to select an existing parent.
      require(fs::is_directory(fs::absolute(o.attempt).parent_path()));
    }
    std::string selected = o.backup_id;
    if(o.resume || o.action == "activate")
    {
      phase = "attempt_identity";
      const auto hint = json::parse(read_direct_restore_attempt_identity(o.attempt));
      require(hint.at("format") == "teleno-direct-restore-attempt" && hint.at("version") == 1);
      require(hint.at("target") == fs::absolute(o.target).lexically_normal().string()
        && hint.at("attempt_dir") == fs::absolute(o.attempt).lexically_normal().string());
      auto normalized_source = source.base_url;
      while(normalized_source.size() > 1 && normalized_source.back() == '/') normalized_source.pop_back();
      require(hint.at("source_identity") == normalized_source && hint.at("network") == source.network
        && hint.at("chain_id") == o.expected_chain_id && hint.at("expected_chain_checked") == true
        && hint.at("https_required") == source.require_https && hint.at("signature_required") == source.signature_required);
      require(hint.value("recovery_reserve_profile",std::string("standard")) == o.reserve_profile);
      const auto pinned = hint.at("backup_id").get<std::string>();
      // An explicit latest on resume is rejected, never interpreted as a new
      // selection. Omitted IDs use the untrusted hint, reauthenticated below.
      require(selected.empty() || (selected != "latest" && selected == pinned));
      selected = pinned;
    }
    Signals signals;
    DirectStageOptions options; options.transfer.cancel_requested = []{ return interrupted != 0; };
    options.reserve_profile = reserve_profile;
    phase = "metadata_authentication";
    auto spec = acquire_public_restore_specification(o.metadata,source,selected,o.expected_chain_id,options.transfer);
    const auto& d = spec.data();
    report["backup_id"] = d.backup_id; report["source_identity"] = d.source_identity;
    report["network"] = d.network; report["chain_id"] = d.chain_id;
    report["manifest_sha256"] = d.manifest_sha256; report["inventory_sha256"] = d.inventory_sha256;
    report["source_trust"] = {{"signature_required",d.signature_required},{"signature_verified",d.signature_verified},
      {"authenticated",d.signature_verified},{"expected_chain_checked",d.expected_chain_checked},
      {"https_required",source.require_https}};
    RestoreBudgetPolicy policy; policy.reserve_profile = reserve_profile;
    const auto budget = estimate_mode_restore_space(spec,RestoreMode::direct,policy);
    report["space_policy"] = {{"logical_payload_bytes",budget.logical_payload_bytes},
      {"modeled_allocated_payload_bytes",budget.allocated_payload_bytes},{"reserve_bytes",budget.reserve_bytes},
      {"metadata_journal_bytes",budget.metadata_journal_bytes},{"validation_allowance_bytes",budget.validation_allowance_bytes},
      {"recommended_free_bytes",budget.recommended_free_bytes},{"repository_cache_bytes",0}};
    if(o.action == "preflight")
    {
      phase = "preflight";
      auto plan = build_direct_restore_preflight(std::move(spec),o.target,o.metadata,reserve_profile);
      return {plan.ok() ? 0 : 2, o.json ? direct_restore_preflight_to_json(plan) : direct_restore_preflight_to_text(plan)};
    }
    report["attempt_dir"] = fs::absolute(o.attempt).lexically_normal().string();
    report["target"] = fs::absolute(o.target).lexically_normal().string();
    if(o.action == "stage")
    {
      phase = "staging";
      auto staged = stage_direct_restore_snapshot(spec,source,o.target,o.attempt,o.resume,options);
      report["attempt_id"] = staged.attempt_id; report["attempt_state"] = staged.state;
      // A returned stage is pre-validation. A refused/failed activation instead
      // keeps null: no claim about an existing target's hold was established.
      report["observer_hold"] = false;
      auto stage = json::parse(direct_stage_result_to_json(staged));
      // Arbitrary transport/path exception text must not enter user logs.
      stage["error"] = staged.download_complete ? "" : "transfer incomplete; preserve attempt and explicitly resume the same input";
      report["stage"] = stage;
      report["ok"] = staged.download_complete; code = staged.download_complete ? 0 : (interrupted ? 130 : 1);
    }
    else
    {
      phase = "attempt_identity";
      auto state = read_direct_restore_attempt_state(o.attempt);
      report["attempt_state"] = state;
      if(state == "download_complete" || state == "validating" || state == "verified")
      {
        phase = "offline_validation";
      }
      else
      {
        require(state == "activation_pending" || state == "activated");
        phase = "activation";
      }
      // Preserve phase diagnostics while the engine retains both ownership
      // locks across the complete validation/activation operation.
      options.checkpoint = [&](const std::string& point,uint64_t) {
        if(point == "before_activation") phase = "activation";
      };
      const auto activated = validate_and_activate_direct_restore_attempt(spec,o.target,o.attempt,options);
      report["attempt_id"] = activated.attempt_id; report["attempt_state"] = activated.state;
      report["observer_hold"] = activated.observer_hold;
      report["validation"] = json::parse(activated.validation_json);
      report["ready_to_start_observer"] = activated.observer_hold;
      report["ok"] = true; code = 0;
    }
    // A new startup can claim the gate after an engine call finishes. Metrics
    // are observational, never authority to open a changed DB or turn an
    // already completed activation into a failed activation.
    try
    {
      StorageLease measurement(o.target);
      report["measured_allocated_bytes"] = {{"attempt",allocated_bytes(o.attempt)},{"target",allocated_bytes(o.target)},
        {"sampling","after command; not peak usage"}};
      report["allocation_measurement_complete"] = true;
    }
    catch(...)
    {
      report["allocation_measurement_complete"] = false;
      report["measurement_warning"] = "Allocation sample unavailable; gate busy or filesystem changed after engine completion.";
    }
  }
  catch(const RestoreSourceError& error)
  {
    code = interrupted ? 130 : 1; report["ok"] = false; report["ready_to_start_observer"] = false;
    report["failure_phase"] = phase;
    report["failure_code"] = restore_source_failure_code(error.failure);
    report["transport_code"] = error.transport_code;
    report["error"] = "Source refused; resolve the reported source failure before restoring. No identity or trust check was bypassed.";
    if(!o.attempt.empty() && phase != "arguments" && phase != "configuration")
      try { report["attempt_state"] = read_direct_restore_attempt_state(o.attempt); } catch(...) {}
  }
  catch(...)
  {
    code = interrupted ? 130 : 1; report["ok"] = false; report["ready_to_start_observer"] = false;
    report["failure_phase"] = phase;
    report["error"] = "Direct restore refused or incomplete. Check the selected action, configuration, pinned identity, paths, reserve and attempt journal; preserve the sole DB. Raw exception details omitted.";
    if(!o.attempt.empty() && phase != "arguments" && phase != "configuration")
      try { report["attempt_state"] = read_direct_restore_attempt_state(o.attempt); } catch(...) {}
  }
  report["exit_code"] = code;
  report["cancelled"] = interrupted != 0;
  return {code,(o.json ? "" : "Direct restore CLI (no node or producer started)\n") + report.dump(2) + "\n"};
}
} // namespace koinos::node::backup
