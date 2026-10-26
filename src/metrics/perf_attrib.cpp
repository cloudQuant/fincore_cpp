#include <fincore/metrics/perf_attrib.hpp>

#include <fincore/metrics/ratios.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <typeindex>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct double_column {
  std::string name;
  std::span<const double> values;
};

struct output_column {
  std::string name;
  std::vector<double> values;
};

using loading_key = std::pair<timestamp, std::string>;

std::string checked_name(const std::string_view name, const char *message) {
  if (name.empty()) {
    throw input_error{message};
  }
  return std::string{name};
}

std::span<const std::string> require_string_column(const dataframe &frame,
                                                   const std::string &name) {
  if (!frame.has_column(name.c_str())) {
    throw input_error{"required string column is absent"};
  }
  const auto columns = frame.get_columns_info<std::string>();
  const auto column = std::ranges::find_if(columns, [&name](const auto &info) {
    return std::get<0>(info) == name.c_str() &&
           std::get<2>(info) == typeid(std::string);
  });
  if (column == columns.end()) {
    throw input_error{"required column is not a string column"};
  }
  const auto &values = frame.get_column<std::string>(name.c_str());
  if (values.size() != frame.get_index().size()) {
    throw input_error{
        "required string column must match the frame index length"};
  }
  return values;
}

// The cash column is validated and then left out of the asset list on purpose: it
// has no factor exposure to attribute, and it need not exist at all, so the check
// is there only to reject a cash column of the wrong type.
std::vector<double_column>
position_assets(const dataframe &positions,
                const std::string_view cash_column_name) {
  const std::string cash_name =
      checked_name(cash_column_name, "cash column name must not be empty");
  if (positions.has_column(cash_name.c_str())) {
    static_cast<void>(require_double_column(positions, cash_name));
  }

  const auto columns = positions.get_columns_info<double>();
  const auto &index = positions.get_index();
  std::vector<double_column> assets;
  assets.reserve(columns.size());
  for (const auto &info : columns) {
    if (std::get<2>(info) != typeid(double)) {
      continue;
    }
    std::string name{std::get<0>(info).c_str()};
    if (name.empty()) {
      throw input_error{"position asset column names must not be empty"};
    }
    const auto &values = positions.get_column<double>(name.c_str());
    if (values.size() != index.size()) {
      throw input_error{
          "position double columns must match the frame index length"};
    }
    if (name != cash_name) {
      assets.push_back({std::move(name), values});
    }
  }
  if (assets.empty()) {
    throw input_error{
        "positions must contain at least one double asset column"};
  }
  return assets;
}

std::vector<std::string> checked_factor_names(
    const std::span<const std::string_view> factor_column_names,
    const std::string_view symbol_column_name) {
  if (factor_column_names.empty()) {
    throw input_error{"factor column names must not be empty"};
  }
  const std::string symbol_name =
      checked_name(symbol_column_name, "symbol column name must not be empty");
  std::vector<std::string> names;
  names.reserve(factor_column_names.size());
  for (const auto name : factor_column_names) {
    const std::string factor_name = checked_name(
        name, "factor column names must not contain an empty name");
    if (factor_name == symbol_name ||
        std::ranges::find(names, factor_name) != names.end()) {
      throw input_error{"factor column names must be distinct"};
    }
    names.push_back(factor_name);
  }
  return names;
}

dataframe make_frame(const std::vector<timestamp> &index,
                     std::vector<output_column> columns) {
  dataframe result;
  auto output_index = index;
  result.load_index(std::move(output_index));
  for (auto &column : columns) {
    result.load_column<double>(column.name.c_str(), std::move(column.values),
                               hmdf::nan_policy::dont_pad_with_nans);
  }
  return result;
}

void require_unique_index(const dataframe &frame, const char *message) {
  std::unordered_set<timestamp> seen;
  seen.reserve(frame.get_index().size());
  for (const timestamp value : frame.get_index()) {
    if (!seen.insert(value).second) {
      throw input_error{message};
    }
  }
}

void require_matching_index(const dataframe &expected, const dataframe &actual,
                            const char *message) {
  if (expected.get_index() != actual.get_index()) {
    throw input_error{message};
  }
}

bool is_reserved_attribution_name(const std::string_view name) {
  return name == attribution_total_returns_column ||
         name == attribution_common_returns_column ||
         name == attribution_specific_returns_column ||
         name == attribution_tilt_returns_column ||
         name == attribution_timing_returns_column;
}

