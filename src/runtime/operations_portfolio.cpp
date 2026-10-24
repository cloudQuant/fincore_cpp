#include <fincore/runtime/operations_portfolio.hpp>

#include <fincore/error.hpp>
#include <fincore/portfolio/capacity.hpp>
#include <fincore/portfolio/positions.hpp>
#include <fincore/portfolio/round_trips.hpp>
#include <fincore/portfolio/transactions.hpp>
#include <fincore/risk/backtesting.hpp>
#include <fincore/risk/evt.hpp>
#include <fincore/risk/garch.hpp>
#include <fincore/risk/models.hpp>
#include <fincore/risk/specs.hpp>
#include <fincore/simulation/base.hpp>
#include <fincore/simulation/bootstrap.hpp>
#include <fincore/simulation/correlation.hpp>
#include <fincore/simulation/monte_carlo.hpp>
#include <fincore/simulation/parameters.hpp>
#include <fincore/simulation/paths.hpp>
#include <fincore/simulation/result.hpp>
#include <fincore/simulation/scenarios.hpp>

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <limits>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <typeindex>
#include <utility>
#include <variant>
#include <vector>

namespace fincore {
namespace {

using runtime_callable = std::function<runtime_value(const runtime_inputs&)>;

constexpr double nan = std::numeric_limits<double>::quiet_NaN();
constexpr std::int64_t eight_hours_ns = 8LL * 60LL * 60LL * 1'000'000'000LL;

std::string lowercase(std::string value) {
    for (char& character : value) {
        character = static_cast<char>(
            std::tolower(static_cast<unsigned char>(character)));
    }
    return value;
}

const double& require_number(const runtime_inputs& inputs,
                             const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<double>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a number: " + name};
    }
    return *value;
}

double optional_number(const runtime_inputs& inputs, const std::string& name,
                       const double fallback) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return fallback;
    }
    const auto* value = std::get_if<double>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a number: " + name};
    }
    return *value;
}

std::optional<double> optional_number_value(const runtime_inputs& inputs,
                                            const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return std::nullopt;
    }
    const auto* value = std::get_if<double>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a number: " + name};
    }
    return *value;
}

const std::int64_t& require_integer(const runtime_inputs& inputs,
                                    const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<std::int64_t>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be an integer: " + name};
    }
    return *value;
}

std::optional<std::int64_t> optional_integer(const runtime_inputs& inputs,
                                             const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return std::nullopt;
    }
    const auto* value = std::get_if<std::int64_t>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be an integer: " + name};
    }
    return *value;
}

std::size_t to_size(const std::int64_t value, const std::string& name) {
    if (value < 0) {
        throw input_error{"runtime input must be non-negative: " + name};
    }
    return static_cast<std::size_t>(static_cast<std::uint64_t>(value));
}

std::size_t require_size(const runtime_inputs& inputs,
                         const std::string& name) {
    return to_size(require_integer(inputs, name), name);
}

std::size_t optional_size(const runtime_inputs& inputs, const std::string& name,
                          const std::size_t fallback) {
    const auto value = optional_integer(inputs, name);
    return value.has_value() ? to_size(*value, name) : fallback;
}

std::optional<std::size_t> optional_size_value(const runtime_inputs& inputs,
                                               const std::string& name) {
    const auto value = optional_integer(inputs, name);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return to_size(*value, name);
}

bool optional_bool(const runtime_inputs& inputs, const std::string& name,
                   const bool fallback) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return fallback;
    }
    const auto* value = std::get_if<bool>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a boolean: " + name};
    }
    return *value;
}

std::string require_string(const runtime_inputs& inputs,
                           const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<std::string>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a string: " + name};
    }
    return *value;
}

std::string optional_string(const runtime_inputs& inputs,
                            const std::string& name,
                            const std::string& fallback) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return fallback;
    }
    const auto* value = std::get_if<std::string>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a string: " + name};
    }
    return *value;
}

const dataframe& require_frame(const runtime_inputs& inputs,
                               const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<dataframe>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a dataframe: " + name};
    }
    return *value;
}

const dataframe* optional_frame(const runtime_inputs& inputs,
                                const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return nullptr;
    }
    const auto* value = std::get_if<dataframe>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a dataframe: " + name};
    }
    return value;
}

std::span<const portfolio_transaction> require_transactions(
    const runtime_inputs& inputs, const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value =
        std::get_if<std::vector<portfolio_transaction>>(&found->second.data);
    if (value == nullptr) {
        throw input_error{
            "runtime input must be a vector of portfolio_transaction: " +
            name};
    }
    return *value;
}

template <typename T>
bool has_typed_column(const dataframe& frame, const std::string& name) {
    const auto columns = frame.get_columns_info<T>();
    return std::any_of(columns.begin(), columns.end(),
                       [&name](const auto& info) {
                           return std::string{std::get<0>(info).c_str()} ==
                                      name &&
                                  std::get<2>(info) == typeid(T);
                       });
}

std::vector<std::string> require_string_column(const dataframe& frame,
                                               const std::string& name) {
    if (!has_typed_column<std::string>(frame, name)) {
        throw input_error{"runtime input must contain a string column: " +
                          name};
    }
    const auto& values = frame.get_column<std::string>(name.c_str());
    return {values.begin(), values.end()};
}

std::vector<double> column_or_default(const dataframe& frame,
                                      const std::string& name,
                                      const double fallback) {
    if (!frame.has_column(name.c_str())) {
        return std::vector<double>(frame.get_index().size(), fallback);
    }
    const auto values = require_double_column(frame, name);
    return {values.begin(), values.end()};
}

class frame_builder {
  public:
    explicit frame_builder(std::vector<timestamp> index)
        : rows_(index.size()) {
        frame_.load_index(std::move(index));
    }

