#include <fincore/metrics/perf_attrib.hpp>

#include <fincore/error.hpp>

#include <cmath>
#include <cstddef>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <typeindex>
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

} // namespace

dataframe
normalize_attribution_positions(const dataframe &positions,
                                const bool positions_in_dollars,
                                const std::string_view cash_column_name) {
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
  const auto assets = position_assets(positions, cash_column_name);
  const std::string symbol_name =
      checked_name(symbol_column_name, "symbol column name must not be empty");
  const auto symbols = require_string_column(factor_loadings, symbol_name);
  const auto &loading_index = factor_loadings.get_index();

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

} // namespace fincore
