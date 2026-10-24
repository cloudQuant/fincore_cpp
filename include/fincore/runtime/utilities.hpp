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

/// Native capability-boundary dependency check. Python imports an optional
/// module here; native callers declare availability explicitly and receive a
/// structured DependencyError when the capability is unavailable.
void load_optional_module(std::string_view module_name, bool available,
                          std::string_view dependency = {},
                          std::string_view extra = {},
                          std::string_view message = {});

/// Dense row-major numeric storage with explicit dimensions.
struct dense_array {
  std::vector<double> values;
  std::vector<std::size_t> shape;
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
  static constexpr std::string_view name{"native"};

  dense_array
  as_float_array(const dense_array &value, std::string_view name = "value",
                 std::optional<std::int64_t> ndim = std::nullopt) const;

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

  dense_array cumulative_product(const dense_array &value) const;
  double sample_standard_deviation(const dense_array &value) const;

  template <typename T>
    requires std::is_arithmetic_v<T>
  dense_array cumulative_product(const std::vector<T> &value) const {
    return cumulative_product(as_float_array(value, "value", 1));
  }

  template <typename T>
    requires std::is_arithmetic_v<T>
  double sample_standard_deviation(const std::vector<T> &value) const {
    return sample_standard_deviation(as_float_array(value, "value", 1));
  }
};

enum class alignment_policy { strict, inner, outer_dropna };

/// Native timestamps are UTC nanoseconds. "UTC" normalization therefore
/// returns an owning copy; named-zone conversion is outside this API.
dataframe normalize_time_series_timezone(const dataframe &value,
                                         std::string_view normalize_tz);

/// Align double-column frames by their UTC-nanosecond index. All column names
/// and order are retained. Unsupported column types and duplicate indices are
/// rejected. Non-double columns are not coerced.
std::vector<dataframe> align_time_series(const std::vector<dataframe> &values,
                                         alignment_policy policy);

/// Align two labelled frames under the same policy as align_time_series.
std::pair<dataframe, dataframe>
align_binary_metric_inputs(const dataframe &left, const dataframe &right,
                           alignment_policy policy = alignment_policy::inner);

/// Align positional arrays by first-axis length. This overload deliberately
/// cannot accept a labelled frame on either side.
std::pair<dense_array, dense_array>
align_binary_metric_inputs(const dense_array &left, const dense_array &right,
                           alignment_policy policy = alignment_policy::inner);

} // namespace fincore
