#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/research_detailed.hpp>
#include <fincore/factor/serialization.hpp>
#include "../../src/factor/research_detailed_internal.hpp"

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using fincore::factor_value;

const factor_value& at(const factor_value& value, const std::string_view key) {
    const auto* found = value.find(key);
    if (found == nullptr) {
        throw std::runtime_error{"missing oracle field: " + std::string{key}};
    }
    return *found;
}

double number(const factor_value& value) {
    if (value.is_null()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (value.is_number()) {
        return value.as_number();
    }
    if (value.is_integer()) {
        return static_cast<double>(value.as_integer());
    }
    throw std::runtime_error{"oracle value is not numeric"};
}

std::string string(const factor_value& value) {
    return value.as_string();
}

std::size_t size(const factor_value& value) {
    return static_cast<std::size_t>(value.as_integer());
}

fincore::timestamp timestamp(const factor_value& value) {
    const auto text = string(value);
    const auto integer = [&text](const std::size_t offset,
                                 const std::size_t count) {
        return std::stoi(text.substr(offset, count));
    };
    const int year = integer(0U, 4U);
    const auto month = static_cast<unsigned>(integer(5U, 2U));
    const auto day = static_cast<unsigned>(integer(8U, 2U));
    const auto hour = static_cast<unsigned>(integer(11U, 2U));
    const auto minute = static_cast<unsigned>(integer(14U, 2U));
    const auto second = static_cast<unsigned>(integer(17U, 2U));
    unsigned nanosecond = 0U;
    const auto dot = text.find('.');
    if (dot != std::string::npos) {
        const auto end = text.find('Z', dot);
        const std::string fraction = text.substr(dot + 1U, end - dot - 1U);
        std::string padded = fraction.substr(0U, 9U);
        padded.append(9U - padded.size(), '0');
        nanosecond = static_cast<unsigned>(std::stoul(padded));
    }
    return fincore::utc_timestamp(year, month, day, hour, minute, second,
                                  nanosecond);
}

void expect_number(const factor_value& expected, const double actual,
                   const std::string_view field) {
    SCOPED_TRACE(std::string{"field="} + std::string{field});
    const double reference = number(expected);
    if (std::isnan(reference)) {
        EXPECT_TRUE(std::isnan(actual));
        return;
    }
    if (std::isinf(reference)) {
        EXPECT_TRUE(std::isinf(actual));
        EXPECT_EQ(std::signbit(actual), std::signbit(reference));
        return;
    }
    ASSERT_TRUE(std::isfinite(actual));
    EXPECT_LE(std::abs(actual - reference), 1e-6);
}

factor_value load_oracle() {
    const auto path = std::filesystem::path{__FILE__}
                          .parent_path()
                          .parent_path() /
                      "data" / "research_detailed_oracle.json";
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error{"could not open research detailed oracle: " +
                                 path.string()};
    }
    std::ostringstream contents;
    contents << stream.rdbuf();
    return fincore::parse_json_value(contents.str());
}

struct oracle_inputs {
    fincore::research_spec spec;
    fincore::dataframe prices;
    std::vector<std::string> assets;
    const factor_value::array_type* factor_rows = nullptr;
};

oracle_inputs make_inputs(const factor_value& oracle) {
    const auto& input = at(oracle, "input");
    const auto& source_spec = at(oracle, "spec");
    oracle_inputs result;
    result.factor_rows = &at(input, "factor_rows").as_array();
    for (const auto& asset : at(input, "assets").as_array()) {
        result.assets.push_back(string(asset));
    }
    for (const auto& target : at(source_spec, "targets").as_array()) {
        result.spec.targets.push_back(string(target));
    }
    result.spec.start = timestamp(at(source_spec, "start"));
    result.spec.end = timestamp(at(source_spec, "end"));
    result.spec.label = string(at(source_spec, "label"));
    result.spec.preprocess = string(at(source_spec, "preprocess"));
    result.spec.snapshot_id = string(at(source_spec, "snapshot_id"));
    result.spec.price_basis = string(at(source_spec, "price_basis"));
    result.spec.calendar_id = string(at(source_spec, "calendar_id"));
    result.spec.frequency = string(at(source_spec, "frequency"));
    result.spec.periods.clear();
    for (const auto& period : at(source_spec, "periods").as_array()) {
        result.spec.periods.push_back(static_cast<int>(period.as_integer()));
    }
    result.spec.quantiles =
        static_cast<int>(at(source_spec, "quantiles").as_integer());
    result.spec.min_assets =
        static_cast<int>(at(source_spec, "min_assets").as_integer());
    result.spec.min_quantile_assets = static_cast<int>(
        at(source_spec, "min_quantile_assets").as_integer());
    result.spec.max_loss = number(at(source_spec, "max_loss"));
    result.spec.stability_lags.clear();
    for (const auto& lag : at(source_spec, "stability_lags").as_array()) {
        result.spec.stability_lags.push_back(static_cast<int>(lag.as_integer()));
    }

    const auto& price_rows =
        at(at(input, "prices"), result.spec.price_basis).as_array();
    std::vector<fincore::timestamp> dates;
    std::vector<std::vector<double>> columns(result.assets.size());
    for (const auto& row : price_rows) {
        dates.push_back(timestamp(at(row, "time")));
        const auto& values = at(row, "values");
        for (std::size_t asset = 0; asset < result.assets.size(); ++asset) {
            columns[asset].push_back(number(at(values, result.assets[asset])));
        }
    }
    result.prices.load_index(std::move(dates));
    for (std::size_t asset = 0; asset < result.assets.size(); ++asset) {
        result.prices.load_column<double>(
            result.assets[asset].c_str(), std::move(columns[asset]),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

fincore::factor_series make_factor_series(
    const factor_value::array_type& rows, const std::string& factor_name) {
    fincore::factor_series result;
    result.dates.reserve(rows.size());
    result.assets.reserve(rows.size());
    result.values.reserve(rows.size());
    for (const auto& row : rows) {
        result.dates.push_back(timestamp(at(row, "time")));
        result.assets.push_back(string(at(row, "asset")));
        result.values.push_back(
            number(at(at(row, "values"), factor_name)));
    }
    return result;
}

void compare_signal_rows(const factor_value& expected,
                         const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=signals");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.signals.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.signals[index];
        EXPECT_EQ(value.date, timestamp(at(row, "time")));
        EXPECT_EQ(value.asset, string(at(row, "asset")));
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.direction, at(row, "direction").as_integer());
        expect_number(at(row, "value"), value.value, "value");
        expect_number(at(row, "processed_value"), value.processed_value,
                      "processed_value");
        EXPECT_EQ(value.preprocess, string(at(row, "preprocess")));
        EXPECT_EQ(value.group, string(at(row, "group")));
    }
}

void compare_label_rows(const factor_value& expected,
                        const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=labels");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.labels.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.labels[index];
        EXPECT_EQ(value.date, timestamp(at(row, "time")));
        EXPECT_EQ(value.asset, string(at(row, "asset")));
        EXPECT_EQ(value.period, string(at(row, "period")));
        expect_number(at(row, "value"), value.value, "value");
    }
}

