#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/research_detailed.hpp>
#include <fincore/factor/serialization.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
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

}  // namespace
