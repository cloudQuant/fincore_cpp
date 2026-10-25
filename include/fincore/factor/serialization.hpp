#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/factor/analysis.hpp>

#include <cstdint>
#include <initializer_list>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <utility>
#include <variant>
#include <vector>

namespace fincore {

struct factor_analysis_model;
struct research_result;

/// Deterministic native value model used by factor serialization.
///
/// Objects are ordered maps, so both JSON output and fingerprints are stable
/// regardless of insertion order. Doubles keep their IEEE-754 bit pattern in
/// the canonical encoding; NaN payloads are canonicalized to one quiet NaN.
class factor_value {
public:
    /// Ordered array of values.
    using array_type = std::vector<factor_value>;
    /// Ordered map from string key to value.
    using object_type = std::map<std::string, factor_value>;

    /// Constructs a null value.
    factor_value() = default;
    /// Constructs a null value from `nullptr`.
    factor_value(std::nullptr_t) {}
    /// Constructs a boolean value.
    factor_value(bool value) : storage_(value) {}
    /// Constructs a 64-bit integer value from `int`.
    factor_value(int value) : storage_(static_cast<std::int64_t>(value)) {}
    /// Constructs a 64-bit integer value.
    factor_value(std::int64_t value) : storage_(value) {}
    /// Constructs a floating-point number value.
    factor_value(double value) : storage_(value) {}
    /// Constructs a string value, taking ownership of `value`.
    factor_value(std::string value) : storage_(std::move(value)) {}
    /// Constructs a string value by copying `value`.
    factor_value(const char* value) : storage_(std::string(value)) {}
    /// Constructs an array value, taking ownership of `value`.
    factor_value(array_type value) : storage_(std::move(value)) {}
    /// Constructs an object value, taking ownership of `value`.
    factor_value(object_type value) : storage_(std::move(value)) {}

    /// Returns whether the value is null.
    bool is_null() const noexcept {
        return std::holds_alternative<std::monostate>(storage_);
    }
    /// Returns whether the value holds a boolean.
    bool is_bool() const noexcept { return std::holds_alternative<bool>(storage_); }
    /// Returns whether the value holds a 64-bit integer.
    bool is_integer() const noexcept {
        return std::holds_alternative<std::int64_t>(storage_);
    }
    /// Returns whether the value holds a floating-point number; integers are
    /// a separate alternative.
    bool is_number() const noexcept { return std::holds_alternative<double>(storage_); }
    /// Returns whether the value holds a string.
    bool is_string() const noexcept {
        return std::holds_alternative<std::string>(storage_);
    }
    /// Returns whether the value holds an array.
    bool is_array() const noexcept {
        return std::holds_alternative<array_type>(storage_);
    }
    /// Returns whether the value holds an object.
    bool is_object() const noexcept {
        return std::holds_alternative<object_type>(storage_);
    }

    /// Returns the stored boolean.
    /// @throws std::bad_variant_access If the value holds another type.
    bool as_bool() const { return std::get<bool>(storage_); }
    /// Returns the stored 64-bit integer.
    /// @throws std::bad_variant_access If the value holds another type.
    std::int64_t as_integer() const { return std::get<std::int64_t>(storage_); }
    /// Returns the stored floating-point number.
    /// @throws std::bad_variant_access If the value holds another type.
    double as_number() const { return std::get<double>(storage_); }
    /// Returns the stored string.
    /// @throws std::bad_variant_access If the value holds another type.
    const std::string& as_string() const { return std::get<std::string>(storage_); }
    /// Returns the stored array.
    /// @throws std::bad_variant_access If the value holds another type.
    const array_type& as_array() const { return std::get<array_type>(storage_); }
    /// Returns the stored object.
    /// @throws std::bad_variant_access If the value holds another type.
    const object_type& as_object() const { return std::get<object_type>(storage_); }
    /// Returns the stored array for mutation.
    /// @throws std::bad_variant_access If the value holds another type.
    array_type& as_array() { return std::get<array_type>(storage_); }
    /// Returns the stored object for mutation.
    /// @throws std::bad_variant_access If the value holds another type.
    object_type& as_object() { return std::get<object_type>(storage_); }

    /// Returns the value stored under `key`.
    ///
    /// @param key Object key to look up.
    /// @return Reference to the stored value.
    /// @throws input_error If the value is not an object or `key` is absent.
    const factor_value& at(std::string_view key) const;
    /// Finds the value stored under `key`.
    ///
    /// @param key Object key to look up.
    /// @return Pointer to the stored value, or nullptr when the value is not
    ///     an object or `key` is absent.
    const factor_value* find(std::string_view key) const noexcept;