void compare_ic_rows(const factor_value& expected,
                     const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=ic");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.ic.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.ic[index];
        EXPECT_EQ(value.time, timestamp(at(row, "time")));
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.period, string(at(row, "period")));
        expect_number(at(row, "ic"), value.ic, "ic");
        expect_number(at(row, "rank_ic"), value.rank_ic, "rank_ic");
        EXPECT_EQ(value.n_assets, size(at(row, "n_assets")));
    }
}

void compare_summary_rows(
    const factor_value& expected,
    const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=ic_summary");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.ic_summary.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.ic_summary[index];
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.period, string(at(row, "period")));
        EXPECT_EQ(value.n_dates, size(at(row, "n_dates")));
        for (const auto* name : {"mean_ic", "std_ic", "icir",
                                 "mean_rank_ic", "std_rank_ic",
                                 "rank_icir"}) {
            const double actual_value =
                std::string_view{name} == "mean_ic" ? value.mean_ic
                : std::string_view{name} == "std_ic" ? value.std_ic
                : std::string_view{name} == "icir" ? value.icir
                : std::string_view{name} == "mean_rank_ic" ? value.mean_rank_ic
                : std::string_view{name} == "std_rank_ic" ? value.std_rank_ic
                                                           : value.rank_icir;
            expect_number(at(row, name), actual_value, name);
        }
    }
}

void compare_quantile_rows(
    const factor_value& expected,
    const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=quantile_returns");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.quantile_returns.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.quantile_returns[index];
        EXPECT_EQ(value.time, timestamp(at(row, "time")));
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.period, string(at(row, "period")));
        EXPECT_EQ(value.quantile, at(row, "quantile").as_integer());
        expect_number(at(row, "mean_return"), value.mean_return,
                      "mean_return");
        EXPECT_EQ(value.n_assets, size(at(row, "n_assets")));
    }
}

void compare_turnover_rows(
    const factor_value& expected,
    const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=turnover");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.turnover.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.turnover[index];
        EXPECT_EQ(value.time, timestamp(at(row, "time")));
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.period, string(at(row, "period")));
        EXPECT_EQ(value.quantile, at(row, "quantile").as_integer());
        expect_number(at(row, "turnover"), value.turnover, "turnover");
    }
}

void compare_spread_rows(const factor_value& expected,
                         const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=spreads");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.spreads.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.spreads[index];
        EXPECT_EQ(value.time, timestamp(at(row, "time")));
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.period, string(at(row, "period")));
        expect_number(at(row, "spread"), value.spread, "spread");
        EXPECT_EQ(value.top_quantile, at(row, "top_quantile").as_integer());
        EXPECT_EQ(value.bottom_quantile,
                  at(row, "bottom_quantile").as_integer());
    }
}

void compare_stability_rows(
    const factor_value& expected,
    const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=stability");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.stability.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.stability[index];
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.lag, at(row, "lag").as_integer());
        expect_number(at(row, "mean_rank_autocorrelation"),
                      value.mean_rank_autocorrelation,
                      "mean_rank_autocorrelation");
        expect_number(at(row, "std_rank_autocorrelation"),
                      value.std_rank_autocorrelation,
                      "std_rank_autocorrelation");
        EXPECT_EQ(value.n_dates, size(at(row, "n_dates")));
    }
}

void compare_coverage_rows(
    const factor_value& expected,
    const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=coverage");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.coverage.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.coverage[index];
        EXPECT_EQ(value.factor, string(at(row, "factor")));
        EXPECT_EQ(value.kind, string(at(row, "kind")));
        EXPECT_EQ(value.direction, at(row, "direction").as_integer());
        EXPECT_EQ(value.input_cells, size(at(row, "input_cells")));
        EXPECT_EQ(value.finite_cells, size(at(row, "finite_cells")));
        EXPECT_EQ(value.label_cells, size(at(row, "label_cells")));
        EXPECT_EQ(value.evaluable_cells, size(at(row, "evaluable_cells")));
        EXPECT_EQ(value.status, string(at(row, "status")));
    }
}

void compare_diagnostic_rows(
    const factor_value& expected,
    const fincore::research_detailed_result& actual) {
    SCOPED_TRACE("table=diagnostics");
    const auto& rows = at(expected, "rows").as_array();
    ASSERT_EQ(actual.diagnostics.size(), rows.size());
    for (std::size_t index = 0; index < rows.size(); ++index) {
        SCOPED_TRACE(std::string{"row="} + std::to_string(index));
        const auto& row = rows[index];
        const auto& value = actual.diagnostics[index];
        const auto& factor = at(row, "factor");
        if (factor.is_null()) {
            EXPECT_FALSE(value.factor.has_value());
        } else {
            ASSERT_TRUE(value.factor.has_value());
            EXPECT_EQ(*value.factor, string(factor));
        }
        EXPECT_EQ(value.stage, string(at(row, "stage")));
        EXPECT_EQ(value.severity, string(at(row, "severity")));
        EXPECT_EQ(value.code, string(at(row, "code")));
        EXPECT_EQ(value.message, string(at(row, "message")));
    }
}

const factor_value& resolve_table_with_rows(const factor_value& oracle,
                                            const factor_value& table) {
    if (table.find("rows") != nullptr) {
        return table;
    }
    const auto reference = string(at(table, "values_from"));
    if (reference == "common_tables.labels") {
        return at(at(oracle, "common_tables"), "labels");
    }
    throw std::runtime_error{"unsupported oracle table reference: " +
                             reference};
}

TEST(FactorResearchDetailed, MatchesPinnedPythonPublicOracle) {
    const auto oracle = load_oracle();
    const auto inputs = make_inputs(oracle);
    const auto& cases = at(oracle, "cases").as_array();
    ASSERT_EQ(cases.size(), 3U);

    for (const auto& item : cases) {
        const std::string factor = string(at(item, "factor"));
        SCOPED_TRACE(std::string{"factor="} + factor);
        auto spec = inputs.spec;
        for (const auto& [name, direction] :
             at(item, "direction_overrides").as_object()) {
            spec.direction_overrides[name] =
                static_cast<int>(direction.as_integer());
        }
        const auto factors = make_factor_series(*inputs.factor_rows, factor);
        const auto actual = fincore::evaluate_factor_research_detailed(
            factor, factors, inputs.prices, spec);
        EXPECT_EQ(actual.status, string(at(item, "status"))) << factor;

        const auto& expected_tables = at(item, "tables");
        compare_signal_rows(at(expected_tables, "signals"), actual);
        compare_label_rows(
            resolve_table_with_rows(oracle, at(expected_tables, "labels")),
            actual);
        compare_ic_rows(at(expected_tables, "ic"), actual);
        compare_summary_rows(at(expected_tables, "ic_summary"), actual);
        compare_quantile_rows(at(expected_tables, "quantile_returns"), actual);
        compare_turnover_rows(at(expected_tables, "turnover"), actual);
        compare_spread_rows(at(expected_tables, "spreads"), actual);
        compare_stability_rows(at(expected_tables, "stability"), actual);
        compare_coverage_rows(at(expected_tables, "coverage"), actual);
        compare_diagnostic_rows(at(expected_tables, "diagnostics"), actual);
        EXPECT_TRUE(actual.correlation.empty());
        EXPECT_TRUE(actual.comparisons.empty());
        EXPECT_EQ(actual.schema_version, 2);
        EXPECT_EQ(actual.manifest.at("factor"), factor);
        EXPECT_EQ(actual.manifest.at("api_version"), "2.0");
    }
}

