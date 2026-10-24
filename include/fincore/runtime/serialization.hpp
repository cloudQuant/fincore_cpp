#pragma once

#include <fincore/runtime/runtime.hpp>

#include <map>
#include <string>
#include <string_view>

namespace fincore {

inline constexpr int runtime_result_schema_version = 1;

// An owning, versioned representation of a runtime result. Copies own their
// DataFrame buffers and all nested report/risk values.
struct runtime_result_payload {
  int schema_version = runtime_result_schema_version;
  runtime_value value;
  std::map<std::string, std::string> metadata;
};

runtime_result_payload to_payload(const runtime_result &result);
// Validates supported values and nested shapes while returning an owning copy.
runtime_result from_payload(const runtime_result_payload &payload);

// The JSON envelope is deterministic for a given native runtime_result.
// Numeric values use explicit tags so non-finite values and signed zero
// survive.
std::string to_json(const runtime_result &result);
runtime_result from_json(std::string_view json);

// SHA-256 over the canonical native payload, domain-separated and versioned.
std::string semantic_digest(const runtime_result &result);

// Return value-semantic copies. with_metadata overwrites keys present in the
// supplied map while retaining all other metadata from the original result.
runtime_result copy_for_consumer(const runtime_result &result);
runtime_result
with_metadata(const runtime_result &result,
              const std::map<std::string, std::string> &metadata);

} // namespace fincore