    void add_double(const std::string& name, std::vector<double> values) {
        check_size(name, values.size());
        frame_.load_column<double>(name.c_str(), std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }

    void add_string(const std::string& name, std::vector<std::string> values) {
        check_size(name, values.size());
        frame_.load_column<std::string>(name.c_str(), std::move(values),
                                        hmdf::nan_policy::dont_pad_with_nans);
    }

    void add_bool(const std::string& name, std::vector<bool> values) {
        check_size(name, values.size());
        frame_.load_column<bool>(name.c_str(), std::move(values),
                                 hmdf::nan_policy::dont_pad_with_nans);
    }

    void add_timestamp(const std::string& name,
                       std::vector<timestamp> values) {
        check_size(name, values.size());
        frame_.load_column<timestamp>(name.c_str(), std::move(values),
                                      hmdf::nan_policy::dont_pad_with_nans);
    }

    dataframe finish() { return std::move(frame_); }

  private:
    void check_size(const std::string& name, const std::size_t size) const {
        if (size != rows_) {
            throw input_error{"runtime output column length mismatch: " +
                              name};
        }
    }

    dataframe frame_;
    std::size_t rows_;
};

std::vector<timestamp> ordinal_index(const std::size_t size) {
    std::vector<timestamp> index;
    index.reserve(size);
    for (std::size_t position = 0U; position < size; ++position) {
        index.push_back(static_cast<timestamp>(position));
    }
    return index;
}

dataframe transaction_frame(
    const std::span<const portfolio_transaction> transactions) {
    std::vector<timestamp> index;
    std::vector<double> amounts;
    std::vector<double> prices;
    index.reserve(transactions.size());
    amounts.reserve(transactions.size());
    prices.reserve(transactions.size());
    for (const auto& transaction : transactions) {
        index.push_back(transaction.date);
        amounts.push_back(transaction.amount);
        prices.push_back(transaction.price);
    }
    frame_builder builder(std::move(index));
    builder.add_double("amount", std::move(amounts));
    builder.add_double("price", std::move(prices));
    return builder.finish();
}

std::vector<trade_record> trade_records(
    const std::span<const portfolio_transaction> transactions) {
    std::vector<trade_record> result;
    result.reserve(transactions.size());
    for (const auto& transaction : transactions) {
        result.push_back(
            {transaction.date, transaction.symbol, transaction.amount});
    }
    return result;
}

std::vector<round_trip_transaction> round_trip_transactions(
    const std::span<const portfolio_transaction> transactions) {
    std::vector<round_trip_transaction> result;
    result.reserve(transactions.size());
    for (const auto& transaction : transactions) {
        result.push_back({transaction.date, transaction.symbol,
                          transaction.amount, transaction.price});
    }
    return result;
}

evt_tail parse_evt_tail(const std::string& value) {
    const auto lower = lowercase(value);
    if (lower == "lower") {
        return evt_tail::lower;
    }
    if (lower == "upper") {
        return evt_tail::upper;
    }
    throw input_error{"tail must be 'lower' or 'upper'"};
}

evt_model parse_evt_model(const std::string& value) {
    const auto lower = lowercase(value);
    if (lower == "gpd") {
        return evt_model::gpd;
    }
    if (lower == "gev") {
        return evt_model::gev;
    }
    throw input_error{"model must be 'gpd' or 'gev'"};
}

gpd_estimation_method parse_gpd_method(const std::string& value) {
    const auto lower = lowercase(value);
    if (lower == "mle") {
        return gpd_estimation_method::mle;
    }
    if (lower == "pwm") {
        return gpd_estimation_method::pwm;
    }
    throw input_error{"gpd method must be 'mle' or 'pwm'"};
}

garch_model parse_garch_model(const std::string& value) {
    auto upper = value;
    std::transform(upper.begin(), upper.end(), upper.begin(),
                   [](const unsigned char character) {
                       return static_cast<char>(std::toupper(character));
                   });
    if (upper == "GARCH") {
        return garch_model::garch;
    }
    if (upper == "EGARCH") {
        return garch_model::egarch;
    }
    if (upper == "GJRGARCH" || upper == "GJR-GARCH" ||
        upper == "GJR_GARCH") {
        return garch_model::gjr_garch;
    }
    throw input_error{"unknown GARCH model: " + value};
}

risk_forecast_method parse_forecast_method(const std::string& value) {
    const auto lower = lowercase(value);
    if (lower == "historical") {
        return risk_forecast_method::historical;
    }
    if (lower == "evt") {
        return risk_forecast_method::evt;
    }
    if (lower == "garch") {
        return risk_forecast_method::garch;
    }
    throw input_error{"unknown forecast method: " + value};
}

bootstrap_statistic parse_bootstrap_statistic(const std::string& value) {
    const auto lower = lowercase(value);
    if (lower == "mean") {
        return bootstrap_statistic::mean;
    }
    if (lower == "std" || lower == "sample_standard_deviation") {
        return bootstrap_statistic::sample_standard_deviation;
    }
    if (lower == "median") {
        return bootstrap_statistic::median;
    }
    if (lower == "sharpe") {
        return bootstrap_statistic::sharpe_ratio;
    }
    if (lower == "sortino") {
        return bootstrap_statistic::sortino_ratio;
    }
    if (lower == "min") {
        return bootstrap_statistic::minimum;
    }
    if (lower == "max") {
        return bootstrap_statistic::maximum;
    }
    throw input_error{"unknown bootstrap statistic: " + value};
}

turnover_denominator parse_turnover_denominator(const std::string& value) {
    const auto lower = lowercase(value);
    if (lower == "agb" || lower == "actual_gross_book") {
        return turnover_denominator::actual_gross_book;
    }
    if (lower == "portfolio_value") {
        return turnover_denominator::portfolio_value;
    }
    throw input_error{
        "denominator must be either 'AGB' or 'portfolio_value'"};
}

dataframe serialize_exposure_bundle(const exposure_bundle& bundle) {
    frame_builder builder(bundle.long_exposure.get_index());
    const auto append = [&builder](const dataframe& frame,
                                   const std::string& prefix) {
        for (const auto& info : frame.get_columns_info<double>()) {
            if (std::get<2>(info) != typeid(double)) {
                continue;
            }
            const std::string name{std::get<0>(info).c_str()};
            const auto& values = frame.get_column<double>(name.c_str());
            builder.add_double(prefix + name,
                               std::vector<double>(values.begin(),
                                                   values.end()));
        }
    };
    append(bundle.long_exposure, "long:");
    append(bundle.short_exposure, "short:");
    append(bundle.gross_exposure, "gross:");
    append(bundle.net_exposure, "net:");
    return builder.finish();
}

dataframe serialize_top_positions(const top_positions& positions) {
    std::size_t rows = positions.long_positions.size() +
                       positions.short_positions.size() +
                       positions.absolute_positions.size();
    frame_builder builder(ordinal_index(rows));
    std::vector<std::string> rankings;
    std::vector<std::string> symbols;
    std::vector<double> values;
    rankings.reserve(rows);
    symbols.reserve(rows);
    values.reserve(rows);
    const auto append = [&rankings, &symbols, &values](
                            const std::string& ranking,
                            const std::vector<ranked_position>& entries) {
        for (const auto& entry : entries) {
            rankings.push_back(ranking);
            symbols.push_back(entry.symbol);
            values.push_back(entry.value);
        }
    };
    append("long", positions.long_positions);
    append("short", positions.short_positions);
    append("absolute", positions.absolute_positions);
    builder.add_string("ranking", std::move(rankings));
    builder.add_string("symbol", std::move(symbols));
    builder.add_double("value", std::move(values));
    return builder.finish();
}

dataframe serialize_capacity_assessment(const capacity_assessment& assessment) {
    const auto& liquidation = assessment.liquidation_days;
    const std::size_t liquidation_rows =
        liquidation.get_index().size() *
        liquidation.get_columns_info<double>().size();
    const std::size_t rows = liquidation_rows +
                             assessment.ticker_maximums.size() +
                             assessment.low_liquidity_transactions.size();
    frame_builder builder(ordinal_index(rows));
    std::vector<std::string> tables;
    std::vector<std::string> symbols;
    std::vector<bool> has_dates;
    std::vector<timestamp> dates;
    std::vector<double> days;
    std::vector<double> allocations;
    std::vector<double> consumptions;
    tables.reserve(rows);
    symbols.reserve(rows);
    has_dates.reserve(rows);
    dates.reserve(rows);
    days.reserve(rows);
    allocations.reserve(rows);
    consumptions.reserve(rows);

    for (const auto& info : liquidation.get_columns_info<double>()) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }
        const std::string symbol{std::get<0>(info).c_str()};
        const auto& values = liquidation.get_column<double>(symbol.c_str());
        for (std::size_t row = 0U; row < liquidation.get_index().size();
             ++row) {
            tables.push_back("liquidation_days");
            symbols.push_back(symbol);
            has_dates.push_back(true);
            dates.push_back(liquidation.get_index()[row]);
            days.push_back(values[row]);
            allocations.push_back(nan);
            consumptions.push_back(nan);
        }
    }
    for (const auto& entry : assessment.ticker_maximums) {
        tables.push_back("ticker_maximums");
        symbols.push_back(entry.symbol);
        has_dates.push_back(false);
        dates.push_back(0);
        days.push_back(entry.days_to_liquidate);
        allocations.push_back(entry.position_allocation_pct);
        consumptions.push_back(nan);
    }
    for (const auto& entry : assessment.low_liquidity_transactions) {
        tables.push_back("low_liquidity_transactions");
        symbols.push_back(entry.symbol);
        has_dates.push_back(true);
        dates.push_back(entry.date);
        days.push_back(nan);
        allocations.push_back(nan);
        consumptions.push_back(entry.max_percent_bar_consumed);
    }
    builder.add_string("table", std::move(tables));
    builder.add_string("symbol", std::move(symbols));
    builder.add_bool("has_date", std::move(has_dates));
    builder.add_timestamp("date", std::move(dates));
    builder.add_double("days_to_liquidate", std::move(days));
    builder.add_double("position_allocation_pct", std::move(allocations));
    builder.add_double("max_percent_bar_consumed", std::move(consumptions));
    return builder.finish();
}

dataframe serialize_ticker_liquidity(
    const std::vector<ticker_liquidity>& entries) {
    frame_builder builder(ordinal_index(entries.size()));
    std::vector<std::string> symbols;
    std::vector<double> days;
    std::vector<double> allocations;
    symbols.reserve(entries.size());
    days.reserve(entries.size());
    allocations.reserve(entries.size());
    for (const auto& entry : entries) {
        symbols.push_back(entry.symbol);
        days.push_back(entry.days_to_liquidate);
        allocations.push_back(entry.position_allocation_pct);
    }
    builder.add_string("symbol", std::move(symbols));
    builder.add_double("days_to_liquidate", std::move(days));
    builder.add_double("position_allocation_pct", std::move(allocations));
    return builder.finish();
}

dataframe serialize_transaction_liquidity(
    const std::vector<transaction_liquidity>& entries) {
    frame_builder builder(ordinal_index(entries.size()));
    std::vector<std::string> symbols;
    std::vector<timestamp> dates;
    std::vector<double> consumptions;
    symbols.reserve(entries.size());
    dates.reserve(entries.size());
    consumptions.reserve(entries.size());
    for (const auto& entry : entries) {
        symbols.push_back(entry.symbol);
        dates.push_back(entry.date);
        consumptions.push_back(entry.max_percent_bar_consumed);
    }
    builder.add_string("symbol", std::move(symbols));
    builder.add_timestamp("date", std::move(dates));
    builder.add_double("max_percent_bar_consumed", std::move(consumptions));
    return builder.finish();
}

dataframe serialize_daily_txn_bars(const std::vector<daily_txn_bar>& entries) {
    frame_builder builder(ordinal_index(entries.size()));
    std::vector<timestamp> dates;
    std::vector<std::string> symbols;
    std::vector<double> amounts;
    std::vector<double> prices;
    std::vector<double> volumes;
    dates.reserve(entries.size());
    symbols.reserve(entries.size());
    amounts.reserve(entries.size());
    prices.reserve(entries.size());
    volumes.reserve(entries.size());
    for (const auto& entry : entries) {
        dates.push_back(entry.date);
        symbols.push_back(entry.symbol);
        amounts.push_back(entry.amount);
        prices.push_back(entry.price);
        volumes.push_back(entry.volume);
    }
    builder.add_timestamp("date", std::move(dates));
    builder.add_string("symbol", std::move(symbols));
    builder.add_double("amount", std::move(amounts));
    builder.add_double("price", std::move(prices));
    builder.add_double("volume", std::move(volumes));
    return builder.finish();
}