TEST(FactorResearchDetailed, RejectsUnsupportedSingleFactorModes) {
    fincore::factor_series factors;
    const auto start = fincore::utc_timestamp(2024, 1, 8);
    factors.dates = {start};
    factors.assets = {"A"};
    factors.values = {1.0};
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start + 1});
    prices.load_column<double>("A", {1.0, 2.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    fincore::research_spec spec;
    spec.start = start;
    spec.end = start;
    spec.targets = {"A"};
    spec.label = "precomputed";
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "factor", factors, prices, spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed, SupportsCloseToCloseWithPeriodMajorNaNTail) {
    const auto start = fincore::utc_timestamp(2024, 2, 5);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const std::vector<std::string> assets{"A", "B", "C"};
    fincore::factor_series factors;
    for (int date = 0; date < 3; ++date) {
        for (std::size_t asset = 0; asset < assets.size(); ++asset) {
            factors.dates.push_back(start + date * day);
            factors.assets.push_back(assets[asset]);
            factors.values.push_back(static_cast<double>(asset + 1U));
        }
    }
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day});
    const std::vector<std::vector<double>> price_paths{
        {10.0, 11.0, 12.1},
        {20.0, 21.0, 24.0},
        {30.0, 30.0, 33.0},
    };
    for (std::size_t asset = 0; asset < assets.size(); ++asset) {
        prices.load_column<double>(
            assets[asset].c_str(), price_paths[asset],
            hmdf::nan_policy::dont_pad_with_nans);
    }
    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start + 2 * day;
    spec.periods = {1};
    spec.quantiles = 3;
    spec.min_assets = 2;
    spec.label = "close_to_close";
    spec.preprocess = "none";
    spec.stability_lags = {1};

    const auto result = fincore::evaluate_factor_research_detailed(
        "close_signal", factors, prices, spec);
    ASSERT_EQ(result.labels.size(), 9U);
    EXPECT_EQ(result.labels[0].period, "1D");
    EXPECT_NEAR(result.labels[0].value, 0.1, 1e-12);
    EXPECT_FALSE(std::isfinite(result.labels.back().value));
    EXPECT_EQ(result.status, "OK");
    EXPECT_EQ(result.coverage.size(), 1U);
    EXPECT_EQ(result.coverage[0].input_cells, 9U);
    EXPECT_EQ(result.coverage[0].label_cells, 6U);
}

TEST(FactorResearchDetailed,
     KeepsCompactHorizonReducersAcrossSparseQuantileDates) {
    const auto start = fincore::utc_timestamp(2024, 2, 12);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<std::string> assets{"A", "B", "C", "D"};

    fincore::factor_series factors;
    for (int date = 0; date < 4; ++date) {
        for (std::size_t asset = 0; asset < assets.size(); ++asset) {
            factors.dates.push_back(start + date * day);
            factors.assets.push_back(assets[asset]);
            factors.values.push_back(static_cast<double>(asset + 1U));
        }
    }

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day,
        start + 4 * day});
    const std::vector<std::vector<double>> price_paths{
        {10.0, 11.0, 12.0, 13.0, 14.0},
        {20.0, 21.0, 22.0, 23.0, 24.0},
        {30.0, 31.0, nan, 33.0, 34.0},
        {40.0, 41.0, nan, 44.0, 45.0},
    };
    for (std::size_t asset = 0; asset < assets.size(); ++asset) {
        prices.load_column<double>(
            assets[asset].c_str(), price_paths[asset],
            hmdf::nan_policy::dont_pad_with_nans);
    }

    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start + 3 * day;
    spec.periods = {1, 2};
    spec.quantiles = 3;
    spec.min_assets = 3;
    spec.min_quantile_assets = 2;
    spec.max_loss = 1.0;
    spec.stability_lags = {1};
    spec.label = "close_to_close";
    spec.preprocess = "none";

    const auto result = fincore::evaluate_factor_research_detailed(
        "sparse_horizon_signal", factors, prices, spec);

    ASSERT_EQ(result.labels.size(), 32U);
    bool one_day_c_is_missing = false;
    bool two_day_c_is_finite = false;
    for (const auto& label : result.labels) {
        if (label.date == start + day && label.asset == "C" &&
            label.period == "1D") {
            one_day_c_is_missing = !std::isfinite(label.value);
        }
        if (label.date == start + day && label.asset == "C" &&
            label.period == "2D") {
            two_day_c_is_finite = std::isfinite(label.value);
        }
    }
    EXPECT_TRUE(one_day_c_is_missing);
    EXPECT_TRUE(two_day_c_is_finite);

    bool one_day_emits_small_cross_section_ic = false;
    bool two_day_keeps_larger_cross_section = false;
    for (const auto& row : result.ic) {
        one_day_emits_small_cross_section_ic =
            one_day_emits_small_cross_section_ic ||
            (row.period == "1D" && row.time == start + day);
        two_day_keeps_larger_cross_section =
            two_day_keeps_larger_cross_section ||
            (row.period == "2D" && row.time == start + day);
    }
    EXPECT_FALSE(one_day_emits_small_cross_section_ic);
    EXPECT_TRUE(two_day_keeps_larger_cross_section);

    bool one_day_sparse_middle_quantile = false;
    bool one_day_emits_filtered_singleton_quantiles = false;
    bool one_day_spread_ignores_quantile_count_filter = false;
    for (const auto& row : result.quantile_returns) {
        if (row.period == "1D" && row.time == start + day) {
            one_day_sparse_middle_quantile =
                one_day_sparse_middle_quantile || row.quantile == 2;
            one_day_emits_filtered_singleton_quantiles = true;
        }
    }
    for (const auto& row : result.spreads) {
        if (row.period == "1D" && row.time == start + day) {
            one_day_spread_ignores_quantile_count_filter = true;
            EXPECT_TRUE(std::isfinite(row.spread));
        }
    }
    EXPECT_FALSE(one_day_sparse_middle_quantile);
    EXPECT_FALSE(one_day_emits_filtered_singleton_quantiles);
    EXPECT_TRUE(one_day_spread_ignores_quantile_count_filter);

    spec.min_quantile_assets = 1;
    const auto unfiltered_quantiles =
        fincore::evaluate_factor_research_detailed(
            "sparse_horizon_signal", factors, prices, spec);
    bool middle_quantile_exists_elsewhere = false;
    bool middle_quantile_emitted_on_sparse_date = false;
    std::size_t middle_quantile_turnover_rows = 0U;
    for (const auto& row : unfiltered_quantiles.quantile_returns) {
        if (row.period == "1D" && row.quantile == 2 &&
            row.time == start) {
            middle_quantile_exists_elsewhere = true;
        }
        if (row.period == "1D" && row.quantile == 2 &&
            row.time == start + day) {
            middle_quantile_emitted_on_sparse_date = true;
        }
    }
    for (const auto& row : unfiltered_quantiles.turnover) {
        if (row.period == "1D" && row.quantile == 2) {
            ++middle_quantile_turnover_rows;
            EXPECT_EQ(row.time, start + 3 * day);
            EXPECT_DOUBLE_EQ(row.turnover, 0.0);
        }
    }
    EXPECT_TRUE(middle_quantile_exists_elsewhere);
    EXPECT_FALSE(middle_quantile_emitted_on_sparse_date);
    EXPECT_EQ(middle_quantile_turnover_rows, 1U);
}