std::vector<std::string>
factor_names_from_returns(const dataframe &factor_returns,
                          const std::string_view symbol_column_name) {
  const std::string symbol_name =
      checked_name(symbol_column_name, "symbol column name must not be empty");
  const auto &index = factor_returns.get_index();
  const auto columns = factor_returns.get_columns_info<double>();
  std::vector<std::string> factors;
  factors.reserve(columns.size());
  for (const auto &info : columns) {
    if (std::get<2>(info) != typeid(double)) {
      continue;
    }
    std::string name{std::get<0>(info).c_str()};
    const auto &values = factor_returns.get_column<double>(name.c_str());
    if (values.size() != index.size()) {
      throw input_error{
          "factor-return double columns must match the frame index length"};
    }
    if (name.empty() || name == symbol_name || is_reserved_attribution_name(name)) {
      throw input_error{"factor-return column name is reserved or invalid"};
    }
    factors.push_back(std::move(name));
  }
  if (factors.empty()) {
    throw input_error{"factor returns must contain at least one double factor column"};
  }
  return factors;
}

double mean_skipping_nan(const std::span<const double> values) {
  double total = 0.0;
  std::size_t count = 0U;
  for (const double value : values) {
    if (!std::isnan(value)) {
      total += value;
      ++count;
    }
  }
  return count == 0U ? std::numeric_limits<double>::quiet_NaN()
                      : total / static_cast<double>(count);
}