dataframe serialize_round_trip_stats(const round_trip_stats& stats) {
    std::size_t rows = 0U;
    const auto count = [&rows](const round_trip_stat_table& table) {
        rows += table.values.size();
    };
    count(stats.pnl);
    count(stats.summary);
    count(stats.duration);
    count(stats.returns);
    for (const auto& row : stats.symbol_values) {
        rows += row.size();
    }
    frame_builder builder(ordinal_index(rows));
    std::vector<std::string> tables;
    std::vector<std::string> row_labels;
    std::vector<std::string> column_labels;
    std::vector<double> values;
    tables.reserve(rows);
    row_labels.reserve(rows);
    column_labels.reserve(rows);
    values.reserve(rows);
    const auto append = [&](const std::string& table_name,
                            const round_trip_stat_table& table) {
        for (std::size_t row = 0U; row < table.values.size(); ++row) {
            if (row >= table.row_labels.size()) {
                throw input_error{"round-trip statistic row label missing"};
            }
            for (std::size_t column = 0U;
                 column < table.values[row].size(); ++column) {
                if (column >= table.column_labels.size()) {
                    throw input_error{
                        "round-trip statistic column label missing"};
                }
                tables.push_back(table_name);
                row_labels.push_back(table.row_labels[row]);
                column_labels.push_back(table.column_labels[column]);
                values.push_back(table.values[row][column]);
            }
        }
    };
    append("pnl", stats.pnl);
    append("summary", stats.summary);
    append("duration", stats.duration);
    append("returns", stats.returns);
    for (std::size_t row = 0U; row < stats.symbol_values.size(); ++row) {
        for (std::size_t column = 0U;
             column < stats.symbol_values[row].size(); ++column) {
            tables.push_back("symbol");
            row_labels.push_back(row < stats.symbol_labels.size()
                                     ? stats.symbol_labels[row]
                                     : std::string{});
            column_labels.push_back(
                column < stats.symbol_stat_labels.size()
                    ? stats.symbol_stat_labels[column]
                    : std::string{});
            values.push_back(stats.symbol_values[row][column]);
        }
    }
    builder.add_string("table", std::move(tables));
    builder.add_string("row_label", std::move(row_labels));
    builder.add_string("column_label", std::move(column_labels));
    builder.add_double("value", std::move(values));
    return builder.finish();
}

dataframe serialize_long_short_aggregate(const long_short_aggregate& aggregate) {
    frame_builder builder(ordinal_index(aggregate.statistic_names.size()));
    std::vector<double> all_trades = aggregate.all_trades;
    std::vector<double> long_trades = aggregate.long_trades;
    std::vector<double> short_trades = aggregate.short_trades;
    builder.add_string("statistic", aggregate.statistic_names);
    builder.add_double("all_trades", std::move(all_trades));
    builder.add_double("long_trades", std::move(long_trades));
    builder.add_double("short_trades", std::move(short_trades));
    return builder.finish();
}

std::string_view backtest_method_name(const risk_backtest_method method) {
    return method == risk_backtest_method::var ? "var" : "es";
}

std::string_view estimate_method_name(const risk_forecast_method method) {
    switch (method) {
    case risk_forecast_method::historical:
        return "historical";
    case risk_forecast_method::evt:
        return "evt";
    case risk_forecast_method::garch:
        return "garch";
    }
    return "historical";
}

std::string_view estimate_status_name(const risk_estimate_status status) {
    switch (status) {
    case risk_estimate_status::ok:
        return "ok";
    case risk_estimate_status::insufficient_data:
        return "insufficient_data";
    case risk_estimate_status::failed:
        return "failed";
    }
    return "failed";
}

dataframe serialize_backtest_result(const risk_backtest_result& result) {
    frame_builder builder(ordinal_index(1U));
    builder.add_string("method",
                       {std::string{backtest_method_name(result.method)}});
    builder.add_double("confidence_level", {result.confidence_level});
    builder.add_double("observations",
                       {static_cast<double>(result.observations)});
    builder.add_double("exceptions",
                       {static_cast<double>(result.exceptions)});
    builder.add_double("expected_exceptions", {result.expected_exceptions});
    builder.add_double("exception_rate", {result.exception_rate});
    builder.add_double("kupiec_lr", {result.kupiec_lr});
    builder.add_double("kupiec_pvalue", {result.kupiec_pvalue});
    builder.add_double("christoffersen_lr", {result.christoffersen_lr});
    builder.add_double("christoffersen_pvalue",
                       {result.christoffersen_pvalue});
    builder.add_string("inputs_digest", {result.inputs_digest});
    builder.add_string("status",
                       {std::string{risk_backtest_status_name(result.status)}});
    for (const auto& [name, value] : result.diagnostics) {
        builder.add_double("diagnostic_" + name, {value});
    }
    return builder.finish();
}

dataframe serialize_risk_estimate(const risk_estimate& estimate) {
    frame_builder builder(ordinal_index(1U));
    builder.add_string(
        "method", {std::string{estimate_method_name(estimate.method)}});
    builder.add_double("confidence_level", {estimate.confidence_level});
    builder.add_double("horizon",
                       {static_cast<double>(estimate.horizon)});
    builder.add_string("sign_convention", {estimate.sign_convention});
    builder.add_double("estimate", {estimate.estimate});
    builder.add_timestamp("forecast_timestamp",
                          {estimate.forecast_timestamp});
    builder.add_bool("has_forecast_timestamp",
                     {estimate.has_forecast_timestamp});
    builder.add_string("inputs_digest", {estimate.inputs_digest});
    builder.add_bool("has_forecast", {estimate.has_forecast});
    builder.add_string("kernel", {estimate.kernel});
    builder.add_string("note", {estimate.note});
    builder.add_string("status",
                       {std::string{estimate_status_name(estimate.status)}});
    for (const auto& [name, value] : estimate.diagnostics) {
        builder.add_double("diagnostic_" + name, {value});
    }
    return builder.finish();
}

dataframe serialize_hill_estimate(const hill_estimate& estimate) {
    const std::size_t rows = estimate.tail_observations.size();
    frame_builder builder(ordinal_index(rows));
    builder.add_double("xi",
                       std::vector<double>(rows, estimate.xi));
    builder.add_double("tail_observation", estimate.tail_observations);
    return builder.finish();
}

dataframe serialize_gpd_parameters(const gpd_parameters& parameters) {
    frame_builder builder(ordinal_index(1U));
    builder.add_double("xi", {parameters.xi});
    builder.add_double("beta", {parameters.beta});
    builder.add_double("threshold", {parameters.threshold});
    builder.add_double("n_exceed",
                       {static_cast<double>(parameters.n_exceed)});
    return builder.finish();
}

dataframe serialize_gev_parameters(const gev_parameters& parameters) {
    frame_builder builder(ordinal_index(1U));
    builder.add_double("xi", {parameters.xi});
    builder.add_double("mu", {parameters.mu});
    builder.add_double("sigma", {parameters.sigma});
    builder.add_double("n_blocks",
                       {static_cast<double>(parameters.n_blocks)});
    return builder.finish();
}

dataframe serialize_extreme_risk(const extreme_risk_result& result) {
    frame_builder builder(ordinal_index(1U));
    builder.add_double("var", {result.var});
    builder.add_double("cvar", {result.cvar});
    builder.add_double("tail_index", {result.tail_index});
    builder.add_double("threshold", {result.threshold});
    builder.add_double("n_exceedances",
                       {static_cast<double>(result.n_exceedances)});
    builder.add_double("alpha", {result.alpha});
    builder.add_double("location",
                       {result.location.value_or(nan)});
    builder.add_double("scale", {result.scale.value_or(nan)});
    builder.add_double(
        "n_blocks",
        {result.n_blocks.has_value()
             ? static_cast<double>(*result.n_blocks)
             : nan});
    return builder.finish();
}

dataframe serialize_garch_forecast(const garch_forecast_result& result) {
    const std::size_t rows = result.forecasts.size();
    frame_builder builder(ordinal_index(rows));
    builder.add_double("forecast", result.forecasts);
    builder.add_double("log_likelihood",
                       std::vector<double>(rows, result.log_likelihood));
    builder.add_bool("converged", std::vector<bool>(rows, result.converged));
    return builder.finish();
}

dataframe serialize_garch_conditional(
    const garch_conditional_estimate& result) {
    const std::size_t rows = result.conditional_variances.size();
    frame_builder builder(ordinal_index(rows));
    builder.add_double("conditional_variance",
                       result.conditional_variances);
    builder.add_double("value", std::vector<double>(rows, result.value));
    builder.add_bool("converged", std::vector<bool>(rows, result.converged));
    return builder.finish();
}

dataframe serialize_simulation_statistics(
    const simulation_terminal_statistics& statistics) {
    frame_builder builder(ordinal_index(1U));
    builder.add_double("mean", {statistics.mean});
    builder.add_double("sample_standard_deviation",
                       {statistics.sample_standard_deviation});
    builder.add_double("median", {statistics.median});
    builder.add_double("percentile_1",
                       {statistics.percentiles.percentile_1});
    builder.add_double("percentile_5",
                       {statistics.percentiles.percentile_5});
    builder.add_double("percentile_25",
                       {statistics.percentiles.percentile_25});
    builder.add_double("percentile_50",
                       {statistics.percentiles.percentile_50});
    builder.add_double("percentile_75",
                       {statistics.percentiles.percentile_75});
    builder.add_double("percentile_95",
                       {statistics.percentiles.percentile_95});
    builder.add_double("percentile_99",
                       {statistics.percentiles.percentile_99});
    return builder.finish();
}

dataframe serialize_simulation_parameters(
    const simulation_parameters& parameters) {
    frame_builder builder(ordinal_index(1U));
    builder.add_double("drift", {parameters.drift});
    builder.add_double("volatility", {parameters.volatility});
    return builder.finish();
}

dataframe serialize_bootstrap_summary(const bootstrap_summary_result& summary) {
    const std::size_t rows = 4U;
    frame_builder builder(ordinal_index(rows));
    std::vector<std::string> names{"mean", "std", "sharpe", "sortino"};
    std::vector<double> values;
    std::vector<double> errors;
    std::vector<double> lower;
    std::vector<double> upper;
    const auto append = [&values, &errors, &lower,
                         &upper](const bootstrap_summary_statistic& entry) {
        values.push_back(entry.value);
        errors.push_back(entry.standard_error);
        lower.push_back(entry.confidence_interval.lower);
        upper.push_back(entry.confidence_interval.upper);
    };
    append(summary.mean);
    append(summary.sample_standard_deviation);
    append(summary.sharpe_ratio);
    append(summary.sortino_ratio);
    builder.add_string("statistic", std::move(names));
    builder.add_double("value", std::move(values));
    builder.add_double("standard_error", std::move(errors));
    builder.add_double("ci_lower", std::move(lower));
    builder.add_double("ci_upper", std::move(upper));
    return builder.finish();
}