TEST(FactorResearchDetailed,
     ReusesHorizonDateOrderingForTiedFactorRankIc) {
    const auto start = fincore::utc_timestamp(2024, 2, 19);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<std::string> assets{
        "A", "B", "C", "D", "E", "F", "G", "H", "I", "J"};
    const std::vector<double> factor_values{
        3.0, 1.0, 1.0, 4.0, 2.0, 5.0, nan, infinity, 6.0, -infinity};

    // Shuffle source rows so the rank/label samples must map back to the
    // canonical date/asset panel while retaining equal-factor average ranks.
    const std::vector<std::size_t> source_order{4U, 1U, 7U, 0U, 6U,
                                                8U, 3U, 9U, 2U, 5U};
    fincore::factor_series factors;
    for (const std::size_t asset : source_order) {
        factors.dates.push_back(start);
        factors.assets.push_back(assets[asset]);
        factors.values.push_back(factor_values[asset]);
    }

    const std::vector<double> one_day_returns{
        0.05, 0.04, 0.01, -0.02, 0.03, -0.01, 0.02, 0.06, 0.00, -0.03};
    const std::vector<double> two_day_returns{
        0.00, 0.10, -0.02, 0.03, 0.05, -0.01, 0.02, 0.06, 0.04, -0.03};
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start + day,
                                                       start + 2 * day});
    for (std::size_t asset = 0U; asset < assets.size(); ++asset) {
        const double base = 100.0 + 10.0 * static_cast<double>(asset);
        const double first_price =
            asset == 0U || asset == 3U
                ? nan
                : base * (1.0 + one_day_returns[asset]);
        const double second_price =
            asset == 1U || asset == 4U
                ? nan
                : (asset == 3U
                       ? infinity
                       : base * (1.0 + two_day_returns[asset]));
        prices.load_column<double>(
            assets[asset].c_str(), {base, first_price, second_price},
            hmdf::nan_policy::dont_pad_with_nans);
    }

    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start;
    spec.periods = {1, 2};
    spec.quantiles = 3;
    spec.min_assets = 2;
    spec.max_loss = 1.0;
    spec.label = "close_to_close";
    spec.preprocess = "none";
    const auto actual = fincore::evaluate_factor_research_detailed(
        "rank_reuse_signal", factors, prices, spec);

    const auto average_ranks = [nan](const std::vector<double>& values) {
        std::vector<std::pair<double, std::size_t>> order;
        order.reserve(values.size());
        for (std::size_t index = 0U; index < values.size(); ++index) {
            order.emplace_back(values[index], index);
        }
        std::stable_sort(order.begin(), order.end(),
                         [](const auto& left, const auto& right) {
                             return left.first < right.first;
                         });
        std::vector<double> ranks(values.size(), nan);
        for (std::size_t first = 0U; first < order.size();) {
            std::size_t last = first + 1U;
            while (last < order.size() &&
                   order[last].first == order[first].first) {
                ++last;
            }
            const double rank =
                (static_cast<double>(first + 1U) +
                 static_cast<double>(last)) /
                2.0;
            for (std::size_t position = first; position < last; ++position) {
                ranks[order[position].second] = rank;
            }
            first = last;
        }
        return ranks;
    };
    const auto scalar_pearson = [](const std::vector<double>& left,
                                   const std::vector<double>& right) {
        double left_mean = 0.0;
        double right_mean = 0.0;
        for (std::size_t index = 0U; index < left.size(); ++index) {
            left_mean += left[index];
            right_mean += right[index];
        }
        left_mean /= static_cast<double>(left.size());
        right_mean /= static_cast<double>(right.size());
        double covariance = 0.0;
        double left_variance = 0.0;
        double right_variance = 0.0;
        for (std::size_t index = 0U; index < left.size(); ++index) {
            const double x = left[index] - left_mean;
            const double y = right[index] - right_mean;
            covariance += x * y;
            left_variance += x * x;
            right_variance += y * y;
        }
        return covariance /
               std::sqrt(left_variance * right_variance);
    };

    ASSERT_EQ(actual.ic.size(), 2U);
    for (std::size_t horizon = 0U; horizon < spec.periods.size(); ++horizon) {
        const std::string period = std::to_string(spec.periods[horizon]) + "D";
        std::vector<double> expected_factors;
        std::vector<double> expected_returns;
        for (std::size_t asset = 0U; asset < assets.size(); ++asset) {
            const double factor = factor_values[asset];
            const double base = 100.0 + 10.0 * static_cast<double>(asset);
            const bool missing_price =
                horizon == 0U ? (asset == 0U || asset == 3U)
                              : (asset == 1U || asset == 4U);
            const double expected_return =
                horizon == 0U ? one_day_returns[asset]
                              : two_day_returns[asset];
            const double future_price =
                missing_price ? nan
                              : (horizon == 1U && asset == 3U
                                     ? infinity
                                     : base * (1.0 + expected_return));
            const double forward_return = future_price / base - 1.0;
            if (std::isfinite(factor) && std::isfinite(forward_return)) {
                expected_factors.push_back(factor);
                expected_returns.push_back(forward_return);
            }
        }
        ASSERT_EQ(expected_factors.size(), horizon == 0U ? 5U : 4U);
        ASSERT_EQ(expected_returns.size(), expected_factors.size());
        const auto factor_ranks = average_ranks(expected_factors);
        const auto return_ranks = average_ranks(expected_returns);
        const auto row = std::find_if(
            actual.ic.begin(), actual.ic.end(),
            [&period](const auto& value) { return value.period == period; });
        ASSERT_NE(row, actual.ic.end());
        EXPECT_EQ(row->time, start);
        EXPECT_EQ(row->n_assets, expected_factors.size());
        EXPECT_NEAR(row->ic,
                    scalar_pearson(expected_factors, expected_returns), 1e-12);
        EXPECT_NEAR(row->rank_ic,
                    scalar_pearson(factor_ranks, return_ranks), 1e-12);
    }

    const auto has_nan_label = [&actual](const std::string& asset,
                                         const std::string& period) {
        const auto found = std::find_if(
            actual.labels.begin(), actual.labels.end(),
            [&asset, &period](const auto& row) {
                return row.asset == asset && row.period == period;
            });
        return found != actual.labels.end() && !std::isfinite(found->value);
    };
    EXPECT_TRUE(has_nan_label("A", "1D"));
    EXPECT_TRUE(has_nan_label("D", "1D"));
    EXPECT_TRUE(has_nan_label("B", "2D"));
    EXPECT_TRUE(has_nan_label("E", "2D"));
    EXPECT_TRUE(has_nan_label("D", "2D"));
}