dataframe make_factor_summary(std::vector<std::string> factors,
                              std::vector<double> averages,
                              std::vector<double> annualized,
                              std::vector<double> cumulative) {
  dataframe result;
  std::vector<timestamp> index;
  index.reserve(factors.size());
  for (std::size_t row = 0U; row < factors.size(); ++row) {
    index.push_back(static_cast<timestamp>(row));
  }
  result.load_index(std::move(index));
  result.load_column<std::string>(attribution_factor_column.data(),
                                  std::move(factors),
                                  hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("Average Risk Factor Exposure", std::move(averages),
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("Annualized Return", std::move(annualized),
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("Cumulative Return", std::move(cumulative),
                             hmdf::nan_policy::dont_pad_with_nans);
  return result;
}

} // namespace

dataframe
normalize_attribution_positions(const dataframe &positions,
                                const bool positions_in_dollars,
                                const std::string_view cash_column_name) {
  require_unique_index(positions,
                       "positions index must not contain duplicates");
  const auto assets = position_assets(positions, cash_column_name);
  const auto &index = positions.get_index();
  std::vector<output_column> normalized;
  normalized.reserve(assets.size());
  for (const auto &asset : assets) {
    normalized.push_back({asset.name, std::vector<double>(index.size())});
  }

  for (std::size_t row = 0U; row < index.size(); ++row) {
    double gross = 0.0;
    if (positions_in_dollars) {
      for (const auto &asset : assets) {
        if (!std::isnan(asset.values[row])) {
          gross += std::abs(asset.values[row]);
        }
      }
    }
    for (std::size_t column = 0U; column < assets.size(); ++column) {
      normalized[column].values[row] = positions_in_dollars
                                           ? assets[column].values[row] / gross
                                           : assets[column].values[row];
    }
  }
  return make_frame(index, std::move(normalized));
}

dataframe
compute_exposures(const dataframe &positions, const dataframe &factor_loadings,
                  const std::span<const std::string_view> factor_column_names,
                  const std::string_view cash_column_name,
                  const std::string_view symbol_column_name) {
  const auto factors =
      checked_factor_names(factor_column_names, symbol_column_name);
  require_unique_index(positions,
                       "positions index must not contain duplicates");
  const auto assets = position_assets(positions, cash_column_name);
  const std::string symbol_name =
      checked_name(symbol_column_name, "symbol column name must not be empty");
  const auto symbols = require_string_column(factor_loadings, symbol_name);
  const auto &loading_index = factor_loadings.get_index();
  for (const auto &symbol : symbols) {
    if (symbol.empty()) {
      throw input_error{"factor-loading symbol values must not be empty"};
    }
  }

  std::vector<std::span<const double>> factor_values;
  factor_values.reserve(factors.size());
  for (const auto &name : factors) {
    factor_values.push_back(require_double_column(factor_loadings, name));
  }

  std::map<loading_key, std::size_t> rows_by_loading_key;
  for (std::size_t row = 0U; row < loading_index.size(); ++row) {
    const loading_key key{loading_index[row], symbols[row]};
    if (!rows_by_loading_key.emplace(key, row).second) {
      throw input_error{
          "factor-loading timestamp and symbol keys must be unique"};
    }
  }

  const auto &position_index = positions.get_index();
  std::vector<output_column> exposures;
  exposures.reserve(factors.size());
  for (const auto &name : factors) {
    exposures.push_back({name, {}});
    exposures.back().values.reserve(position_index.size());
  }
  for (std::size_t row = 0U; row < position_index.size(); ++row) {
    for (const auto &asset : assets) {
      const loading_key key{position_index[row], asset.name};
      if (!rows_by_loading_key.contains(key)) {
        throw input_error{
            "required factor-loading timestamp and symbol key is absent"};
      }
    }
    for (std::size_t factor = 0U; factor < factors.size(); ++factor) {
      double total = 0.0;
      for (const auto &asset : assets) {
        const loading_key key{position_index[row], asset.name};
        const std::size_t loading_row = rows_by_loading_key.at(key);
        const double product =
            asset.values[row] * factor_values[factor][loading_row];
        if (!std::isnan(product)) {
          total += product;
        }
      }
      exposures[factor].values.push_back(total);
    }
  }
  return make_frame(position_index, std::move(exposures));
}

perf_attribution_result perf_attrib_core(
    const dataframe &returns, const dataframe &normalized_positions,
    const dataframe &factor_returns, const dataframe &factor_loadings,
    const std::string_view returns_column_name,
    const std::string_view cash_column_name,
    const std::string_view symbol_column_name) {
  const auto return_values = require_double_column(returns, returns_column_name);
  require_unique_index(returns, "returns index must not contain duplicates");
  require_unique_index(normalized_positions,
                       "positions index must not contain duplicates");
  require_unique_index(factor_returns,
                       "factor returns index must not contain duplicates");
  require_matching_index(returns, normalized_positions,
                         "returns and positions indexes must match exactly");
  require_matching_index(returns, factor_returns,
                         "returns and factor returns indexes must match exactly");

  const auto factors = factor_names_from_returns(factor_returns, symbol_column_name);
  std::vector<std::string_view> factor_names;
  factor_names.reserve(factors.size());
  for (const auto &factor : factors) {
    factor_names.push_back(factor);
  }
  dataframe exposures = compute_exposures(normalized_positions, factor_loadings,
                                          factor_names, cash_column_name,
                                          symbol_column_name);

  std::vector<std::span<const double>> exposure_values;
  std::vector<std::span<const double>> factor_values;
  exposure_values.reserve(factors.size());
  factor_values.reserve(factors.size());
  for (const auto &factor : factors) {
    exposure_values.push_back(require_double_column(exposures, factor));
    factor_values.push_back(require_double_column(factor_returns, factor));
  }

  const auto &index = returns.get_index();
  std::vector<output_column> attribution;
  attribution.reserve(factors.size() + 5U);
  for (std::size_t factor = 0U; factor < factors.size(); ++factor) {
    std::vector<double> contribution;
    contribution.reserve(index.size());
    for (std::size_t row = 0U; row < index.size(); ++row) {
      contribution.push_back(exposure_values[factor][row] * factor_values[factor][row]);
    }
    attribution.push_back({factors[factor], std::move(contribution)});
  }

  std::vector<double> common(index.size());
  std::vector<double> specific(index.size());
  std::vector<double> tilt(index.size(), 0.0);
  std::vector<double> timing(index.size());
  std::vector<double> total(return_values.begin(), return_values.end());
  std::vector<double> factor_tilts;
  factor_tilts.reserve(factors.size());
  for (const auto values : exposure_values) {
    factor_tilts.push_back(mean_skipping_nan(values));
  }
  for (std::size_t row = 0U; row < index.size(); ++row) {
    double common_total = 0.0;
    double tilt_total = 0.0;
    for (std::size_t factor = 0U; factor < factors.size(); ++factor) {
      const double contribution = attribution[factor].values[row];
      if (!std::isnan(contribution)) {
        common_total += contribution;
      }
      const double tilt_contribution = factor_tilts[factor] * factor_values[factor][row];
      if (!std::isnan(tilt_contribution)) {
        tilt_total += tilt_contribution;
      }
    }
    common[row] = common_total;
    specific[row] = return_values[row] - common_total;
    tilt[row] = tilt_total;
    timing[row] = common_total - tilt_total;
  }
  attribution.push_back({std::string{attribution_total_returns_column},
                         std::move(total)});
  attribution.push_back({std::string{attribution_common_returns_column},
                         std::move(common)});
  attribution.push_back({std::string{attribution_specific_returns_column},
                         std::move(specific)});
  attribution.push_back({std::string{attribution_tilt_returns_column},
                         std::move(tilt)});
  attribution.push_back({std::string{attribution_timing_returns_column},
                         std::move(timing)});
  return {std::move(exposures), make_frame(index, std::move(attribution))};
}

perf_attribution_result perf_attrib(
    const dataframe &returns, const dataframe &positions,
    const dataframe &factor_returns, const dataframe &factor_loadings,
    const bool positions_in_dollars, const std::string_view returns_column_name,
    const std::string_view cash_column_name,
    const std::string_view symbol_column_name) {
  if (positions_in_dollars) {
    auto normalized =
        normalize_attribution_positions(positions, true, cash_column_name);
    return perf_attrib_core(returns, normalized, factor_returns, factor_loadings,
                            returns_column_name, cash_column_name,
                            symbol_column_name);
  }
  return perf_attrib_core(returns, positions, factor_returns, factor_loadings,
                          returns_column_name, cash_column_name,
                          symbol_column_name);
}

perf_attribution_statistics create_perf_attrib_stats(
    const perf_attribution_result &result, const double annualization) {
  require_matching_index(result.risk_exposures, result.attribution,
                         "risk exposures and attribution indexes must match exactly");
  const auto factors = result.risk_exposures.get_columns_info<double>();
  std::vector<std::string> factor_names;
  std::vector<double> averages;
  std::vector<double> annualized;
  std::vector<double> cumulative;
  factor_names.reserve(factors.size());
  averages.reserve(factors.size());
  annualized.reserve(factors.size());
  cumulative.reserve(factors.size());
  for (const auto &info : factors) {
    if (std::get<2>(info) != typeid(double)) {
      continue;
    }
    const std::string factor{std::get<0>(info).c_str()};
    const auto exposure = require_double_column(result.risk_exposures, factor);
    const auto contribution = require_double_column(result.attribution, factor);
    const auto contribution_frame = make_series(
        std::vector<timestamp>(result.attribution.get_index()), returns_column,
        std::vector<double>(contribution.begin(), contribution.end()));
    factor_names.push_back(factor);
    averages.push_back(mean_skipping_nan(exposure));
    annualized.push_back(annual_return(contribution_frame, annualization));
    cumulative.push_back(cumulative_return(contribution_frame));
  }

  const auto total = require_double_column(result.attribution,
                                           attribution_total_returns_column);
  const auto common = require_double_column(result.attribution,
                                            attribution_common_returns_column);
  const auto specific = require_double_column(result.attribution,
                                              attribution_specific_returns_column);
  const auto &index = result.attribution.get_index();
  const auto total_frame = make_series(std::vector<timestamp>(index), returns_column,
                                       std::vector<double>(total.begin(), total.end()));
  const auto common_frame = make_series(std::vector<timestamp>(index), returns_column,
                                        std::vector<double>(common.begin(), common.end()));
  const auto specific_frame = make_series(
      std::vector<timestamp>(index), returns_column,
      std::vector<double>(specific.begin(), specific.end()));
  return {{{"Annualized Specific Return", annual_return(specific_frame, annualization)},
           {"Annualized Common Return", annual_return(common_frame, annualization)},
           {"Annualized Total Return", annual_return(total_frame, annualization)},
           {"Specific Sharpe Ratio",
            sharpe_ratio(specific_frame, 0.0, annualization)},
           {"Cumulative Specific Return", cumulative_return(specific_frame)},
           {"Cumulative Common Return", cumulative_return(common_frame)},
           {"Total Returns", cumulative_return(total_frame)}},
          make_factor_summary(std::move(factor_names), std::move(averages),
                              std::move(annualized), std::move(cumulative))};
}

dataframe cumulative_returns_less_costs(const dataframe &returns,
                                        const std::string_view returns_column_name) {
  return cumulative_returns(returns, returns_column_name);
}

dataframe cumulative_returns_less_costs(const dataframe &returns,
                                        const dataframe &costs,
                                        const std::string_view returns_column_name,
                                        const std::string_view costs_column_name) {
  const auto return_values = require_double_column(returns, returns_column_name);
  const auto cost_values = require_double_column(costs, costs_column_name);
  require_matching_index(returns, costs,
                         "returns and costs indexes must match exactly");
  std::vector<double> adjusted;
  adjusted.reserve(return_values.size());
  for (std::size_t row = 0U; row < return_values.size(); ++row) {
    adjusted.push_back(return_values[row] - cost_values[row]);
  }
  return cumulative_returns(make_series(std::vector<timestamp>(returns.get_index()),
                                        returns_column_name, std::move(adjusted)),
                            returns_column_name);
}

} // namespace fincore