dataframe serialize_stress_results(
    const std::vector<stress_scenario_result>& results,
    const std::string& column) {
    std::size_t rows = 0U;
    for (const auto& result : results) {
        rows += result.stressed_returns.get_index().size();
    }
    frame_builder builder(ordinal_index(rows));
    std::vector<std::string> scenarios;
    std::vector<timestamp> dates;
    std::vector<double> stressed;
    std::vector<double> cumulative;
    std::vector<double> drawdown;
    std::vector<double> volatility;
    std::vector<double> final_values;
    scenarios.reserve(rows);
    dates.reserve(rows);
    stressed.reserve(rows);
    cumulative.reserve(rows);
    drawdown.reserve(rows);
    volatility.reserve(rows);
    final_values.reserve(rows);
    for (const auto& result : results) {
        const auto values = require_double_column(result.stressed_returns,
                                                  column);
        for (std::size_t row = 0U;
             row < result.stressed_returns.get_index().size(); ++row) {
            scenarios.push_back(result.name);
            dates.push_back(result.stressed_returns.get_index()[row]);
            stressed.push_back(values[row]);
            cumulative.push_back(result.cumulative_return);
            drawdown.push_back(result.max_drawdown);
            volatility.push_back(result.volatility);
            final_values.push_back(result.final_value);
        }
    }
    builder.add_string("scenario", std::move(scenarios));
    builder.add_timestamp("date", std::move(dates));
    builder.add_double("stressed_return", std::move(stressed));
    builder.add_double("cumulative_return", std::move(cumulative));
    builder.add_double("max_drawdown", std::move(drawdown));
    builder.add_double("volatility", std::move(volatility));
    builder.add_double("final_value", std::move(final_values));
    return builder.finish();
}

std::vector<int> exception_flags(const dataframe& frame,
                                 const std::string& name) {
    if (has_typed_column<bool>(frame, name)) {
        const auto& values = frame.get_column<bool>(name.c_str());
        std::vector<int> result;
        result.reserve(values.size());
        for (const bool value : values) {
            result.push_back(value ? 1 : 0);
        }
        return result;
    }
    if (has_typed_column<std::int64_t>(frame, name)) {
        const auto& values = frame.get_column<std::int64_t>(name.c_str());
        std::vector<int> result;
        result.reserve(values.size());
        for (const auto value : values) {
            result.push_back(value != 0 ? 1 : 0);
        }
        return result;
    }
    if (has_typed_column<double>(frame, name)) {
        const auto& values = frame.get_column<double>(name.c_str());
        std::vector<int> result;
        result.reserve(values.size());
        for (const double value : values) {
            result.push_back(value != 0.0 ? 1 : 0);
        }
        return result;
    }
    throw input_error{
        "exceptions must be a boolean, integer or double dataframe column"};
}

std::vector<sector_definition> default_sector_definitions() {
    return {{101.0, "Basic Materials"},
            {102.0, "Consumer Cyclical"},
            {103.0, "Financial Services"},
            {104.0, "Real Estate"},
            {205.0, "Consumer Defensive"},
            {206.0, "Healthcare"},
            {207.0, "Utilities"},
            {308.0, "Communication Services"},
            {309.0, "Energy"},
            {310.0, "Industrials"},
            {311.0, "Technology"}};
}

std::vector<sector_definition> sector_definitions_from(
    const dataframe& mapping) {
    if (!has_typed_column<double>(mapping, "id") ||
        !has_typed_column<std::string>(mapping, "name")) {
        throw input_error{
            "sector_dict must contain double 'id' and string 'name' columns"};
    }
    const auto& ids = mapping.get_column<double>("id");
    const auto& names = mapping.get_column<std::string>("name");
    if (ids.size() != names.size()) {
        throw input_error{"sector_dict columns must have equal lengths"};
    }
    std::vector<sector_definition> result;
    result.reserve(ids.size());
    for (std::size_t row = 0U; row < ids.size(); ++row) {
        result.push_back({ids[row], names[row]});
    }
    return result;
}

std::vector<sector_assignment> sector_assignments_from(
    const dataframe& mapping) {
    if (!has_typed_column<std::string>(mapping, "symbol") ||
        !has_typed_column<std::string>(mapping, "sector")) {
        throw input_error{
            "symbol_sector_map must contain string 'symbol' and 'sector' "
            "columns"};
    }
    const auto& symbols = mapping.get_column<std::string>("symbol");
    const auto& sectors = mapping.get_column<std::string>("sector");
    if (symbols.size() != sectors.size()) {
        throw input_error{
            "symbol_sector_map columns must have equal lengths"};
    }
    std::vector<sector_assignment> result;
    result.reserve(symbols.size());
    for (std::size_t row = 0U; row < symbols.size(); ++row) {
        result.push_back({symbols[row], sectors[row]});
    }
    return result;
}

std::map<std::string, std::string> sector_mapping_map(
    const dataframe& mapping) {
    const auto assignments = sector_assignments_from(mapping);
    std::map<std::string, std::string> result;
    for (const auto& assignment : assignments) {
        result.emplace(assignment.symbol, assignment.sector);
    }
    return result;
}

std::vector<round_trip_statistic> round_trip_statistics_from(
    const std::string& requested) {
    std::vector<round_trip_statistic> result;
    std::size_t start = 0U;
    while (start <= requested.size()) {
        const auto comma = requested.find(',', start);
        auto token = requested.substr(
            start, comma == std::string::npos ? std::string::npos
                                              : comma - start);
        token.erase(token.begin(),
                    std::find_if(token.begin(), token.end(),
                                 [](const unsigned char character) {
                                     return !std::isspace(character);
                                 }));
        token.erase(
            std::find_if(token.rbegin(), token.rend(),
                         [](const unsigned char character) {
                             return !std::isspace(character);
                         })
                .base(),
            token.end());
        if (!token.empty()) {
            const auto lower = lowercase(token);
            if (lower == "mean") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         if (values.empty()) {
                             return nan;
                         }
                         double total = 0.0;
                         for (const double value : values) {
                             total += value;
                         }
                         return total / static_cast<double>(values.size());
                     }});
            } else if (lower == "sum") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         double total = 0.0;
                         for (const double value : values) {
                             total += value;
                         }
                         return total;
                     }});
            } else if (lower == "median") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         if (values.empty()) {
                             return nan;
                         }
                         std::vector<double> sorted(values.begin(),
                                                    values.end());
                         std::sort(sorted.begin(), sorted.end());
                         const std::size_t middle = sorted.size() / 2U;
                         return sorted.size() % 2U != 0U
                                    ? sorted[middle]
                                    : (sorted[middle - 1U] +
                                       sorted[middle]) /
                                          2.0;
                     }});
            } else if (lower == "std" ||
                       lower == "sample_standard_deviation") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         if (values.size() < 2U) {
                             return nan;
                         }
                         double mean = 0.0;
                         for (const double value : values) {
                             mean += value;
                         }
                         mean /= static_cast<double>(values.size());
                         double sum = 0.0;
                         for (const double value : values) {
                             const double delta = value - mean;
                             sum += delta * delta;
                         }
                         return std::sqrt(
                             sum / static_cast<double>(values.size() - 1U));
                     }});
            } else if (lower == "min") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         if (values.empty()) {
                             return nan;
                         }
                         return *std::min_element(values.begin(),
                                                  values.end());
                     }});
            } else if (lower == "max") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         if (values.empty()) {
                             return nan;
                         }
                         return *std::max_element(values.begin(),
                                                  values.end());
                     }});
            } else if (lower == "count") {
                result.push_back(
                    {token, [](std::span<const double> values) {
                         return static_cast<double>(values.size());
                     }});
            } else {
                throw input_error{"unknown round-trip statistic: " + token};
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1U;
    }
    if (result.empty()) {
        throw input_error{"statistics must name at least one statistic"};
    }
    return result;
}

std::vector<predefined_stress_scenario> stress_scenarios_from(
    const std::string& requested) {
    std::vector<predefined_stress_scenario> result;
    std::size_t start = 0U;
    while (start <= requested.size()) {
        const auto comma = requested.find(',', start);
        auto token = requested.substr(
            start, comma == std::string::npos ? std::string::npos
                                              : comma - start);
        token.erase(std::remove_if(token.begin(), token.end(),
                                   [](const unsigned char character) {
                                       return std::isspace(character);
                                   }),
                    token.end());
        if (!token.empty()) {
            const auto lower = lowercase(token);
            if (lower == "crash") {
                result.push_back(predefined_stress_scenario::crash);
            } else if (lower == "spike") {
                result.push_back(predefined_stress_scenario::spike);
            } else if (lower == "vol_crush") {
                result.push_back(
                    predefined_stress_scenario::volatility_crush);
            } else if (lower == "vol_spike") {
                result.push_back(
                    predefined_stress_scenario::volatility_spike);
            }
        }
        if (comma == std::string::npos) {
            break;
        }
        start = comma + 1U;
    }
    return result;
}

std::vector<custom_stress_scenario> custom_scenarios_from(
    const dataframe& frame) {
    if (!has_typed_column<std::string>(frame, "name")) {
        throw input_error{
            "custom_scenarios must contain a string 'name' column"};
    }
    const auto names = require_string_column(frame, "name");
    auto shifts = column_or_default(frame, "return_shift", 0.0);
    auto multipliers =
        column_or_default(frame, "return_multiplier", 1.0);
    std::vector<custom_stress_scenario> result;
    result.reserve(names.size());
    for (std::size_t row = 0U; row < names.size(); ++row) {
        result.push_back({names[row], shifts[row], multipliers[row]});
    }
    return result;
}