TEST(FactorResearchDetailed,
     PreservesDuplicateQcutBoundaryAndMaxLossDiagnostics) {
    const auto start = fincore::utc_timestamp(2024, 2, 26);
    const std::vector<std::string> assets{"A", "B", "C", "D"};
    fincore::factor_series factors;
    for (const auto& asset : assets) {
        factors.dates.push_back(start);
        factors.assets.push_back(asset);
        factors.values.push_back(1.0);
    }
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start + 1});
    for (const auto& asset : assets) {
        prices.load_column<double>(
            asset.c_str(), {100.0, 101.0},
            hmdf::nan_policy::dont_pad_with_nans);
    }

    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start;
    spec.periods = {1};
    spec.quantiles = 2;
    spec.min_assets = 2;
    spec.label = "close_to_close";
    spec.preprocess = "none";

    const auto find_diagnostic = [](const auto& result,
                                    const std::string& code) {
        return std::find_if(result.diagnostics.begin(),
                            result.diagnostics.end(),
                            [&code](const auto& row) {
                                return row.code == code;
                            });
    };
    spec.max_loss = 0.0;
    const auto strict = fincore::evaluate_factor_research_detailed(
        "duplicate_boundary_signal", factors, prices, spec);
    const auto boundary = find_diagnostic(strict, "preparation_failed");
    ASSERT_NE(boundary, strict.diagnostics.end());
    EXPECT_NE(boundary->message.find("Bin edges must be unique"),
              std::string::npos);

    spec.max_loss = 0.99;
    const auto bounded = fincore::evaluate_factor_research_detailed(
        "duplicate_boundary_signal", factors, prices, spec);
    EXPECT_NE(find_diagnostic(bounded, "max_loss_exceeded"),
              bounded.diagnostics.end());
}

TEST(FactorResearchDetailed, RejectsDuplicateStabilityLags) {
    const auto start = fincore::utc_timestamp(2024, 3, 4);
    fincore::factor_series factors;
    factors.dates = {start};
    factors.assets = {"A"};
    factors.values = {1.0};
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start + 1});
    prices.load_column<double>("A", {1.0, 2.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    fincore::research_spec spec;
    spec.targets = {"A"};
    spec.start = start;
    spec.end = start;
    spec.stability_lags = {1, 1};

    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "factor", factors, prices, spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed, RejectsMissingNextOpenTargetPriceColumn) {
    const auto start = fincore::utc_timestamp(2024, 3, 11);
    fincore::factor_series factors;
    factors.dates = {start};
    factors.assets = {"A"};
    factors.values = {1.0};
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start + 1,
                                                      start + 2});
    prices.load_column<double>("B", {1.0, 2.0, 3.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    fincore::research_spec spec;
    spec.targets = {"A"};
    spec.start = start;
    spec.end = start;
    spec.periods = {1};
    spec.label = "next_open_to_open";
    spec.price_basis = "open";

    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "factor", factors, prices, spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed,
     PreprocessContiguousDateRangesPreservesValuesAndCollapsedCount) {
    const auto start = fincore::utc_timestamp(2024, 3, 18);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();
    const std::vector<fincore::timestamp> dates{
        start, start + day, start + 2 * day};
    const std::vector<std::string> assets{"A", "B", "C", "D", "E"};
    const std::vector<std::vector<double>> source_values{
        {0.0, -0.0, -infinity, infinity, nan},
        {-infinity, infinity, nan, -infinity, infinity},
        {1.0, nan, 3.0, 5.0, nan},
    };

    fincore::factor_series factors;
    for (std::size_t date = dates.size(); date > 0U; --date) {
        const std::size_t date_index = date - 1U;
        for (std::size_t asset = assets.size(); asset > 0U; --asset) {
            const std::size_t asset_index = asset - 1U;
            factors.dates.push_back(dates[date_index]);
            factors.assets.push_back(assets[asset_index]);
            factors.values.push_back(source_values[date_index][asset_index]);
        }
    }

    fincore::dataframe prices;
    prices.load_index(
        std::vector<fincore::timestamp>{start, start + day,
                                        start + 2 * day, start + 3 * day});
    for (const auto& asset : assets) {
        prices.load_column<double>(
            asset.c_str(), {10.0, 11.0, 12.0, 13.0},
            hmdf::nan_policy::dont_pad_with_nans);
    }

    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start + 2 * day;
    spec.periods = {1};
    spec.quantiles = 2;
    spec.min_assets = 1;
    spec.min_quantile_assets = 1;
    spec.max_loss = 1.0;
    spec.stability_lags = {1};
    spec.label = "close_to_close";
    spec.price_basis = "close";

    const auto evaluate_with_preprocess = [&](const std::string& method) {
        auto method_spec = spec;
        method_spec.preprocess = method;
        return fincore::evaluate_factor_research_detailed(
            "signal", factors, prices, method_spec);
    };
    const auto none = evaluate_with_preprocess("none");
    const auto rank = evaluate_with_preprocess("cross_section_rank");
    const auto zscore = evaluate_with_preprocess("cross_section_zscore");

    const std::vector<std::vector<double>> expected_rank{
        {0.625, 0.625, 0.25, 1.0, nan},
        {0.375, 0.875, nan, 0.375, 0.875},
        {1.0 / 3.0, nan, 2.0 / 3.0, 1.0, nan},
    };
    const std::vector<std::vector<double>> expected_zscore{
        {nan, nan, nan, nan, nan},
        {nan, nan, nan, nan, nan},
        {-1.0, nan, 0.0, 1.0, nan},
    };
    const auto expect_value = [](const double actual, const double expected) {
        if (std::isnan(expected)) {
            EXPECT_TRUE(std::isnan(actual));
        } else if (std::isinf(expected)) {
            EXPECT_TRUE(std::isinf(actual));
            EXPECT_EQ(std::signbit(actual), std::signbit(expected));
        } else {
            EXPECT_DOUBLE_EQ(actual, expected);
            if (expected == 0.0) {
                EXPECT_EQ(std::signbit(actual), std::signbit(expected));
            }
        }
    };
    const auto expect_signals = [&](const auto& result,
                                    const std::vector<std::vector<double>>&
                                        expected_processed) {
        ASSERT_EQ(result.signals.size(), dates.size() * assets.size());
        for (std::size_t date = 0U; date < dates.size(); ++date) {
            for (std::size_t asset = 0U; asset < assets.size(); ++asset) {
                const std::size_t row = date * assets.size() + asset;
                const auto& signal = result.signals[row];
                EXPECT_EQ(signal.date, dates[date]);
                EXPECT_EQ(signal.asset, assets[asset]);
                expect_value(signal.value, source_values[date][asset]);
                expect_value(signal.processed_value,
                             expected_processed[date][asset]);
            }
        }
    };
    expect_signals(none, source_values);
    expect_signals(rank, expected_rank);
    expect_signals(zscore, expected_zscore);

    const auto collapsed_count = [](const auto& result) {
        return static_cast<std::size_t>(std::count_if(
            result.diagnostics.begin(), result.diagnostics.end(),
            [](const auto& row) {
                return row.code == "preprocess_collapsed_cross_section";
            }));
    };
    EXPECT_EQ(collapsed_count(none), 0);
    EXPECT_EQ(collapsed_count(rank), 0);
    ASSERT_EQ(collapsed_count(zscore), 1);
    const auto collapsed = std::find_if(
        zscore.diagnostics.begin(), zscore.diagnostics.end(),
        [](const auto& row) {
            return row.code == "preprocess_collapsed_cross_section";
        });
    ASSERT_NE(collapsed, zscore.diagnostics.end());
    ASSERT_TRUE(collapsed->factor.has_value());
    EXPECT_EQ(*collapsed->factor, "signal");
    EXPECT_EQ(collapsed->stage, "preprocess");
    EXPECT_EQ(collapsed->severity, "warning");
    EXPECT_EQ(collapsed->message,
              "1 cross-sections for factor 'signal' became fully undefined "
              "under preprocess='cross_section_zscore'");
}

TEST(FactorResearchDetailed,
     MissingPriceErrorKeepsPanelOrderAndRejectsNonDoubleColumn) {
    const auto start = fincore::utc_timestamp(2024, 3, 20);
    fincore::factor_series factors;
    factors.dates = {start, start, start};
    factors.assets = {"C", "B", "A"};
    factors.values = {3.0, 2.0, 1.0};

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start});
    prices.load_column<std::string>("A", {"not a price"},
                                   hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("C", {10.0},
                              hmdf::nan_policy::dont_pad_with_nans);

    fincore::research_spec spec;
    spec.targets = {"B", "A", "C"};
    spec.start = start;
    spec.end = start;
    spec.periods = {1};
    spec.preprocess = "cross_section_zscore";

    try {
        static_cast<void>(fincore::evaluate_factor_research_detailed(
            "signal", factors, prices, spec));
        FAIL() << "expected missing double price columns to be rejected";
    } catch (const fincore::factor_data_error& error) {
        EXPECT_STREQ(error.what(),
                     "prices are missing target assets: A,B");
    }
}

