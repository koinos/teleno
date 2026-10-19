#include <google/protobuf/unknown_field_set.h>
#include <google/protobuf/any.pb.h>
#include <google/protobuf/arena.h>

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <new>
#include <string>

// Ordinary C++ allocation failures on this test thread. This does not model
// machine-wide OOM or account for dependency malloc/aligned allocations.
static constinit std::atomic<long> live_allocations{0};
static thread_local long fail_at = -1;
static thread_local std::size_t failed_request = 0;
static thread_local bool injected = false;

void* operator new(std::size_t n) {
  if (fail_at >= 0 && fail_at-- == 0) {
    fail_at = -1;
    failed_request = n;
    injected = true;
    throw std::bad_alloc();
  }
  if (auto* p = std::malloc(n ? n : 1)) {
    ++live_allocations;
    return p;
  }
  throw std::bad_alloc();
}
void* operator new[](std::size_t n) { return ::operator new(n); }
void operator delete(void* p) noexcept {
  if (p) --live_allocations;
  std::free(p);
}
void operator delete[](void* p) noexcept { ::operator delete(p); }
void operator delete(void* p, std::size_t) noexcept { ::operator delete(p); }
void operator delete[](void* p, std::size_t) noexcept { ::operator delete(p); }

namespace {
using google::protobuf::UnknownFieldSet;

bool fault_case(bool group, unsigned prefix, unsigned ordinal) {
  const auto before = live_allocations.load();
  bool valid = true;
  injected = false;
  {
    UnknownFieldSet fields;
    for (unsigned i = 0; i < prefix; ++i) fields.AddVarint(i + 1, i + 17);
    bool caught = false;
    fail_at = ordinal;
    try {
      if (group) fields.AddGroup(100)->AddVarint(101, 1234);
      else fields.AddLengthDelimited(100)->assign(4096, 's');
    } catch (const std::bad_alloc&) {
      caught = true;
    }
    fail_at = -1;
    valid &= caught == injected;
    for (unsigned i = 0; i < prefix; ++i)
      valid &= fields.field(i).number() == static_cast<int>(i + 1) &&
               fields.field(i).varint() == i + 17;
    if (!caught) {
      valid &= fields.field_count() == static_cast<int>(prefix + 1);
      const auto& field = fields.field(prefix);
      valid &= field.number() == 100;
      if (group)
        valid &= field.group().field_count() == 1 &&
                 field.group().field(0).number() == 101 &&
                 field.group().field(0).varint() == 1234;
      else
        valid &= field.length_delimited() == std::string(4096, 's');
    }
  }
  return valid && live_allocations.load() == before;
}

bool roundtrip() {
  google::protobuf::Any original, parsed;
  auto* fields = original.GetReflection()->MutableUnknownFields(&original);
  fields->AddVarint(11, 123456);
  fields->AddFixed32(12, 0xfedcba98);
  fields->AddFixed64(13, 0xfedcba9876543210ULL);
  fields->AddLengthDelimited(14, std::string("future\0value", 12));
  fields->AddGroup(15)->AddLengthDelimited(16, std::string(4096, 'g'));
  fields->AddVarint(11, 789); // Preserve repeated fields and insertion order.
  const auto wire = original.SerializeAsString();
  return parsed.ParseFromString(wire) && parsed.SerializeAsString() == wire;
}

// Refuse the payload copy after the arena has registered string cleanup. The
// source bytes and default field must survive, and arena destruction must not
// destroy an object whose throwing constructor never completed.
bool arena_string_copy_failure(unsigned ordinal, bool& refused_payload) {
  const auto before = live_allocations.load();
  bool valid = true;
  {
    std::string source(8192, 'p');
    source[37] = '\0';
    const auto expected = source;
    bool caught = false;
    injected = false;
    failed_request = 0;
    {
      google::protobuf::Arena arena;
      auto* value = google::protobuf::Arena::CreateMessage<google::protobuf::Any>(&arena);
      fail_at = ordinal;
      try { value->set_type_url(source); }
      catch (const std::bad_alloc&) { caught = true; }
      fail_at = -1;
      refused_payload = caught && failed_request >= source.size() + 1;
      valid &= caught == injected && source == expected;
      valid &= caught ? value->type_url().empty() : value->type_url() == expected;
      // The field must remain usable after refusal; successful copies preserve
      // embedded NULs and the exact protobuf wire representation.
      value->set_type_url(source);
      google::protobuf::Any ordinary, parsed;
      ordinary.set_type_url(source);
      const auto wire = value->SerializeAsString();
      valid &= wire == ordinary.SerializeAsString() && parsed.ParseFromString(wire) &&
               parsed.type_url() == expected;
    }
  }
  return valid && live_allocations.load() == before;
}
} // namespace

int main() {
  unsigned cases = 0;
  for (bool group : {false, true}) {
    for (unsigned prefix : {0U, 8U}) {
      bool reached_success = false;
      for (unsigned ordinal = 0; ordinal < 64; ++ordinal) {
        ++cases;
        if (!fault_case(group, prefix, ordinal)) {
          std::fprintf(stderr, "Unknown-field ownership failed: group=%u prefix=%u ordinal=%u\n",
                       group, prefix, ordinal);
          return 1;
        }
        if (!injected) { reached_success = true; break; }
      }
      if (!reached_success) return 2;
    }
  }
  if (!roundtrip()) return 3;
  bool reached_arena_success = false, saw_payload_refusal = false;
  unsigned arena_cases = 0;
  for (unsigned ordinal = 0; ordinal < 64; ++ordinal) {
    ++arena_cases;
    bool refused_payload = false;
    if (!arena_string_copy_failure(ordinal, refused_payload)) return 4;
    saw_payload_refusal |= refused_payload;
    if (!injected) { reached_arena_success = true; break; }
  }
  if (!reached_arena_success || !saw_payload_refusal) return 5;
  std::printf("Protobuf ownership: %u unknown-field fault cases, %u arena copy cases and byte-exact roundtrips passed\n", cases, arena_cases);
}