std::vector<stress_scenario_result> stress_results_from_table(
    const dataframe& table) {
    if (!has_typed_column<std::string>(table, "scenario")) {
        throw input_error{
            "stress_results must contain a string 'scenario' column"};
    }
    const auto names = require_string_column(table, "scenario");
    const auto cumulative =
        column_or_default(table, "cumulative_return", nan);
    const auto drawdown = column_or_default(table, "max_drawdown", nan);
    const auto volatility = column_or_default(table, "volatility", nan);
    const auto final_values = column_or_default(table, "final_value", nan);
    std::vector<stress_scenario_result> result;
    std::vector<std::string> seen;
    for (std::size_t row = 0U; row < names.size(); ++row) {
        if (std::find(seen.begin(), seen.end(), names[row]) != seen.end()) {
            continue;
        }
        seen.push_back(names[row]);
        result.push_back({names[row], dataframe{}, cumulative[row],
                          drawdown[row], volatility[row], final_values[row]});
    }
    return result;
}

operation_spec make_spec(std::string operation_id,
                         std::string capability_id, std::string kernel_symbol,
                         std::string input_schema, const bool deterministic,
                         std::string rng_policy,
                         runtime_callable callable) {
    const auto dot = operation_id.find('.');
    if (dot == std::string::npos || dot == 0U) {
        throw input_error{"operation id must contain a domain"};
    }
    const std::string domain = operation_id.substr(0U, dot);
    operation_spec spec;
    spec.operation_id = operation_id;
    spec.capability_id = std::move(capability_id);
    spec.domain = domain;
    spec.callable = std::move(callable);
    spec.deterministic = deterministic;
    spec.rng_policy = std::move(rng_policy);
    spec.provenance = {{"owner", domain},
                       {"kernel_symbol", std::move(kernel_symbol)},
                       {"source_operation_id", operation_id},
                       {"input_schema", std::move(input_schema)}};
    return spec;
}

constexpr std::string_view seeded_rng = "seeded_mt19937";
constexpr std::string_view no_rng = "none";

}  // namespace