TEST(FactorResearchDetailed, MapsFilteredUnsortedRowsToOriginalValues) {
    const auto start = fincore::utc_timestamp(2024, 3, 25);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const double infinity = std::numeric_limits<double>::infinity();

    // Selected rows appear in reverse date order and mixed asset order. The
    // rows before the window and for non-target assets must not be emitted.
    fincore::factor_series factors;
    factors.dates = {start + 2 * day, start + day, start,
                     start + day, start + 2 * day, start + day, start};
    factors.assets = {"B", "B", "A", "C", "A", "A", "B"};
    factors.values = {infinity, -infinity, 42.0, 99.0, nan, infinity, 24.0};

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day});
    prices.load_column<double>("A", {10.0, 11.0, 12.0, 13.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("B", {20.0, 21.0, 22.0, 23.0},
                               hmdf::nan_policy::dont_pad_with_nans);

    fincore::research_spec spec;
    spec.targets = {"A", "B"};
    spec.start = start + day;
    spec.end = start + 2 * day;
    spec.periods = {1};
    spec.quantiles = 2;
    spec.min_assets = 1;
    spec.min_quantile_assets = 1;
    spec.max_loss = 1.0;
    spec.stability_lags = {1};
    spec.label = "next_open_to_open";
    spec.preprocess = "none";
    spec.price_basis = "open";
    spec.direction_overrides.emplace("signal", -1);

    const auto result = fincore::evaluate_factor_research_detailed(
        "signal", factors, prices, spec);

    ASSERT_EQ(result.signals.size(), 4U);
    EXPECT_EQ(result.signals[0].date, start + day);
    EXPECT_EQ(result.signals[0].asset, "A");
    EXPECT_EQ(result.signals[1].date, start + day);
    EXPECT_EQ(result.signals[1].asset, "B");
    EXPECT_EQ(result.signals[2].date, start + 2 * day);
    EXPECT_EQ(result.signals[2].asset, "A");
    EXPECT_EQ(result.signals[3].date, start + 2 * day);
    EXPECT_EQ(result.signals[3].asset, "B");
    for (const auto& signal : result.signals) {
        EXPECT_EQ(signal.direction, -1);
    }
    EXPECT_EQ(result.signals[0].value, -infinity);
    EXPECT_EQ(result.signals[0].processed_value, -infinity);
    EXPECT_EQ(result.signals[1].value, infinity);
    EXPECT_EQ(result.signals[1].processed_value, infinity);
    EXPECT_TRUE(std::isnan(result.signals[2].value));
    EXPECT_TRUE(std::isnan(result.signals[2].processed_value));
    EXPECT_EQ(result.signals[3].value, -infinity);
    EXPECT_EQ(result.signals[3].processed_value, -infinity);
    for (const auto& signal : result.signals) {
        EXPECT_EQ(signal.group, "default");
    }

    // The index fast path for sorted input and the sort fallback for shuffled
    // input must produce the same canonical rows. Include unselected assets,
    // out-of-window rows, NaN and infinities in both source layouts.
    fincore::factor_series ordered_factors;
    ordered_factors.dates = {start, start, start + day, start + day,
                             start + day, start + 2 * day,
                             start + 2 * day};
    ordered_factors.assets = {"A", "B", "A", "B", "C", "A", "B"};
    ordered_factors.values = {42.0, 24.0, infinity, -infinity,
                              99.0, nan, infinity};
    const auto ordered_result = fincore::evaluate_factor_research_detailed(
        "signal", ordered_factors, prices, spec);

    auto reversed_targets_spec = spec;
    reversed_targets_spec.targets = {"B", "A"};
    const auto reversed_targets_result =
        fincore::evaluate_factor_research_detailed(
            "signal", factors, prices, reversed_targets_spec);

    const auto expect_same_rows = [&result](const auto& actual) {
        ASSERT_EQ(actual.signals.size(), result.signals.size());
        for (std::size_t row = 0U; row < result.signals.size(); ++row) {
            const auto& expected_signal = result.signals[row];
            const auto& actual_signal = actual.signals[row];
            EXPECT_EQ(actual_signal.date, expected_signal.date);
            EXPECT_EQ(actual_signal.asset, expected_signal.asset);
            EXPECT_EQ(actual_signal.factor, expected_signal.factor);
            EXPECT_EQ(actual_signal.direction, expected_signal.direction);
            EXPECT_EQ(actual_signal.preprocess, expected_signal.preprocess);
            EXPECT_EQ(actual_signal.group, expected_signal.group);
            if (std::isnan(expected_signal.value)) {
                EXPECT_TRUE(std::isnan(actual_signal.value));
            } else {
                EXPECT_EQ(actual_signal.value, expected_signal.value);
            }
            if (std::isnan(expected_signal.processed_value)) {
                EXPECT_TRUE(std::isnan(actual_signal.processed_value));
            } else {
                EXPECT_EQ(actual_signal.processed_value,
                          expected_signal.processed_value);
            }
        }
        ASSERT_EQ(actual.labels.size(), result.labels.size());
        for (std::size_t row = 0U; row < result.labels.size(); ++row) {
            const auto& expected_label = result.labels[row];
            const auto& actual_label = actual.labels[row];
            EXPECT_EQ(actual_label.date, expected_label.date);
            EXPECT_EQ(actual_label.asset, expected_label.asset);
            EXPECT_EQ(actual_label.period, expected_label.period);
            if (std::isnan(expected_label.value)) {
                EXPECT_TRUE(std::isnan(actual_label.value));
            } else {
                EXPECT_EQ(actual_label.value, expected_label.value);
            }
        }
    };
    expect_same_rows(ordered_result);
    expect_same_rows(reversed_targets_result);

    auto custom_groups_spec = spec;
    custom_groups_spec.groups = {{"A", "group_a"}, {"B", "group_b"}};
    const auto custom_groups_result =
        fincore::evaluate_factor_research_detailed(
            "signal", factors, prices, custom_groups_spec);
    ASSERT_EQ(custom_groups_result.signals.size(), 4U);
    for (const auto& signal : custom_groups_result.signals) {
        EXPECT_EQ(signal.group,
                  signal.asset == "A" ? "group_a" : "group_b");
    }

    auto missing_group_spec = spec;
    missing_group_spec.groups = {{"B", "group_b"}};
    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "signal", factors, prices, missing_group_spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed, RejectsDuplicateKeysOutsideSelectionWindow) {
    const auto start = fincore::utc_timestamp(2024, 5, 6);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    fincore::factor_series factors;
    factors.dates = {start - day, start - day, start};
    factors.assets = {"A", "A", "A"};
    factors.values = {1.0, 2.0, 3.0};

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start - day, start, start + day, start + 2 * day});
    prices.load_column<double>("A", {10.0, 11.0, 12.0, 13.0},
                               hmdf::nan_policy::dont_pad_with_nans);

    fincore::research_spec spec;
    spec.targets = {"A"};
    spec.start = start;
    spec.end = start;
    spec.periods = {1};
    spec.label = "next_open_to_open";
    spec.price_basis = "open";

    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "duplicate_signal", factors, prices, spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed, StabilityRanksIncludeInfinities) {
    const auto start = fincore::utc_timestamp(2024, 4, 1);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const std::vector<std::string> assets{"A", "B", "C"};
    const double infinity = std::numeric_limits<double>::infinity();
    fincore::factor_series factors;
    const std::vector<std::vector<double>> factor_rows{
        {-infinity, 0.0, 1.0},
        {0.0, 1.0, infinity},
        {1.0, 2.0, 3.0},
    };
    for (std::size_t date = 0; date < factor_rows.size(); ++date) {
        for (std::size_t asset = 0; asset < assets.size(); ++asset) {
            factors.dates.push_back(start +
                                    static_cast<fincore::timestamp>(date) *
                                        day);
            factors.assets.push_back(assets[asset]);
            factors.values.push_back(factor_rows[date][asset]);
        }
    }
    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day});
    const std::vector<std::vector<double>> price_rows{
        {10.0, 11.0, 12.0, 13.0},
        {20.0, 21.0, 22.0, 23.0},
        {30.0, 33.0, 36.0, 39.0},
    };
    for (std::size_t asset = 0; asset < assets.size(); ++asset) {
        prices.load_column<double>(
            assets[asset].c_str(), price_rows[asset],
            hmdf::nan_policy::dont_pad_with_nans);
    }
    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start + 2 * day;
    spec.periods = {1};
    spec.quantiles = 2;
    spec.min_assets = 2;
    spec.min_quantile_assets = 1;
    spec.max_loss = 1.0;
    spec.stability_lags = {1};
    spec.label = "next_open_to_open";
    spec.preprocess = "none";

    const auto result = fincore::evaluate_factor_research_detailed(
        "infinite_signal", factors, prices, spec);
    ASSERT_EQ(result.signals.size(), 9U);
    EXPECT_TRUE(std::isinf(result.signals[0].value));
    EXPECT_TRUE(std::isinf(result.signals[5].value));
    ASSERT_EQ(result.stability.size(), 1U);
    EXPECT_EQ(result.stability[0].n_dates, 2U);
    EXPECT_NEAR(result.stability[0].mean_rank_autocorrelation, 1.0, 1e-12);
}