    friend bool operator==(const factor_value&, const factor_value&) = default;

private:
    std::variant<std::monostate, bool, std::int64_t, double, std::string,
                 array_type, object_type>
        storage_;
};

/// Serializes a native value as deterministic, standards-compliant JSON text.
///
/// Object keys are emitted in sorted order, so the text is stable for a given
/// value.
///
/// @param value Value to serialize.
/// @return Deterministic JSON text.
/// @throws input_error If the value contains a non-finite floating-point
///     number or a string that is not valid UTF-8.
std::string to_json_string(const factor_value& value);

/// Parses strict JSON into a native value.
///
/// Integers are returned as 64-bit integer values where they fit, and other
/// numbers as doubles. Object keys must be strings.
///
/// @param text JSON text to parse.
/// @return Parsed native value.
/// @throws input_error If the text is malformed, has trailing content, uses an
///     unsupported escape, or contains a number outside the finite double
///     range.
factor_value parse_json_value(std::string_view text);

/// Converts a factor-analysis model into its native envelope value.
///
/// The envelope carries the schema identifier, fingerprint version, result
/// fingerprint, configuration, forward periods, loss report, quantile
/// statistics and tables, IC tables, mean-return spread, turnover and
/// rank-autocorrelation series, and optional event analysis. Non-finite entries
/// in the IC, quantile, turnover, rank-autocorrelation and event arrays are
/// encoded as null.
///
/// @param model Model to serialize.
/// @return Native object value for the model.
factor_value serializable_value(const factor_analysis_model& model);
/// Converts a loss report into a native object value.
///
/// @param report Loss report to serialize.
/// @return Native object with the report counters and loss values.
factor_value serializable_value(const factor_loss_report& report);

/// Returns the JSON text form of ``serializable_value(model)``.
///
/// This is the native equivalent of the Python ``to_serializable`` method.
///
/// @param model Model to serialize.
/// @return Deterministic JSON text for the model envelope.
/// @throws input_error If a value cannot be represented as JSON, such as a
///     non-finite loss-report number.
std::string to_serializable(const factor_analysis_model& model);

/// Parses serialized model text into a native value.
///
/// The text is parsed without checking the envelope schema; use
/// `deserialize_factor_model` to validate and restore the supported subset.
///
/// @param text Serialized JSON text.
/// @return Parsed native value.
/// @throws input_error If the text is not valid strict JSON.
factor_value deserialize_serializable_value(std::string_view text);

/// The subset of a serialized model that the native envelope can restore.
struct factor_model_envelope {
    /// Serialization schema identifier; the value written and accepted by
    /// this library is ``fincore-factor-analysis-native-v1``.
    std::string schema;
    /// Result-fingerprint format version stored in the envelope.
    std::uint32_t fingerprint_version = 0U;
    /// Result fingerprint restored from the envelope.
    std::string result_fingerprint;
    /// Factor-analysis configuration restored from the envelope.
    factor_analysis_config config;
    /// Forward-return period labels restored from the envelope.
    std::vector<std::string> forward_periods;
    /// Data loss report restored from the envelope.
    factor_loss_report loss_report;
};

/// Restores the supported model envelope from serialized JSON text.
///
/// The root must be an object carrying ``schema``, ``fingerprint_version``,
/// ``result_fingerprint``, ``config``, ``forward_periods`` and
/// ``loss_report``. The schema must equal the supported version, and the
/// fingerprint version must fit in 32 bits. Additional keys are ignored.
///
/// @param text Serialized JSON text.
/// @return Envelope with the schema, fingerprint fields, configuration,
///     forward periods and loss report.
/// @throws input_error If the text is not valid JSON, the root is not an
///     object, a required field is missing, the schema is unsupported, or the
///     fingerprint version is out of range.
factor_model_envelope deserialize_factor_model(std::string_view text);

/// Returns a stable SHA-256 digest over the canonical native encoding.
///
/// The digest covers `detail::canonical_factor_value(value)`, is 64 lowercase
/// hexadecimal characters, and is independent of object insertion order and
/// NaN payloads.
///
/// @param value Value to fingerprint.
/// @return Lowercase hexadecimal SHA-256 digest.
std::string fingerprint_value(const factor_value& value);
/// Returns the fingerprint of `serializable_value(model)`.
///
/// @param model Model to fingerprint.
/// @return Lowercase hexadecimal SHA-256 digest.
std::string fingerprint_value(const factor_analysis_model& model);
/// Returns the fingerprint of `serializable_value(report)`.
///
/// @param report Loss report to fingerprint.
/// @return Lowercase hexadecimal SHA-256 digest.
std::string fingerprint_value(const factor_loss_report& report);
/// Returns the fingerprint of the serialized research result.
///
/// @param result Research result to fingerprint.
/// @return Lowercase hexadecimal SHA-256 digest.
std::string fingerprint_value(const research_result& result);

/// Immutable ordered map. Construction snapshots the supplied map; callers
/// cannot mutate the stored entries and mutation of the source has no effect.
template <typename Value>
class frozen_mapping {
public:
    /// Ordered map type used to store the entries.
    using storage_type = std::map<std::string, Value>;
    /// Read-only iterator over the stored entries.
    using const_iterator = typename storage_type::const_iterator;