std::vector<operation_spec> portfolio_risk_simulation_operations() {
    std::vector<operation_spec> operations;
    operations.reserve(58U);

    // ---------------------------------------------------------------- portfolio
    operations.push_back(make_spec(
        "portfolio.capacity.assess_liquidity",
        "fincore.portfolio.capacity.assess_liquidity", "assess_liquidity",
        R"({"required":{"positions":"dataframe","transactions":"portfolio_transactions","prices":"dataframe","volumes":"dataframe"},"optional":{"max_bar_consumption":"double","capital_base":"double","mean_volume_window":"integer","last_n_days":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            capacity_config config;
            config.max_bar_consumption =
                optional_number(inputs, "max_bar_consumption",
                                config.max_bar_consumption);
            config.capital_base = optional_number(inputs, "capital_base",
                                                  config.capital_base);
            config.mean_volume_window =
                optional_size(inputs, "mean_volume_window",
                              config.mean_volume_window);
            config.last_n_days =
                optional_size_value(inputs, "last_n_days");
            const auto trades =
                trade_records(require_transactions(inputs, "transactions"));
            return runtime_value(serialize_capacity_assessment(
                assess_liquidity(require_frame(inputs, "positions"), trades,
                                 require_frame(inputs, "prices"),
                                 require_frame(inputs, "volumes"), config)));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.compute_cap_exposures",
        "fincore.portfolio.positions.compute_cap_exposures",
        "compute_cap_exposures",
        R"({"required":{"positions":"dataframe","caps":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_exposure_bundle(
                compute_cap_exposures(
                    require_frame(inputs, "positions"),
                    require_frame(inputs, "caps"),
                    optional_string(inputs, "cash_column_name", "cash"))));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.compute_sector_exposures",
        "fincore.portfolio.positions.compute_sector_exposures",
        "compute_sector_exposures",
        R"({"required":{"positions":"dataframe","sectors":"dataframe"},"optional":{"sector_dict":"dataframe","cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto* mapping = optional_frame(inputs, "sector_dict");
            auto definitions = mapping == nullptr
                                   ? default_sector_definitions()
                                   : sector_definitions_from(*mapping);
            return runtime_value(serialize_exposure_bundle(
                compute_sector_exposures(
                    require_frame(inputs, "positions"),
                    require_frame(inputs, "sectors"), definitions,
                    optional_string(inputs, "cash_column_name", "cash"))));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.compute_style_factor_exposures",
        "fincore.portfolio.positions.compute_style_factor_exposures",
        "compute_style_factor_exposures",
        R"({"required":{"positions":"dataframe","risk_factor":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(compute_style_factor_exposures(
                require_frame(inputs, "positions"),
                require_frame(inputs, "risk_factor"),
                optional_string(inputs, "cash_column_name", "cash")));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.compute_volume_exposures",
        "fincore.portfolio.positions.compute_volume_exposures",
        "compute_volume_exposures",
        R"({"required":{"shares_held":"dataframe","volumes":"dataframe","percentile":"double"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_exposure_bundle(
                compute_volume_exposures(
                    require_frame(inputs, "shares_held"),
                    require_frame(inputs, "volumes"),
                    require_number(inputs, "percentile"))));
        }));

    const auto stack_positions_callable = [](const runtime_inputs& inputs) {
        const auto records = stack_positions(
            require_frame(inputs, "positions"),
            optional_bool(inputs, "pos_in_dollars", true),
            optional_string(inputs, "cash_column_name", "cash"));
        frame_builder builder(ordinal_index(records.size()));
        std::vector<timestamp> dates;
        std::vector<std::string> symbols;
        std::vector<double> values;
        dates.reserve(records.size());
        symbols.reserve(records.size());
        values.reserve(records.size());
        for (const auto& record : records) {
            dates.push_back(record.date);
            symbols.push_back(record.symbol);
            values.push_back(record.value);
        }
        builder.add_timestamp("date", std::move(dates));
        builder.add_string("symbol", std::move(symbols));
        builder.add_double("value", std::move(values));
        return runtime_value(builder.finish());
    };

    operations.push_back(make_spec(
        "portfolio.positions.extract_pos",
        "fincore.portfolio.positions.extract_pos", "stack_positions",
        R"({"required":{"positions":"dataframe"},"optional":{"pos_in_dollars":"boolean","cash_column_name":"string"}})",
        true, std::string{no_rng}, stack_positions_callable));

    operations.push_back(make_spec(
        "portfolio.positions.get_long_short_notional",
        "fincore.portfolio.positions.get_long_short_notional",
        "get_long_short_notional",
        R"({"required":{"positions":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto& positions = require_frame(inputs, "positions");
            const auto result = get_long_short_notional(
                positions,
                optional_string(inputs, "cash_column_name", "cash"));
            frame_builder builder(positions.get_index());
            builder.add_double("long", result.first);
            builder.add_double("short", result.second);
            return runtime_value(builder.finish());
        }));

    operations.push_back(make_spec(
        "portfolio.positions.get_long_short_pos",
        "fincore.portfolio.positions.get_long_short_pos",
        "get_long_short_pos",
        R"({"required":{"positions":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(get_long_short_pos(
                require_frame(inputs, "positions"),
                optional_string(inputs, "cash_column_name", "cash")));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.get_max_median_position_concentration",
        "fincore.portfolio.positions.get_max_median_position_concentration",
        "get_max_median_position_concentration",
        R"({"required":{"positions":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(get_max_median_position_concentration(
                require_frame(inputs, "positions"),
                optional_string(inputs, "cash_column_name", "cash")));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.get_percent_alloc",
        "fincore.portfolio.positions.get_percent_alloc", "get_percent_alloc",
        R"({"required":{"positions":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(
                get_percent_alloc(require_frame(inputs, "positions")));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.get_sector_exposures",
        "fincore.portfolio.positions.get_sector_exposures",
        "get_sector_exposures",
        R"({"required":{"positions":"dataframe","symbol_sector_map":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto assignments = sector_assignments_from(
                require_frame(inputs, "symbol_sector_map"));
            return runtime_value(get_sector_exposures(
                require_frame(inputs, "positions"), assignments,
                optional_string(inputs, "cash_column_name", "cash")));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.get_top_long_short_abs",
        "fincore.portfolio.positions.get_top_long_short_abs",
        "get_top_long_short_abs",
        R"({"required":{"positions":"dataframe"},"optional":{"top":"integer","cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_top_positions(
                get_top_long_short_abs(
                    require_frame(inputs, "positions"),
                    optional_size(inputs, "top", 10U),
                    optional_string(inputs, "cash_column_name", "cash"))));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.gross_lev",
        "fincore.portfolio.positions.gross_lev", "gross_lev",
        R"({"required":{"positions":"dataframe"},"optional":{"cash_column_name":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(gross_lev(
                require_frame(inputs, "positions"),
                optional_string(inputs, "cash_column_name", "cash")));
        }));

    operations.push_back(make_spec(
        "portfolio.positions.stack_positions",
        "fincore.portfolio.positions.stack_positions", "stack_positions",
        R"({"required":{"positions":"dataframe"},"optional":{"pos_in_dollars":"boolean","cash_column_name":"string"}})",
        true, std::string{no_rng}, stack_positions_callable));

    operations.push_back(make_spec(
        "portfolio.round_trips.add_closing_transactions",
        "fincore.portfolio.round_trips.add_closing_transactions",
        "add_closing_transactions",
        R"({"required":{"positions":"dataframe","transactions":"portfolio_transactions"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto trades = round_trip_transactions(
                require_transactions(inputs, "transactions"));
            const auto result = add_closing_transactions(
                require_frame(inputs, "positions"), trades);
            frame_builder builder(ordinal_index(result.size()));
            std::vector<timestamp> dates;
            std::vector<std::string> symbols;
            std::vector<double> amounts;
            std::vector<double> prices;
            dates.reserve(result.size());
            symbols.reserve(result.size());
            amounts.reserve(result.size());
            prices.reserve(result.size());
            for (const auto& transaction : result) {
                dates.push_back(transaction.date);
                symbols.push_back(transaction.symbol);
                amounts.push_back(transaction.amount);
                prices.push_back(transaction.price);
            }
            builder.add_timestamp("date", std::move(dates));
            builder.add_string("symbol", std::move(symbols));
            builder.add_double("amount", std::move(amounts));
            builder.add_double("price", std::move(prices));
            return runtime_value(builder.finish());
        }));

    operations.push_back(make_spec(
        "portfolio.round_trips.agg_all_long_short",
        "fincore.portfolio.round_trips.agg_all_long_short",
        "agg_all_long_short",
        R"({"required":{"round_trips":"dataframe","column":"string","statistics":"string"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto statistics =
                round_trip_statistics_from(require_string(inputs,
                                                          "statistics"));
            return runtime_value(serialize_long_short_aggregate(
                agg_all_long_short(require_frame(inputs, "round_trips"),
                                   require_string(inputs, "column"),
                                   statistics)));
        }));

    operations.push_back(make_spec(
        "portfolio.round_trips.apply_sector_mappings_to_round_trips",
        "fincore.portfolio.round_trips.apply_sector_mappings_to_round_trips",
        "apply_sector_mappings_to_round_trips",
        R"({"required":{"round_trips":"dataframe","sector_mappings":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(apply_sector_mappings_to_round_trips(
                require_frame(inputs, "round_trips"),
                sector_mapping_map(
                    require_frame(inputs, "sector_mappings"))));
        }));

    operations.push_back(make_spec(
        "portfolio.round_trips.extract_round_trips",
        "fincore.portfolio.round_trips.extract_round_trips",
        "extract_round_trips",
        R"({"required":{"transactions":"dataframe"},"optional":{"max_delta_nanoseconds":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto max_delta = optional_integer(inputs,
                                                    "max_delta_nanoseconds");
            return runtime_value(extract_round_trips(
                require_frame(inputs, "transactions"),
                std::chrono::nanoseconds{max_delta.value_or(eight_hours_ns)}));
        }));

    operations.push_back(make_spec(
        "portfolio.round_trips.gen_round_trip_stats",
        "fincore.portfolio.round_trips.gen_round_trip_stats",
        "gen_round_trip_stats",
        R"({"required":{"round_trips":"dataframe"},"optional":{"pnl_column":"string","returns_column":"string","long_column":"string","symbol_column":"string","open_column":"string","close_column":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_round_trip_stats(
                gen_round_trip_stats(
                    require_frame(inputs, "round_trips"),
                    optional_string(inputs, "pnl_column", "pnl"),
                    optional_string(inputs, "returns_column", "rt_returns"),
                    optional_string(inputs, "long_column", "long"),
                    optional_string(inputs, "symbol_column", "symbol"),
                    optional_string(inputs, "open_column", "open_dt"),
                    optional_string(inputs, "close_column", "close_dt"))));
        }));

    const auto groupby_consecutive_callable =
        [](const runtime_inputs& inputs) {
            const auto max_delta =
                optional_integer(inputs, "max_delta_nanoseconds");
            return runtime_value(groupby_consecutive_transactions(
                require_frame(inputs, "transactions"),
                std::chrono::nanoseconds{
                    max_delta.value_or(eight_hours_ns)}));
        };

    operations.push_back(make_spec(
        "portfolio.round_trips.groupby_consecutive",
        "fincore.portfolio.round_trips.groupby_consecutive",
        "groupby_consecutive_transactions",
        R"({"required":{"transactions":"dataframe"},"optional":{"max_delta_nanoseconds":"integer"}})",
        true, std::string{no_rng}, groupby_consecutive_callable));

    operations.push_back(make_spec(
        "portfolio.transactions.adjust_returns_for_slippage",
        "fincore.portfolio.transactions.adjust_returns_for_slippage",
        "adjust_returns_for_slippage",
        R"({"required":{"returns":"dataframe","positions":"dataframe","transactions":"portfolio_transactions","slippage_bps":"double"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto frame = transaction_frame(
                require_transactions(inputs, "transactions"));
            return runtime_value(adjust_returns_for_slippage(
                require_frame(inputs, "returns"),
                require_frame(inputs, "positions"), frame,
                require_number(inputs, "slippage_bps")));
        }));

    operations.push_back(make_spec(
        "portfolio.transactions.apply_slippage_penalty",
        "fincore.portfolio.transactions.apply_slippage_penalty",
        "apply_slippage_penalty",
        R"({"required":{"returns":"dataframe","transactions_with_bar_data":"dataframe","simulate_starting_capital":"double","backtest_starting_capital":"double"},"optional":{"impact":"double"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(apply_slippage_penalty(
                require_frame(inputs, "returns"),
                require_frame(inputs, "transactions_with_bar_data"),
                require_number(inputs, "simulate_starting_capital"),
                require_number(inputs, "backtest_starting_capital"),
                optional_number(inputs, "impact", 0.1)));
        }));

    operations.push_back(make_spec(
        "portfolio.transactions.daily_txns_with_bar_data",
        "fincore.portfolio.transactions.daily_txns_with_bar_data",
        "daily_txns_with_bar_data",
        R"({"required":{"transactions":"portfolio_transactions","prices":"dataframe","volumes":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto trades =
                trade_records(require_transactions(inputs, "transactions"));
            return runtime_value(serialize_daily_txn_bars(
                daily_txns_with_bar_data(trades,
                                         require_frame(inputs, "prices"),
                                         require_frame(inputs, "volumes"))));
        }));

    const auto days_to_liquidate_callable = [](const runtime_inputs& inputs) {
        return runtime_value(days_to_liquidate_positions(
            require_frame(inputs, "positions"),
            require_frame(inputs, "prices"),
            require_frame(inputs, "volumes"),
            optional_number(inputs, "max_bar_consumption", 0.2),
            optional_number(inputs, "capital_base", 1'000'000.0),
            optional_size(inputs, "mean_volume_window", 5U)));
    };

    operations.push_back(make_spec(
        "portfolio.transactions.days_to_liquidate_positions",
        "fincore.portfolio.transactions.days_to_liquidate_positions",
        "days_to_liquidate_positions",
        R"({"required":{"positions":"dataframe","prices":"dataframe","volumes":"dataframe"},"optional":{"max_bar_consumption":"double","capital_base":"double","mean_volume_window":"integer"}})",
        true, std::string{no_rng}, days_to_liquidate_callable));

    operations.push_back(make_spec(
        "portfolio.transactions.get_low_liquidity_transactions",
        "fincore.portfolio.transactions.get_low_liquidity_transactions",
        "get_low_liquidity_transactions",
        R"({"required":{"transactions":"portfolio_transactions","prices":"dataframe","volumes":"dataframe"},"optional":{"last_n_days":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto trades =
                trade_records(require_transactions(inputs, "transactions"));
            return runtime_value(serialize_transaction_liquidity(
                get_low_liquidity_transactions(
                    trades, require_frame(inputs, "prices"),
                    require_frame(inputs, "volumes"),
                    optional_size_value(inputs, "last_n_days"))));
        }));

    operations.push_back(make_spec(
        "portfolio.transactions.get_max_days_to_liquidate_by_ticker",
        "fincore.portfolio.transactions.get_max_days_to_liquidate_by_ticker",
        "get_max_days_to_liquidate_by_ticker",
        R"({"required":{"positions":"dataframe","prices":"dataframe","volumes":"dataframe"},"optional":{"max_bar_consumption":"double","capital_base":"double","mean_volume_window":"integer","last_n_days":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_ticker_liquidity(
                get_max_days_to_liquidate_by_ticker(
                    require_frame(inputs, "positions"),
                    require_frame(inputs, "prices"),
                    require_frame(inputs, "volumes"),
                    optional_number(inputs, "max_bar_consumption", 0.2),
                    optional_number(inputs, "capital_base", 1'000'000.0),
                    optional_size(inputs, "mean_volume_window", 5U),
                    optional_size_value(inputs, "last_n_days"))));
        }));

    operations.push_back(make_spec(
        "portfolio.transactions.get_turnover",
        "fincore.portfolio.transactions.get_turnover", "get_turnover",
        R"({"required":{"positions":"dataframe","transactions":"portfolio_transactions"},"optional":{"denominator":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto frame = transaction_frame(
                require_transactions(inputs, "transactions"));
            return runtime_value(get_turnover(
                require_frame(inputs, "positions"), frame,
                parse_turnover_denominator(
                    optional_string(inputs, "denominator", "AGB"))));
        }));

    operations.push_back(make_spec(
        "portfolio.transactions.get_txn_vol",
        "fincore.portfolio.transactions.get_txn_vol", "get_txn_vol",
        R"({"required":{"transactions":"portfolio_transactions"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto frame = transaction_frame(
                require_transactions(inputs, "transactions"));
            return runtime_value(get_txn_vol(frame));
        }));

    // -------------------------------------------------------------------- risk
    operations.push_back(make_spec(
        "risk.backtesting.backtest_es",
        "fincore.risk.backtesting.backtest_es", "backtest_es",
        R"({"required":{"forecast":"dataframe","realized":"dataframe"},"optional":{"confidence_level":"double","n_bootstrap":"integer","seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(serialize_backtest_result(backtest_es(
                require_frame(inputs, "forecast"),
                require_frame(inputs, "realized"),
                optional_number(inputs, "confidence_level", 0.975),
                optional_size(inputs, "n_bootstrap", 1000U),
                seed.has_value()
                    ? static_cast<std::uint64_t>(*seed)
                    : std::uint64_t{20260817U})));
        }));

    operations.push_back(make_spec(
        "risk.backtesting.backtest_var",
        "fincore.risk.backtesting.backtest_var", "backtest_var",
        R"({"required":{"forecast":"dataframe","realized":"dataframe"},"optional":{"confidence_level":"double","significance":"double"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_backtest_result(backtest_var(
                require_frame(inputs, "forecast"),
                require_frame(inputs, "realized"),
                optional_number(inputs, "confidence_level", 0.99),
                optional_number(inputs, "significance", 0.05))));
        }));

    operations.push_back(make_spec(
        "risk.backtesting.christoffersen_lr",
        "fincore.risk.backtesting.christoffersen_lr", "christoffersen_lr",
        R"({"required":{"exceptions":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto flags = exception_flags(
                require_frame(inputs, "exceptions"), "exceptions");
            return runtime_value(christoffersen_lr(flags));
        }));

    operations.push_back(make_spec(
        "risk.calibration.es_calibration_score",
        "fincore.risk.calibration.es_calibration_score",
        "es_calibration_score",
        R"({"required":{"forecast_es":"double","realized":"dataframe","confidence_level":"double"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto& realized = require_frame(inputs, "realized");
            const auto column = realized.has_column("realized")
                                    ? std::string{"realized"}
                                    : std::string{"returns"};
            const auto values =
                require_double_column(realized, column);
            return runtime_value(es_calibration_score(
                require_number(inputs, "forecast_es"), values,
                require_number(inputs, "confidence_level")));
        }));

    operations.push_back(make_spec(
        "risk.evt.evt_cvar", "fincore.risk.evt.evt_cvar", "evt_cvar",
        R"({"required":{"data":"dataframe"},"optional":{"alpha":"double","model":"string","tail":"string","threshold":"double","block_size":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(evt_cvar(
                require_frame(inputs, "data"),
                optional_number(inputs, "alpha", 0.05),
                parse_evt_model(optional_string(inputs, "model", "gpd")),
                parse_evt_tail(optional_string(inputs, "tail", "lower")),
                optional_number_value(inputs, "threshold"),
                optional_size_value(inputs, "block_size")));
        }));

    operations.push_back(make_spec(
        "risk.evt.evt_var", "fincore.risk.evt.evt_var", "evt_var",
        R"({"required":{"data":"dataframe"},"optional":{"alpha":"double","model":"string","tail":"string","threshold":"double","block_size":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(evt_var(
                require_frame(inputs, "data"),
                optional_number(inputs, "alpha", 0.05),
                parse_evt_model(optional_string(inputs, "model", "gpd")),
                parse_evt_tail(optional_string(inputs, "tail", "lower")),
                optional_number_value(inputs, "threshold"),
                optional_size_value(inputs, "block_size")));
        }));

    operations.push_back(make_spec(
        "risk.evt.extreme_risk", "fincore.risk.evt.extreme_risk",
        "extreme_risk",
        R"({"required":{"returns":"dataframe"},"optional":{"alpha":"double","tail":"string","model":"string","threshold":"double","block_size":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_extreme_risk(extreme_risk(
                require_frame(inputs, "returns"),
                optional_number(inputs, "alpha", 0.05),
                parse_evt_tail(optional_string(inputs, "tail", "lower")),
                parse_evt_model(optional_string(inputs, "model", "gpd")),
                optional_number_value(inputs, "threshold"),
                optional_size_value(inputs, "block_size"))));
        }));

    operations.push_back(make_spec(
        "risk.evt.gev_fit", "fincore.risk.evt.gev_fit", "gev_fit",
        R"({"required":{"data":"dataframe"},"optional":{"block_size":"integer","tail":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_gev_parameters(gev_fit(
                require_frame(inputs, "data"),
                optional_size_value(inputs, "block_size"),
                parse_evt_tail(optional_string(inputs, "tail", "lower")))));
        }));

    operations.push_back(make_spec(
        "risk.evt.gpd_fit", "fincore.risk.evt.gpd_fit", "gpd_fit",
        R"({"required":{"data":"dataframe"},"optional":{"threshold":"double","method":"string","tail":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_gpd_parameters(gpd_fit(
                require_frame(inputs, "data"),
                optional_number_value(inputs, "threshold"),
                parse_gpd_method(optional_string(inputs, "method", "mle")),
                parse_evt_tail(optional_string(inputs, "tail", "lower")))));
        }));

    operations.push_back(make_spec(
        "risk.evt.hill_estimator", "fincore.risk.evt.hill_estimator",
        "hill_estimator",
        R"({"required":{"data":"dataframe"},"optional":{"threshold":"double","tail":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_hill_estimate(hill_estimator(
                require_frame(inputs, "data"),
                optional_number_value(inputs, "threshold"),
                parse_evt_tail(optional_string(inputs, "tail", "upper")))));
        }));

    operations.push_back(make_spec(
        "risk.garch.conditional_es", "fincore.risk.garch.conditional_es",
        "conditional_es",
        R"({"required":{"returns":"dataframe"},"optional":{"model":"string","alpha":"double","horizon":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_garch_conditional(
                conditional_es(
                    require_frame(inputs, "returns"),
                    parse_garch_model(
                        optional_string(inputs, "model", "GARCH")),
                    optional_number(inputs, "alpha", 0.05),
                    optional_size(inputs, "horizon", 1U))));
        }));

    operations.push_back(make_spec(
        "risk.garch.conditional_var", "fincore.risk.garch.conditional_var",
        "conditional_var",
        R"({"required":{"returns":"dataframe"},"optional":{"model":"string","alpha":"double","horizon":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_garch_conditional(
                conditional_var(
                    require_frame(inputs, "returns"),
                    parse_garch_model(
                        optional_string(inputs, "model", "GARCH")),
                    optional_number(inputs, "alpha", 0.05),
                    optional_size(inputs, "horizon", 1U))));
        }));

    operations.push_back(make_spec(
        "risk.garch.forecast_volatility",
        "fincore.risk.garch.forecast_volatility", "forecast_volatility",
        R"({"required":{"returns":"dataframe"},"optional":{"model":"string","horizon":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_garch_forecast(
                forecast_volatility(
                    require_frame(inputs, "returns"),
                    parse_garch_model(
                        optional_string(inputs, "model", "GARCH")),
                    optional_size(inputs, "horizon", 1U))));
        }));

    const auto forecast_impl = [](const runtime_inputs& inputs,
                                  const bool expected_shortfall) {
        risk_forecast_config config;
        config.method = parse_forecast_method(
            optional_string(inputs, "method", "historical"));
        config.confidence_level =
            optional_number(inputs, "confidence_level", 0.95);
        config.horizon = optional_size(inputs, "horizon", 1U);
        config.garch_kernel = parse_garch_model(
            optional_string(inputs, "garch_kernel", "GARCH"));
        config.evt_kernel = parse_evt_model(
            optional_string(inputs, "evt_kernel", "gpd"));
        config.tail =
            parse_evt_tail(optional_string(inputs, "tail", "lower"));
        config.threshold = optional_number_value(inputs, "threshold");
        config.block_size = optional_size_value(inputs, "block_size");
        const auto& returns = require_frame(inputs, "returns");
        return runtime_value(serialize_risk_estimate(
            expected_shortfall ? forecast_es(returns, config)
                               : forecast_var(returns, config)));
    };

    operations.push_back(make_spec(
        "risk.models.forecast_es", "fincore.risk.models.forecast_es",
        "forecast_es",
        R"({"required":{"returns":"dataframe"},"optional":{"method":"string","confidence_level":"double","horizon":"integer","garch_kernel":"string","evt_kernel":"string","tail":"string","threshold":"double","block_size":"integer"}})",
        true, std::string{no_rng}, [forecast_impl](const runtime_inputs& inputs) {
            return forecast_impl(inputs, true);
        }));

    operations.push_back(make_spec(
        "risk.models.forecast_var", "fincore.risk.models.forecast_var",
        "forecast_var",
        R"({"required":{"returns":"dataframe"},"optional":{"method":"string","confidence_level":"double","horizon":"integer","garch_kernel":"string","evt_kernel":"string","tail":"string","threshold":"double","block_size":"integer"}})",
        true, std::string{no_rng}, [forecast_impl](const runtime_inputs& inputs) {
            return forecast_impl(inputs, false);
        }));

    // -------------------------------------------------------------- simulation
    operations.push_back(make_spec(
        "simulation.base.annualize", "fincore.simulation.base.annualize",
        "annualize",
        R"({"required":{"daily_value":"double"},"optional":{"period":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(
                annualize(require_number(inputs, "daily_value"),
                          optional_string(inputs, "period", "daily")));
        }));

    const auto compute_statistics_callable = [](const runtime_inputs& inputs) {
        const auto result =
            analyze_simulation_paths(require_frame(inputs, "paths"));
        return runtime_value(
            serialize_simulation_statistics(result.statistics()));
    };

    operations.push_back(make_spec(
        "simulation.base.compute_statistics",
        "fincore.simulation.base.compute_statistics",
        "analyze_simulation_paths",
        R"({"required":{"paths":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, compute_statistics_callable));

    operations.push_back(make_spec(
        "simulation.base.estimate_parameters",
        "fincore.simulation.base.estimate_parameters",
        "estimate_simulation_parameters",
        R"({"required":{"returns":"dataframe"},"optional":{"frequency":"integer"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(serialize_simulation_parameters(
                estimate_simulation_parameters(
                    require_frame(inputs, "returns"),
                    optional_size(inputs, "frequency", 252U))));
        }));

    operations.push_back(make_spec(
        "simulation.base.validate_returns",
        "fincore.simulation.base.validate_returns", "validate_returns",
        R"({"required":{"returns":"dataframe"},"optional":{"column":"string"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto& returns = require_frame(inputs, "returns");
            const auto column = optional_string(inputs, "column", "returns");
            auto values = validate_returns(returns, column);
            return runtime_value(
                make_series(returns.get_index(), column, std::move(values)));
        }));

    operations.push_back(make_spec(
        "simulation.bootstrap.bootstrap",
        "fincore.simulation.bootstrap.bootstrap", "bootstrap_distribution",
        R"({"required":{"returns":"dataframe"},"optional":{"n_samples":"integer","statistic":"string","seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(bootstrap_distribution(
                require_frame(inputs, "returns"),
                optional_size(inputs, "n_samples", 10000U),
                parse_bootstrap_statistic(
                    optional_string(inputs, "statistic", "mean")),
                seed.has_value()
                    ? std::optional<std::uint64_t>{
                          static_cast<std::uint64_t>(*seed)}
                    : std::nullopt));
        }));

    operations.push_back(make_spec(
        "simulation.bootstrap.bootstrap_ci",
        "fincore.simulation.bootstrap.bootstrap_ci",
        "bootstrap_confidence_interval",
        R"({"required":{"returns":"dataframe"},"optional":{"n_samples":"integer","alpha":"double","statistic":"string","method":"string","seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto method =
                lowercase(optional_string(inputs, "method", "percentile"));
            if (method != "percentile") {
                throw input_error{"method must be 'percentile'"};
            }
            const auto seed = optional_integer(inputs, "seed");
            const auto result = bootstrap_confidence_interval(
                require_frame(inputs, "returns"),
                optional_size(inputs, "n_samples", 10000U),
                optional_number(inputs, "alpha", 0.05),
                parse_bootstrap_statistic(
                    optional_string(inputs, "statistic", "mean")),
                seed.has_value()
                    ? std::optional<std::uint64_t>{
                          static_cast<std::uint64_t>(*seed)}
                    : std::nullopt);
            frame_builder builder(ordinal_index(1U));
            builder.add_double("lower", {result.lower});
            builder.add_double("upper", {result.upper});
            return runtime_value(builder.finish());
        }));

    operations.push_back(make_spec(
        "simulation.bootstrap.bootstrap_summary",
        "fincore.simulation.bootstrap.bootstrap_summary",
        "bootstrap_summary",
        R"({"required":{"returns":"dataframe"},"optional":{"n_samples":"integer","alpha":"double","seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(serialize_bootstrap_summary(
                bootstrap_summary(
                    require_frame(inputs, "returns"),
                    optional_size(inputs, "n_samples", 10000U),
                    optional_number(inputs, "alpha", 0.05),
                    seed.has_value()
                        ? std::optional<std::uint64_t>{
                              static_cast<std::uint64_t>(*seed)}
                        : std::nullopt)));
        }));

    operations.push_back(make_spec(
        "simulation.monte_carlo.MonteCarlo.from_parameters",
        "fincore.simulation.monte_carlo.MonteCarlo.from_parameters",
        "simulate_monte_carlo_from_parameters",
        R"({"required":{"mu":"double","sigma":"double"},"optional":{"S0":"double","n_paths":"integer","horizon":"integer","seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(
                simulate_monte_carlo_from_parameters(
                    require_number(inputs, "mu"),
                    require_number(inputs, "sigma"),
                    optional_number(inputs, "S0", 1.0),
                    optional_size(inputs, "n_paths", 1000U),
                    optional_size(inputs, "horizon", 252U),
                    seed.has_value()
                        ? std::optional<std::uint64_t>{
                              static_cast<std::uint64_t>(*seed)}
                        : std::nullopt)
                    .paths());
        }));

    operations.push_back(make_spec(
        "simulation.paths.antithetic_variates",
        "fincore.simulation.paths.antithetic_variates",
        "antithetic_variates",
        R"({"required":{"paths":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(
                antithetic_variates(require_frame(inputs, "paths")));
        }));

    operations.push_back(make_spec(
        "simulation.paths.gbm_from_returns",
        "fincore.simulation.paths.gbm_from_returns", "gbm_from_returns",
        R"({"required":{"returns":"dataframe"},"optional":{"horizon":"integer","n_paths":"integer","frequency":"integer","seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(gbm_from_returns(
                require_frame(inputs, "returns"),
                optional_size(inputs, "horizon", 252U),
                optional_size(inputs, "n_paths", 1000U),
                optional_size(inputs, "frequency", 252U),
                seed.has_value()
                    ? std::optional<std::uint64_t>{
                          static_cast<std::uint64_t>(*seed)}
                    : std::nullopt));
        }));

    operations.push_back(make_spec(
        "simulation.paths.geometric_brownian_motion",
        "fincore.simulation.paths.geometric_brownian_motion",
        "geometric_brownian_motion",
        R"({"required":{"S0":"double","mu":"double","sigma":"double","T":"double","dt":"double","n_paths":"integer"},"optional":{"seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(geometric_brownian_motion(
                require_number(inputs, "S0"), require_number(inputs, "mu"),
                require_number(inputs, "sigma"),
                require_number(inputs, "T"), require_number(inputs, "dt"),
                require_size(inputs, "n_paths"),
                seed.has_value()
                    ? std::optional<std::uint64_t>{
                          static_cast<std::uint64_t>(*seed)}
                    : std::nullopt));
        }));

    operations.push_back(make_spec(
        "simulation.paths.latin_hypercube_sampling",
        "fincore.simulation.paths.latin_hypercube_sampling",
        "latin_hypercube_sampling",
        R"({"required":{"n_samples":"integer","n_dimensions":"integer"},"optional":{"seed":"integer"}})",
        false, std::string{seeded_rng}, [](const runtime_inputs& inputs) {
            const auto seed = optional_integer(inputs, "seed");
            return runtime_value(latin_hypercube_sampling(
                require_size(inputs, "n_samples"),
                require_size(inputs, "n_dimensions"),
                seed.has_value()
                    ? std::optional<std::uint64_t>{
                          static_cast<std::uint64_t>(*seed)}
                    : std::nullopt));
        }));

    operations.push_back(make_spec(
        "simulation.scenarios.generate_correlation_breakdown",
        "fincore.simulation.scenarios.generate_correlation_breakdown",
        "generate_correlation_breakdown",
        R"({"required":{"n_assets":"integer"},"optional":{"target_correlation":"double"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto count = require_size(inputs, "n_assets");
            std::vector<std::string> owned;
            owned.reserve(count);
            for (std::size_t position = 0U; position < count; ++position) {
                owned.push_back("asset_" + std::to_string(position + 1U));
            }
            std::vector<std::string_view> names;
            names.reserve(owned.size());
            for (const auto& name : owned) {
                names.push_back(name);
            }
            return runtime_value(generate_correlation_breakdown(
                names, optional_number(inputs, "target_correlation", 1.0)));
        }));

    operations.push_back(make_spec(
        "simulation.scenarios.scenario_table",
        "fincore.simulation.scenarios.scenario_table", "scenario_table",
        R"({"required":{"stress_results":"dataframe"},"optional":{}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto results = stress_results_from_table(
                require_frame(inputs, "stress_results"));
            return runtime_value(scenario_table(results));
        }));

    operations.push_back(make_spec(
        "simulation.scenarios.stress_test",
        "fincore.simulation.scenarios.stress_test", "stress_test",
        R"({"required":{"returns":"dataframe"},"optional":{"scenarios":"string","custom_scenarios":"dataframe"}})",
        true, std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto& returns = require_frame(inputs, "returns");
            const auto* custom = optional_frame(inputs, "custom_scenarios");
            auto custom_scenarios =
                custom == nullptr ? std::vector<custom_stress_scenario>{}
                                  : custom_scenarios_from(*custom);
            std::vector<stress_scenario_result> results;
            if (inputs.find("scenarios") == inputs.end() &&
                custom == nullptr) {
                results = stress_test(returns);
            } else {
                auto scenarios = std::vector<predefined_stress_scenario>{};
                if (inputs.find("scenarios") != inputs.end()) {
                    scenarios = stress_scenarios_from(
                        require_string(inputs, "scenarios"));
                }
                results =
                    stress_test(returns, scenarios, custom_scenarios);
            }
            return runtime_value(serialize_stress_results(results,
                                                          "returns"));
        }));

    operations.push_back(make_spec(
        "portfolio.transactions.map_transaction",
        "fincore.portfolio.transactions.map_transaction", "map_transaction",
        R"({"required":{"transaction":"portfolio_transactions"}})", true,
        std::string{no_rng}, [](const runtime_inputs& inputs) {
            const auto transactions =
                require_transactions(inputs, "transaction");
            if (transactions.size() != 1U) {
                throw input_error{
                    "map_transaction expects exactly one transaction"};
            }
            return runtime_value(std::vector<portfolio_transaction>{
                map_transaction(transactions.front())});
        }));
    operations.push_back(make_spec(
        "portfolio.transactions.make_transaction_frame",
        "fincore.portfolio.transactions.make_transaction_frame",
        "make_transaction_frame",
        R"({"required":{"transactions":"portfolio_transactions"}})", true,
        std::string{no_rng}, [](const runtime_inputs& inputs) {
            return runtime_value(make_transaction_frame(
                require_transactions(inputs, "transactions")));
        }));

    return operations;
}

}  // namespace fincore