TEST(FactorResearchDetailed,
     StabilityMatchesPreparedReferenceForSparsePanelAndMultipleLags) {
    const auto start = fincore::utc_timestamp(2024, 4, 8);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::vector<std::string> assets{"A", "B", "C"};

    fincore::factor_series factors;
    const auto add_factor = [&factors, start, day](const int date,
                                                   const std::string& asset,
                                                   const double value) {
        factors.dates.push_back(
            start + static_cast<fincore::timestamp>(date) * day);
        factors.assets.push_back(asset);
        factors.values.push_back(value);
    };
    add_factor(0, "A", 0.0);
    add_factor(0, "B", 1.0);
    add_factor(0, "C", 2.0);
    add_factor(1, "A", 0.0);
    add_factor(1, "C", 1.0);
    add_factor(2, "A", nan);
    add_factor(2, "B", nan);
    add_factor(2, "C", nan);
    add_factor(3, "B", 0.0);
    add_factor(3, "C", 1.0);
    add_factor(4, "A", 0.0);
    add_factor(4, "B", 2.0);
    add_factor(4, "C", 1.0);

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day,
        start + 4 * day, start + 5 * day});
    prices.load_column<double>("A", {10.0, 11.0, 12.0, 13.0, 14.0, 15.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("B", {20.0, 21.0, 22.0, 23.0, 24.0, 25.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("C", {30.0, 31.0, 32.0, 33.0, 34.0, 35.0},
                               hmdf::nan_policy::dont_pad_with_nans);

    fincore::research_spec spec;
    spec.targets = assets;
    spec.start = start;
    spec.end = start + 4 * day;
    spec.periods = {1};
    spec.quantiles = 2;
    spec.min_assets = 2;
    spec.min_quantile_assets = 1;
    spec.max_loss = 1.0;
    spec.stability_lags = {1, 2};
    spec.label = "close_to_close";
    spec.preprocess = "none";

    const auto actual = fincore::evaluate_factor_research_detailed(
        "sparse_stability_signal", factors, prices, spec);

    // The legacy prepared-factor helper remains the exact Pearson reference.
    // These are the whole-panel average ranks, including the empty date as
    // three NaN rows, before that helper's idempotent second rank pass.
    fincore::prepared_factor_data reference_panel;
    const auto add_reference = [&reference_panel, start, day](
                                  const int date, const std::string& asset,
                                  const double rank) {
        fincore::factor_row row;
        row.date = start + static_cast<fincore::timestamp>(date) * day;
        row.asset = asset;
        row.factor = rank;
        reference_panel.rows.push_back(row);
    };
    add_reference(0, "A", 1.0);
    add_reference(0, "B", 2.0);
    add_reference(0, "C", 3.0);
    add_reference(1, "A", 1.0);
    add_reference(1, "C", 2.0);
    add_reference(2, "A", nan);
    add_reference(2, "B", nan);
    add_reference(2, "C", nan);
    add_reference(3, "B", 1.0);
    add_reference(3, "C", 2.0);
    add_reference(4, "A", 1.0);
    add_reference(4, "B", 3.0);
    add_reference(4, "C", 2.0);
    const auto reference = fincore::detail::factor_rank_autocorrelation_many(
        reference_panel, spec.stability_lags);

    ASSERT_EQ(actual.stability.size(), spec.stability_lags.size());
    ASSERT_EQ(reference.size(), actual.stability.size());
    for (std::size_t lag = 0U; lag < actual.stability.size(); ++lag) {
        SCOPED_TRACE(std::string{"lag="} +
                     std::to_string(spec.stability_lags[lag]));
        std::size_t count = 0U;
        double sum = 0.0;
        for (const double value : reference[lag].values) {
            if (std::isfinite(value)) {
                ++count;
                sum += value;
            }
        }
        EXPECT_EQ(actual.stability[lag].lag, spec.stability_lags[lag]);
        EXPECT_EQ(actual.stability[lag].n_dates, count);
        if (count == 0U) {
            EXPECT_TRUE(std::isnan(
                actual.stability[lag].mean_rank_autocorrelation));
            EXPECT_TRUE(
                std::isnan(actual.stability[lag].std_rank_autocorrelation));
            continue;
        }
        const double mean = sum / static_cast<double>(count);
        EXPECT_NEAR(actual.stability[lag].mean_rank_autocorrelation, mean,
                    1e-12);
        if (count < 2U) {
            EXPECT_TRUE(
                std::isnan(actual.stability[lag].std_rank_autocorrelation));
            continue;
        }
        double squared = 0.0;
        for (const double value : reference[lag].values) {
            if (std::isfinite(value)) {
                const double delta = value - mean;
                squared += delta * delta;
            }
        }
        const double deviation =
            std::sqrt(squared / static_cast<double>(count - 1U));
        EXPECT_NEAR(actual.stability[lag].std_rank_autocorrelation,
                    deviation, 1e-12);
    }
    EXPECT_EQ(actual.stability[0].n_dates, 2U);
    EXPECT_NEAR(actual.stability[0].mean_rank_autocorrelation, 0.0, 1e-12);
    EXPECT_NEAR(actual.stability[0].std_rank_autocorrelation,
                std::sqrt(2.0), 1e-12);
    EXPECT_EQ(actual.stability[1].n_dates, 0U);
}

TEST(FactorResearchDetailed, RejectsDuplicateFactorKeys) {
    const auto start = fincore::utc_timestamp(2024, 5, 6);
    fincore::factor_series factors;
    factors.dates = {start, start};
    factors.assets = {"A", "A"};
    factors.values = {1.0, 2.0};

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start + 1,
                                                      start + 2});
    prices.load_column<double>("A", {10.0, 11.0, 12.0},
                               hmdf::nan_policy::dont_pad_with_nans);

    fincore::research_spec spec;
    spec.targets = {"A"};
    spec.start = start;
    spec.end = start;
    spec.periods = {1};
    spec.label = "next_open_to_open";
    spec.price_basis = "open";

    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "duplicate_key_signal", factors, prices, spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed, RejectsDuplicatePriceGridTimestamps) {
    const auto start = fincore::utc_timestamp(2024, 5, 13);
    fincore::factor_series factors;
    factors.dates = {start};
    factors.assets = {"A"};
    factors.values = {1.0};

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{start, start,
                                                      start + 1});
    prices.load_column<double>("A", {10.0, 11.0, 12.0},
                               hmdf::nan_policy::dont_pad_with_nans);

    fincore::research_spec spec;
    spec.targets = {"A"};
    spec.start = start;
    spec.end = start;
    spec.periods = {1};
    spec.label = "next_open_to_open";
    spec.price_basis = "open";

    EXPECT_THROW(static_cast<void>(fincore::evaluate_factor_research_detailed(
                     "duplicate_grid_signal", factors, prices, spec)),
                 fincore::factor_data_error);
}