    /// Constructs an empty mapping.
    frozen_mapping() = default;
    /// Constructs a mapping that takes ownership of `values`.
    ///
    /// @param values Entries to store; the map is moved into the snapshot.
    explicit frozen_mapping(storage_type values) : values_(std::move(values)) {}
    /// Constructs a mapping from an initializer list.
    ///
    /// @param values Entries to copy into the mapping.
    frozen_mapping(std::initializer_list<typename storage_type::value_type> values)
        : values_(values) {}

    /// Returns the value stored under `key`.
    ///
    /// @param key Entry key to look up.
    /// @return Reference to the stored value.
    /// @throws std::out_of_range If `key` is absent.
    const Value& at(const std::string& key) const { return values_.at(key); }
    /// Finds the value stored under `key`.
    ///
    /// @param key Entry key to look up.
    /// @return Pointer to the stored value, or nullptr when `key` is absent.
    const Value* find(const std::string& key) const noexcept {
        const auto found = values_.find(key);
        return found == values_.end() ? nullptr : &found->second;
    }
    /// Returns whether `key` is present.
    ///
    /// @param key Entry key to look up.
    bool contains(const std::string& key) const noexcept {
        return values_.contains(key);
    }
    /// Returns the number of entries.
    std::size_t size() const noexcept { return values_.size(); }
    /// Returns whether the mapping has no entries.
    bool empty() const noexcept { return values_.empty(); }
    /// Returns an iterator to the first entry in key order.
    const_iterator begin() const noexcept { return values_.begin(); }
    /// Returns the past-the-end iterator.
    const_iterator end() const noexcept { return values_.end(); }
    /// Returns a mutable copy of the stored map.
    storage_type snapshot() const { return values_; }

    friend bool operator==(const frozen_mapping&, const frozen_mapping&) = default;

private:
    storage_type values_;
};

/// Owned native dataframe snapshot (replacing the pandas pickle snapshot).
class dataframe_snapshot {
public:
    /// Constructs an empty snapshot with an empty digest.
    dataframe_snapshot() = default;
    /// Snapshots `value` and computes its content digest.
    ///
    /// @param value Dataframe to take ownership of.
    /// @throws fincore_error If a column type cannot be encoded in the
    ///     canonical content representation.
    explicit dataframe_snapshot(dataframe value);

    /// Returns the owned dataframe.
    const dataframe& data() const noexcept { return data_; }
    /// Returns a copy of the owned dataframe.
    dataframe copy() const { return data_; }
    /// Returns the lowercase hexadecimal content digest; empty for a
    /// default-constructed snapshot.
    const std::string& content_sha256() const noexcept { return content_sha256_; }

private:
    dataframe data_;
    std::string content_sha256_;
};

/// Snapshots a native dataframe and computes its deterministic content digest.
///
/// @param value Dataframe to copy.
/// @return Snapshot owning an independent copy of `value`.
/// @throws fincore_error If a column type cannot be encoded in the canonical
///     content representation.
dataframe_snapshot snapshot_pandas(const dataframe& value);
/// Snapshots a native dataframe by taking ownership of its storage.
///
/// @param value Dataframe to move.
/// @return Snapshot owning `value`.
/// @throws fincore_error If a column type cannot be encoded in the canonical
///     content representation.
dataframe_snapshot snapshot_pandas(dataframe&& value);

namespace detail {

/// Encodes a value into the canonical binary form used by `fingerprint_value`.
///
/// The encoding starts with a fixed domain tag, writes integers and lengths in
/// big-endian order, sorts object keys and canonicalizes NaN payloads.
///
/// @param value Value to encode.
/// @return Canonical byte string used as the SHA-256 input.
std::string canonical_factor_value(const factor_value& value);

}  // namespace detail

}  // namespace fincore
