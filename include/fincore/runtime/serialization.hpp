#pragma once

#include <fincore/runtime/runtime.hpp>

#include <map>
#include <string>
#include <string_view>

namespace fincore {

/// Current runtime result payload schema version.
inline constexpr int runtime_result_schema_version = 1;

/// Owning, versioned representation of a runtime result.
///
/// Copies own their DataFrame buffers and all nested report/risk values.
struct runtime_result_payload {
  int schema_version = runtime_result_schema_version;  ///< Payload schema version.
  runtime_value value;                                 ///< Result value.
  std::map<std::string, std::string> metadata;         ///< Result metadata.
};

/// Converts a runtime result into an owning versioned payload.
runtime_result_payload to_payload(const runtime_result &result);
/// Validates a payload and returns an owning runtime result.
///
/// Supported values and nested shapes are checked. Throws `input_error` for an
/// unsupported schema version or an invalid value representation.
runtime_result from_payload(const runtime_result_payload &payload);

/// Serializes a runtime result to deterministic JSON.
///
/// The JSON envelope is deterministic for a given native runtime_result.
/// Numeric values use explicit tags so non-finite values and signed zero
/// survive. Throws `input_error` for values that cannot be represented.
std::string to_json(const runtime_result &result);
/// Parses a JSON envelope into a runtime result.
///
/// Throws `input_error` for malformed JSON, duplicate object keys, invalid
/// UTF-8, unsupported schema versions, or out-of-range numbers.
runtime_result from_json(std::string_view json);

/// Returns a versioned semantic digest of a runtime result.
///
/// The digest is SHA-256 over the canonical native payload, domain-separated
/// and versioned. Volatile metadata such as run durations, run identifiers and
/// path-valued keys is excluded so equivalent results compare equal.
std::string semantic_digest(const runtime_result &result);

/// Returns an owning value-semantic copy of a runtime result.
runtime_result copy_for_consumer(const runtime_result &result);
/// Returns a copy with the supplied metadata keys overwritten.
///
/// Keys present in `metadata` replace existing values; all other metadata
/// from `result` is retained.
runtime_result
with_metadata(const runtime_result &result,
              const std::map<std::string, std::string> &metadata);

} // namespace fincore