TEST(FactorResearchDetailed,
     BuildsNextOpenLabelsDirectlyForSparseUnsortedPanelRows) {
    const auto start = fincore::utc_timestamp(2024, 5, 20);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const double infinity = std::numeric_limits<double>::infinity();
    const double nan = std::numeric_limits<double>::quiet_NaN();

    fincore::dataframe prices;
    prices.load_index(std::vector<fincore::timestamp>{
        start, start + 2 * day, start + 3 * day, start + 4 * day,
        start + 5 * day});
    prices.load_column<double>("A", {100.0, 120.0, infinity, 130.0, 140.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("B", {100.0, 0.0, 120.0, 130.0, nan},
                               hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("NOT_IN_PANEL", {1.0, 2.0, 3.0, 4.0, 5.0},
                               hmdf::nan_policy::dont_pad_with_nans);

    // Deliberately unsorted, sparse rows exercise date/asset lookup without
    // relying on the production evaluator's sorted panel precondition.
    fincore::factor_series panel;
    panel.dates = {start + 2 * day, start + day, start, start + 4 * day,
                   start, start + 2 * day};
    panel.assets = {"A", "B", "B", "A", "A", "B"};
    panel.values = {1.0, 2.0, 3.0, 4.0, 5.0, 6.0};

    fincore::research_spec spec;
    spec.start = start;
    spec.end = start + 4 * day;
    spec.periods = {1, 2};
    spec.label = "next_open_to_open";

    const auto labels =
        fincore::research_detailed_detail::build_detailed_label_input(
            panel, prices, spec);

    ASSERT_EQ(labels.horizons.size(), 2U);
    EXPECT_EQ(labels.horizons[0].period, "1D");
    EXPECT_EQ(labels.horizons[1].period, "2D");
    ASSERT_EQ(labels.horizons[0].label_values.size(), panel.dates.size());
    ASSERT_EQ(labels.horizons[1].label_values.size(), panel.dates.size());

    EXPECT_DOUBLE_EQ(labels.horizons[0].label_values[0], -1.0);
    EXPECT_TRUE(std::isnan(labels.horizons[0].label_values[1]));
    EXPECT_TRUE(std::isnan(labels.horizons[0].label_values[2]));
    EXPECT_TRUE(std::isnan(labels.horizons[0].label_values[3]));
    EXPECT_TRUE(std::isnan(labels.horizons[0].label_values[4]));
    EXPECT_DOUBLE_EQ(labels.horizons[0].label_values[5],
                     130.0 / 120.0 - 1.0);

    EXPECT_DOUBLE_EQ(labels.horizons[1].label_values[0], -1.0);
    EXPECT_TRUE(std::isnan(labels.horizons[1].label_values[1]));
    EXPECT_TRUE(std::isnan(labels.horizons[1].label_values[2]));
    EXPECT_TRUE(std::isnan(labels.horizons[1].label_values[3]));
    EXPECT_DOUBLE_EQ(labels.horizons[1].label_values[4],
                     130.0 / 120.0 - 1.0);
    EXPECT_TRUE(std::isnan(labels.horizons[1].label_values[5]));
}

}  // namespace
