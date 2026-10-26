#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <cstdint>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <utility>
#include <vector>

namespace fincore {

/// Checks a native capability dependency and throws when it is unavailable.
///
/// Native callers declare availability explicitly instead of importing an
/// optional Python module; an unavailable capability raises a structured
/// `DependencyError`. An empty `dependency` defaults to `module_name`, and an
/// empty `message` is replaced by an `optional_dependency_missing` message.
void load_optional_module(std::string_view module_name, bool available,
                          std::string_view dependency = {},
                          std::string_view extra = {},
                          std::string_view message = {});

/// Dense row-major numeric storage with explicit dimensions.
struct dense_array {
  std::vector<double> values;   ///< Flat row-major values.
  std::vector<std::size_t> shape;  ///< Dimension sizes.
};

/// Return an owning copy after checking shape, size and finite-value rules.
dense_array
validate_finite_array(const dense_array &value, std::string_view name = "value",
                      std::int64_t min_size = 1,
                      std::optional<std::int64_t> ndim = std::nullopt);

/// Return a copy of a typed string-keyed mapping after rejecting empty keys.
template <typename T, typename Compare, typename Allocator>
std::map<std::string, T, Compare, Allocator>
validate_mapping(const std::map<std::string, T, Compare, Allocator> &value,
                 std::string_view name) {
  for (const auto &[key, unused] : value) {
    static_cast<void>(unused);
    if (key.empty()) {
      throw input_error{std::string{name} + " keys must be non-empty strings"};
    }
  }
  return value;
}

/// Native generic dense-array operations corresponding to the pinned NumPy
/// backend. Inputs are copied and validated at each public boundary.
class native_backend {
public:
  /// Backend name reported to callers.
  static constexpr std::string_view name{"native"};

  /// Returns a validated owning copy of `value` as a dense double array.
  ///
  /// Throws `input_error` when the shape is empty or inconsistent with the
  /// value count, the dimension does not match `ndim`, the size is below one,
  /// or any value is non-finite.
  dense_array
  as_float_array(const dense_array &value, std::string_view name = "value",
                 std::optional<std::int64_t> ndim = std::nullopt) const;

  /// Converts an arithmetic vector and delegates to the dense-array overload.
  template <typename T>
    requires std::is_arithmetic_v<T>
  dense_array
  as_float_array(const std::vector<T> &value, std::string_view name = "value",
                 std::optional<std::int64_t> ndim = std::nullopt) const {
    std::vector<double> converted;
    converted.reserve(value.size());
    for (const auto item : value) {
      converted.push_back(static_cast<double>(item));
    }
    return as_float_array(dense_array{std::move(converted), {value.size()}},
                          name, ndim);
  }

  /// Returns the running cumulative product of a one-dimensional array.
  ///
  /// The result has the same shape as the validated input. Throws
  /// `input_error` when the input fails `as_float_array` validation.
  dense_array cumulative_product(const dense_array &value) const;
  /// Returns the sample standard deviation using Bessel's correction.
  ///
  /// Throws `input_error` when the input is invalid or holds fewer than two
  /// values.
  double sample_standard_deviation(const dense_array &value) const;

  /// Converts an arithmetic vector and returns its cumulative product.
  template <typename T>
    requires std::is_arithmetic_v<T>
  dense_array cumulative_product(const std::vector<T> &value) const {
    return cumulative_product(as_float_array(value, "value", 1));
  }

  /// Converts an arithmetic vector and returns its sample standard deviation.
  template <typename T>
    requires std::is_arithmetic_v<T>
  double sample_standard_deviation(const std::vector<T> &value) const {
    return sample_standard_deviation(as_float_array(value, "value", 1));
  }
};

/// Time-series index alignment strategy.
enum class alignment_policy {
  strict,       ///< Requires identical indices in every frame.
  inner,        ///< Keeps labels present in every frame.
  outer_dropna  ///< Keeps labels complete across all frames, dropping gaps.
};

/// Returns a UTC-normalized copy of a time-series frame.
///
/// Native timestamps are UTC nanoseconds. "UTC" normalization therefore
/// returns an owning copy; named-zone conversion is outside this API. Throws
/// `input_error` when `normalize_tz` is not UTC, compared case-insensitively.
dataframe normalize_time_series_timezone(const dataframe &value,
                                         std::string_view normalize_tz);

/// Aligns double-column frames by their UTC-nanosecond index.
///
/// All column names and order are retained. Unsupported column types and
/// duplicate indices are rejected. Non-double columns are not coerced. Throws
/// `input_error` when the policy is unknown or strict alignment finds
/// different indices.
std::vector<dataframe> align_time_series(const std::vector<dataframe> &values,
                                         alignment_policy policy);

/// Aligns two labelled frames under the same policy as align_time_series.
///
/// Throws `input_error` for the same invalid inputs as `align_time_series`.
std::pair<dataframe, dataframe>
align_binary_metric_inputs(const dataframe &left, const dataframe &right,
                           alignment_policy policy = alignment_policy::inner);

/// Aligns positional arrays by first-axis length.
///
/// This overload deliberately cannot accept a labelled frame on either side.
/// The arrays are returned unchanged after validation; throws `input_error`
/// for an unknown policy, an inconsistent shape, or unequal first-axis
/// lengths.
std::pair<dense_array, dense_array>
align_binary_metric_inputs(const dense_array &left, const dense_array &right,
                           alignment_policy policy = alignment_policy::inner);

} // namespace fincore
