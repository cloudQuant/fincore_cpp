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
    using array_type = std::vector<factor_value>;
    using object_type = std::map<std::string, factor_value>;

    factor_value() = default;
    factor_value(std::nullptr_t) {}
    factor_value(bool value) : storage_(value) {}
    factor_value(int value) : storage_(static_cast<std::int64_t>(value)) {}
    factor_value(std::int64_t value) : storage_(value) {}
    factor_value(double value) : storage_(value) {}
    factor_value(std::string value) : storage_(std::move(value)) {}
    factor_value(const char* value) : storage_(std::string(value)) {}
    factor_value(array_type value) : storage_(std::move(value)) {}
    factor_value(object_type value) : storage_(std::move(value)) {}

    bool is_null() const noexcept {
        return std::holds_alternative<std::monostate>(storage_);
    }
    bool is_bool() const noexcept { return std::holds_alternative<bool>(storage_); }
    bool is_integer() const noexcept {
        return std::holds_alternative<std::int64_t>(storage_);
    }
    bool is_number() const noexcept { return std::holds_alternative<double>(storage_); }
    bool is_string() const noexcept {
        return std::holds_alternative<std::string>(storage_);
    }
    bool is_array() const noexcept {
        return std::holds_alternative<array_type>(storage_);
    }
    bool is_object() const noexcept {
        return std::holds_alternative<object_type>(storage_);
    }

    bool as_bool() const { return std::get<bool>(storage_); }
    std::int64_t as_integer() const { return std::get<std::int64_t>(storage_); }
    double as_number() const { return std::get<double>(storage_); }
    const std::string& as_string() const { return std::get<std::string>(storage_); }
    const array_type& as_array() const { return std::get<array_type>(storage_); }
    const object_type& as_object() const { return std::get<object_type>(storage_); }
    array_type& as_array() { return std::get<array_type>(storage_); }
    object_type& as_object() { return std::get<object_type>(storage_); }

    const factor_value& at(std::string_view key) const;
    const factor_value* find(std::string_view key) const noexcept;

    friend bool operator==(const factor_value&, const factor_value&) = default;

private:
    std::variant<std::monostate, bool, std::int64_t, double, std::string,
                 array_type, object_type>
        storage_;
};

/// Serialize a native value as a deterministic, standards-compliant JSON text.
std::string to_json_string(const factor_value& value);

/// Parse strict JSON into a native value. Throws ``input_error`` on malformed
/// input, trailing content or unsupported escapes.
factor_value parse_json_value(std::string_view text);

/// Convert a factor-analysis model (or loss report) into its native value.
factor_value serializable_value(const factor_analysis_model& model);
factor_value serializable_value(const factor_loss_report& report);

/// JSON text form of ``serializable_value(model)`` (Python ``to_serializable``).
std::string to_serializable(const factor_analysis_model& model);

/// Parse a serialized model envelope. Unsupported schema versions are rejected.
factor_value deserialize_serializable_value(std::string_view text);

/// The subset of a serialized model that the native envelope can restore.
struct factor_model_envelope {
    std::string schema;
    std::uint32_t fingerprint_version = 0U;
    std::string result_fingerprint;
    factor_analysis_config config;
    std::vector<std::string> forward_periods;
    factor_loss_report loss_report;
};

factor_model_envelope deserialize_factor_model(std::string_view text);

/// Stable SHA-256 over the canonical native encoding of ``value``.
std::string fingerprint_value(const factor_value& value);
std::string fingerprint_value(const factor_analysis_model& model);
std::string fingerprint_value(const factor_loss_report& report);
std::string fingerprint_value(const research_result& result);

/// Immutable ordered map. Construction snapshots the supplied map; callers
/// cannot mutate the stored entries and mutation of the source has no effect.
template <typename Value>
class frozen_mapping {
public:
    using storage_type = std::map<std::string, Value>;
    using const_iterator = typename storage_type::const_iterator;

    frozen_mapping() = default;
    explicit frozen_mapping(storage_type values) : values_(std::move(values)) {}
    frozen_mapping(std::initializer_list<typename storage_type::value_type> values)
        : values_(values) {}

    const Value& at(const std::string& key) const { return values_.at(key); }
    const Value* find(const std::string& key) const noexcept {
        const auto found = values_.find(key);
        return found == values_.end() ? nullptr : &found->second;
    }
    bool contains(const std::string& key) const noexcept {
        return values_.contains(key);
    }
    std::size_t size() const noexcept { return values_.size(); }
    bool empty() const noexcept { return values_.empty(); }
    const_iterator begin() const noexcept { return values_.begin(); }
    const_iterator end() const noexcept { return values_.end(); }
    storage_type snapshot() const { return values_; }

    friend bool operator==(const frozen_mapping&, const frozen_mapping&) = default;

private:
    storage_type values_;
};

/// Owned native dataframe snapshot (replacing the pandas pickle snapshot).
class dataframe_snapshot {
public:
    dataframe_snapshot() = default;
    explicit dataframe_snapshot(dataframe value);

    const dataframe& data() const noexcept { return data_; }
    dataframe copy() const { return data_; }
    const std::string& content_sha256() const noexcept { return content_sha256_; }

private:
    dataframe data_;
    std::string content_sha256_;
};

/// Snapshot a native dataframe and compute its deterministic content digest.
dataframe_snapshot snapshot_pandas(const dataframe& value);
dataframe_snapshot snapshot_pandas(dataframe&& value);

namespace detail {

/// Canonical binary encoding consumed by ``fingerprint_value``.
std::string canonical_factor_value(const factor_value& value);

}  // namespace detail

}  // namespace fincore
