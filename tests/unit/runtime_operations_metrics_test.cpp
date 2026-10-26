#include <fincore/calendar.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/stats.hpp>
#include <fincore/runtime/operations_metrics.hpp>
#include <fincore/runtime/runtime.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstdint>
#include <functional>
#include <set>
#include <string>
#include <variant>
#include <vector>

namespace {

constexpr std::int64_t day_nanoseconds = 86'400'000'000'000LL;

std::vector<fincore::timestamp> daily_index(const std::size_t count) {
    const auto start = fincore::utc_timestamp(2020, 1, 1);
    std::vector<fincore::timestamp> index;
    index.reserve(count);
    for (std::size_t position = 0; position < count; ++position) {
        index.push_back(start + static_cast<fincore::timestamp>(position) *
                                    day_nanoseconds);
    }
    return index;
}

fincore::dataframe returns_frame() {
    constexpr std::size_t count = 600U;
    std::vector<double> values;
    values.reserve(count);
    for (std::size_t position = 0; position < count; ++position) {
        double value = 0.0008 +
                       (static_cast<double>(position % 13U) - 6.0) * 0.0012;
        if (position % 5U == 0U) {
            value -= 0.004;
        }
        values.push_back(value);
    }
    return fincore::make_series(daily_index(count), "returns",
                                std::move(values));
}

fincore::dataframe benchmark_frame() {
    constexpr std::size_t count = 600U;
    std::vector<double> values;
    values.reserve(count);
    for (std::size_t position = 0; position < count; ++position) {
        values.push_back(
            0.0004 +
            (static_cast<double>(position % 11U) - 5.0) * 0.0010);
    }
    return fincore::make_series(daily_index(count), "benchmark",
                                std::move(values));
}

fincore::dataframe prices_frame() {
    constexpr std::size_t count = 30U;
    std::vector<double> values;
    values.reserve(count);
    for (std::size_t position = 0; position < count; ++position) {
        values.push_back(100.0 + static_cast<double>(position) * 0.25);
    }
    return fincore::make_series(daily_index(count), "price",
                                std::move(values));
}

fincore::dataframe peer_frame() {
    constexpr std::size_t count = 600U;
    std::vector<double> first;
    std::vector<double> second;
    first.reserve(count);
    second.reserve(count);
    for (std::size_t position = 0; position < count; ++position) {
        first.push_back(
            0.0006 +
            (static_cast<double>(position % 9U) - 4.0) * 0.0011);
        second.push_back(
            0.0002 +
            (static_cast<double>(position % 7U) - 3.0) * 0.0009);
    }
    fincore::dataframe frame;
    const auto index = daily_index(count);
    frame.load_index(index.begin(), index.end());
    frame.load_column<double>("peer_a", std::move(first),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("peer_b", std::move(second),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe valuations_frame() {
    const std::vector<fincore::timestamp> index{
        fincore::utc_timestamp(2020, 1, 1),
        fincore::utc_timestamp(2020, 1, 2),
        fincore::utc_timestamp(2020, 1, 3),
        fincore::utc_timestamp(2020, 1, 4)};
    fincore::dataframe frame;
    frame.load_index(index.begin(), index.end());
    frame.load_column<double>("value", {100.0, 110.0, 105.0, 120.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe cashflow_frame() {
    const std::vector<fincore::timestamp> index{
        fincore::utc_timestamp(2020, 1, 1),
        fincore::utc_timestamp(2020, 6, 1),
        fincore::utc_timestamp(2021, 1, 1)};
    fincore::dataframe frame;
    frame.load_index(index.begin(), index.end());
    frame.load_column<double>("cashflow", {-100.0, 50.0, 70.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

const fincore::dataframe& frame_input(const fincore::runtime_inputs& inputs) {
    return std::get<fincore::dataframe>(inputs.at("returns").data);
}

fincore::operation_spec known_spec(
    const std::string& operation_id, const std::string& kernel_symbol,
    std::function<fincore::runtime_value(const fincore::runtime_inputs&)>
        callable) {
    const auto domain = operation_id.substr(0U, operation_id.find('.'));
    return {operation_id, "fincore." + operation_id, domain,
            std::move(callable), true, "none", std::nullopt, std::nullopt,
            {{"owner", domain},
             {"kernel_symbol", kernel_symbol},
             {"source_operation_id", operation_id},
             {"input_schema",
              "{\"required\":{\"returns\":\"dataframe\"},\"optional\":{}}"}}};
}

// The native table holds 99 operations and this helper appends two more declared
// locally, so the 101-operation digest below covers the extended catalog rather
// than the shipped table on its own.
fincore::operation_catalog full_catalog() {
    auto operations = fincore::metrics_performance_operations();
    operations.push_back(known_spec(
        "metrics.ratios.sharpe_ratio", "sharpe_ratio",
        [](const fincore::runtime_inputs& inputs) {
            return fincore::runtime_value(
                fincore::sharpe_ratio(frame_input(inputs)));
        }));
    operations.push_back(known_spec(
        "metrics.stats.kurtosis", "kurtosis",
        [](const fincore::runtime_inputs& inputs) {
            return fincore::runtime_value(
                fincore::kurtosis(frame_input(inputs)));
        }));
    return fincore::operation_catalog(std::move(operations));
}

fincore::runtime_inputs binary_inputs() {
    fincore::runtime_inputs inputs;
    inputs.emplace("returns", fincore::runtime_value(returns_frame()));
    inputs.emplace("benchmark", fincore::runtime_value(benchmark_frame()));
    return inputs;
}

TEST(OperationsMetrics, DeclaresUniqueNativeOperations) {
    const auto operations = fincore::metrics_performance_operations();
    EXPECT_EQ(operations.size(), 99U);

    std::set<std::string> identifiers;
    for (const auto& operation : operations) {
        EXPECT_TRUE(identifiers.insert(operation.operation_id).second)
            << operation.operation_id;
        EXPECT_TRUE(operation.callable);
        EXPECT_TRUE(operation.deterministic);
        EXPECT_EQ(operation.rng_policy, "none");
        EXPECT_EQ(operation.domain,
                  operation.operation_id.substr(
                      0U, operation.operation_id.find('.')));
        // One operation keeps its historical capability id under the stats
        // module, so the exception is named once instead of relaxing the
        // prefix rule for every operation.
        EXPECT_EQ(operation.capability_id,
                  operation.operation_id ==
                          "metrics.ratios.common_sense_ratio"
                      ? "fincore.metrics.stats.common_sense_ratio"
                      : "fincore." + operation.operation_id);
        EXPECT_FALSE(operation.provenance.empty());
        EXPECT_EQ(operation.provenance.count("kernel_symbol"), 1U);
        EXPECT_EQ(operation.provenance.count("source_operation_id"), 1U);
        EXPECT_EQ(operation.provenance.at("source_operation_id"),
                  operation.operation_id);
        const auto schema = operation.provenance.find("input_schema");
        ASSERT_NE(schema, operation.provenance.end());
        EXPECT_FALSE(schema->second.empty());
        EXPECT_EQ(schema->second.front(), '{');
        EXPECT_NE(schema->second.find("\"required\""), std::string::npos);
        EXPECT_NE(schema->second.find("\"optional\""), std::string::npos);
    }
    EXPECT_EQ(identifiers.count(
                  "metrics.drawdown.get_max_drawdown_underwater"),
              1U);
}

TEST(OperationsMetrics, CatalogDigestIsStable) {
    const auto first = full_catalog();
    const auto second = full_catalog();

    EXPECT_EQ(first.digest(), second.digest());
    EXPECT_EQ(first.operation_ids(), second.operation_ids());
    EXPECT_EQ(first.operations().size(), 101U);

    std::set<std::string> identifiers;
    for (const auto& operation : first.operations()) {
        EXPECT_TRUE(identifiers.insert(operation.operation_id).second)
            << operation.operation_id;
    }
}

TEST(OperationsMetrics, DispatchesRepresentativeKernelFamilies) {    const auto catalog = full_catalog();

    const auto run = [&catalog](const std::string& operation_id,
                                const fincore::runtime_inputs& inputs) {
        return fincore::run(operation_id, inputs, {}, catalog);
    };

    const auto binary = binary_inputs();
    const auto alpha_beta = run("metrics.alpha_beta.alpha_beta", binary);
    ASSERT_TRUE(std::holds_alternative<fincore::dataframe>(
        alpha_beta.value.data));
    const auto& alpha_beta_frame =
        std::get<fincore::dataframe>(alpha_beta.value.data);
    EXPECT_EQ(alpha_beta_frame.get_index().size(), 1U);
    EXPECT_EQ(alpha_beta_frame.get_column<double>("alpha").size(), 1U);
    EXPECT_EQ(alpha_beta_frame.get_column<double>("beta").size(), 1U);

    auto peer_inputs = binary;
    peer_inputs.emplace("peer_returns",
                        fincore::runtime_value(peer_frame()));
    const auto percentile =
        run("metrics.alpha_beta.alpha_percentile_rank", peer_inputs);
    ASSERT_TRUE(
        std::holds_alternative<double>(percentile.value.data));
    EXPECT_TRUE(std::isfinite(std::get<double>(percentile.value.data)));

    const auto consecutive = run(
        "metrics.consecutive.max_consecutive_up_days", binary);
    ASSERT_TRUE(
        std::holds_alternative<double>(consecutive.value.data));

    const auto drawdown =
        run("metrics.drawdown.get_max_drawdown", binary);
    ASSERT_TRUE(
        std::holds_alternative<fincore::dataframe>(drawdown.value.data));
    EXPECT_EQ(std::get<fincore::dataframe>(drawdown.value.data)
                  .get_column<std::int64_t>("peak")
                  .size(),
              1U);

    const auto drawdown_table =
        run("metrics.drawdown.gen_drawdown_table", binary);
    ASSERT_TRUE(std::holds_alternative<fincore::dataframe>(
        drawdown_table.value.data));

    const auto treynor = run("metrics.ratios.treynor_ratio", binary);
    ASSERT_TRUE(std::holds_alternative<double>(treynor.value.data));
    EXPECT_TRUE(std::isfinite(std::get<double>(treynor.value.data)));

    auto price_inputs = binary;
    price_inputs.emplace("prices",
                         fincore::runtime_value(prices_frame()));
    const auto simple = run("metrics.returns.simple_returns", price_inputs);
    ASSERT_TRUE(
        std::holds_alternative<fincore::dataframe>(simple.value.data));
    EXPECT_FALSE(
        std::get<fincore::dataframe>(simple.value.data).get_index().empty());

    const auto aggregated =
        run("metrics.returns.aggregate_returns", binary);
    ASSERT_TRUE(std::holds_alternative<fincore::dataframe>(
        aggregated.value.data));
    EXPECT_FALSE(std::get<fincore::dataframe>(aggregated.value.data)
                     .get_index()
                     .empty());

    const auto value_at_risk = run("metrics.risk.value_at_risk", binary);
    ASSERT_TRUE(
        std::holds_alternative<double>(value_at_risk.value.data));

    auto rolling_inputs = binary;
    rolling_inputs.emplace("window",
                          fincore::runtime_value(std::int64_t{3}));
    const auto rolling =
        run("metrics.rolling.roll_alpha", rolling_inputs);
    ASSERT_TRUE(
        std::holds_alternative<fincore::dataframe>(rolling.value.data));
    EXPECT_FALSE(std::get<fincore::dataframe>(rolling.value.data)
                     .get_index()
                     .empty());

    const auto correlation = run(
        "metrics.stats.stock_market_correlation", binary);
    ASSERT_TRUE(
        std::holds_alternative<double>(correlation.value.data));
    EXPECT_TRUE(std::isfinite(std::get<double>(correlation.value.data)));

    const auto timing =
        run("metrics.timing.treynor_mazuy_timing", binary);
    ASSERT_TRUE(std::holds_alternative<double>(timing.value.data));

    const auto yearly =
        run("metrics.yearly.annual_return_by_year", binary);
    ASSERT_TRUE(
        std::holds_alternative<fincore::dataframe>(yearly.value.data));
    EXPECT_FALSE(
        std::get<fincore::dataframe>(yearly.value.data).get_index().empty());

    auto valuation_inputs = binary;
    valuation_inputs.emplace(
        "valuations", fincore::runtime_value(valuations_frame()));
    const auto adjusted_twr = run(
        "performance.cashflows.cashflow_adjusted_twr", valuation_inputs);
    ASSERT_TRUE(
        std::holds_alternative<double>(adjusted_twr.value.data));
    EXPECT_TRUE(std::isfinite(std::get<double>(adjusted_twr.value.data)));

    const auto disclosure = run(
        "performance.disclosures.render_disclosure", {});
    ASSERT_TRUE(
        std::holds_alternative<std::string>(disclosure.value.data));
    EXPECT_NE(std::get<std::string>(disclosure.value.data)
                  .find("Convention: TWR"),
              std::string::npos);

    const auto interval = run(
        "performance.inference.sharpe_confidence_interval", binary);
    ASSERT_TRUE(std::holds_alternative<fincore::dataframe>(
        interval.value.data));
    EXPECT_EQ(std::get<fincore::dataframe>(interval.value.data)
                  .get_column<double>("lower")
                  .size(),
              1U);

    const auto twr = run("performance.returns.twr", binary);
    ASSERT_TRUE(std::holds_alternative<double>(twr.value.data));
    EXPECT_TRUE(std::isfinite(std::get<double>(twr.value.data)));

    fincore::runtime_inputs cashflow_inputs;
    cashflow_inputs.emplace("cashflows",
                            fincore::runtime_value(cashflow_frame()));
    const auto mwr = run("performance.returns.mwr", cashflow_inputs);
    ASSERT_TRUE(std::holds_alternative<double>(mwr.value.data));

    const auto xirr = run("performance.returns.xirr", cashflow_inputs);
    ASSERT_TRUE(std::holds_alternative<double>(xirr.value.data));
}

TEST(OperationsMetrics, DispatchesUnderwaterDrawdown) {
    const auto specs = fincore::metrics_performance_operations();
    const auto found = std::find_if(
        specs.begin(), specs.end(), [](const fincore::operation_spec& spec) {
            return spec.operation_id ==
                   "metrics.drawdown.get_max_drawdown_underwater";
        });
    ASSERT_NE(found, specs.end());
    const auto start = fincore::utc_timestamp(2020, 1, 2);
    const auto underwater = fincore::make_series(
        {start, start + day_nanoseconds, start + 2 * day_nanoseconds,
         start + 3 * day_nanoseconds},
        "drawdown", {0.0, -0.1, -0.2, 0.0});
    const auto result =
        found->callable({{"underwater", fincore::runtime_value(underwater)}});
    const auto* frame = std::get_if<fincore::dataframe>(&result.data);
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->get_index().size(), 1U);
    EXPECT_EQ(frame->get_column<std::int64_t>("peak")[0], start);
    EXPECT_EQ(frame->get_column<std::int64_t>("valley")[0],
              start + 2 * day_nanoseconds);
    EXPECT_EQ(frame->get_column<std::int64_t>("recovery")[0],
              start + 3 * day_nanoseconds);
}

}  // namespace
