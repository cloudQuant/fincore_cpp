#include <fincore/calendar.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/rolling.hpp>
#include <fincore/metrics/stats.hpp>
#include <fincore/risk/backtesting.hpp>
#include <fincore/runtime/runtime.hpp>

#include <gtest/gtest.h>

#include <memory>
#include <string>
#include <vector>

namespace {

fincore::dataframe returns_fixture() {
    return fincore::make_series({1, 2, 3}, "returns",
                                {0.01, 0.02, -0.01});
}

fincore::dataframe benchmark_fixture() {
    return fincore::make_series({1, 2, 3}, "benchmark",
                                {0.005, 0.01, -0.005});
}

fincore::operation_catalog demo_catalog(std::shared_ptr<int> counter) {
    std::vector<fincore::operation_spec> operations;
    operations.push_back(
        {"demo.double", "demo.double", "demo",
         [counter](const fincore::runtime_inputs& inputs) {
             ++*counter;
             const auto found = inputs.find("value");
             const auto* value =
                 found == inputs.end()
                     ? nullptr
                     : std::get_if<double>(&found->second.data);
             if (value == nullptr) {
                 throw fincore::input_error{"value must be a number"};
             }
             return fincore::runtime_value(*value * 2.0);
         }});
    operations.push_back(
        {"demo.text", "demo.text", "demo",
         [](const fincore::runtime_inputs&) {
             return fincore::runtime_value(std::string("ok"));
         }});
    return fincore::operation_catalog(std::move(operations));
}

TEST(Runtime, RunsPlansAndBatchesInOrder) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    const auto single = fincore::run(
        "demo.double", {{"value", fincore::runtime_value(2.5)}},
        {{"mode", fincore::runtime_value(std::string("test"))}}, catalog);
    const auto* value = std::get_if<double>(&single.value.data);
    ASSERT_NE(value, nullptr);
    EXPECT_DOUBLE_EQ(*value, 5.0);
    EXPECT_EQ(single.metadata.at("operation_id"), "demo.double");
    EXPECT_EQ(single.metadata.at("config_recorded"), "true");
    EXPECT_EQ(single.metadata.at("cache"), "disabled");
    EXPECT_EQ(single.metadata.at("input_digest").size(), 64U);
    EXPECT_EQ(single.metadata.at("catalog_digest"), catalog.digest());

    const std::vector<fincore::operation_request> requests{
        {"demo.double", {{"value", fincore::runtime_value(1.0)}}},
        {"demo.text", {}}};
    const auto plan = fincore::plan(requests, catalog);
    EXPECT_EQ(plan.catalog_digest, catalog.digest());
    const auto results = fincore::batch(plan, catalog);
    ASSERT_EQ(results.size(), 2U);
    const auto* first = std::get_if<double>(&results[0].value.data);
    ASSERT_NE(first, nullptr);
    EXPECT_DOUBLE_EQ(*first, 2.0);
    const auto* second = std::get_if<std::string>(&results[1].value.data);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(*second, "ok");
    EXPECT_EQ(*counter, 2);
}

TEST(Runtime, ValidatesCatalogAndResolution) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    EXPECT_THROW(static_cast<void>(catalog.resolve("missing")),
                 fincore::operation_resolution_error);
    EXPECT_THROW(
        static_cast<void>(fincore::operation_catalog(
            {{"a", "a", "demo", [](const fincore::runtime_inputs&) {
                  return fincore::runtime_value(1.0);
              }},
             {"a", "b", "demo", [](const fincore::runtime_inputs&) {
                  return fincore::runtime_value(1.0);
              }}})),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::operation_catalog(
            {{"", "a", "demo", [](const fincore::runtime_inputs&) {
                  return fincore::runtime_value(1.0);
              }}})),
        fincore::input_error);

    std::make_shared<int>(0);
    const auto other = fincore::operation_catalog(
        {{"other.op", "other.op", "demo",
          [](const fincore::runtime_inputs&) {
              return fincore::runtime_value(1.0);
          }}});
    const std::vector<fincore::operation_request> requests{
        {"demo.double", {{"value", fincore::runtime_value(1.0)}}}};
    const auto execution_plan = fincore::plan(requests, catalog);
    EXPECT_THROW(static_cast<void>(fincore::batch(execution_plan, other)),
                 fincore::input_error);
}

TEST(Runtime, SessionCachesAndClears) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    fincore::analysis_session session(catalog);
    const fincore::runtime_inputs inputs{
        {"value", fincore::runtime_value(3.0)}};
    const auto first = session.run("demo.double", inputs);
    EXPECT_EQ(first.metadata.at("cache"), "miss");
    EXPECT_EQ(*counter, 1);
    const auto second = session.run("demo.double", inputs);
    EXPECT_EQ(second.metadata.at("cache"), "hit");
    EXPECT_EQ(*counter, 1);
    session.clear();
    const auto third = session.run("demo.double", inputs);
    EXPECT_EQ(third.metadata.at("cache"), "miss");
    EXPECT_EQ(*counter, 2);
}

TEST(Runtime, SnapshotsAndValueDigestsAreStable) {
    const auto frame = returns_fixture();
    const fincore::analysis_snapshot first({{"returns", fincore::runtime_value(frame)}});
    const fincore::analysis_snapshot second({{"returns", fincore::runtime_value(frame)}});
    EXPECT_EQ(first.digest(), second.digest());
    EXPECT_EQ(first.digest().size(), 64U);
    const auto changed = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, 0.03});
    EXPECT_NE(fincore::runtime_value_digest(fincore::runtime_value(frame)),
              fincore::runtime_value_digest(fincore::runtime_value(changed)));
}

TEST(Runtime, BuiltinCatalogRunsAcceptedOperations) {
    const auto catalog = fincore::builtin_catalog();
    ASSERT_EQ(catalog.operations().size(), 44U);
    EXPECT_EQ(catalog.operation_ids().size(), 44U);
    const auto frame = returns_fixture();
    const auto benchmark = benchmark_fixture();
    const fincore::runtime_inputs pair{{"returns", frame},
                                       {"benchmark", benchmark}};
    const auto single = [&catalog, &frame](const std::string& id,
                                           const double expected) {
        const auto result =
            fincore::run(id, {{"returns", fincore::runtime_value(frame)}},
                         {}, catalog);
        const auto* value = std::get_if<double>(&result.value.data);
        EXPECT_NE(value, nullptr);
        if (value != nullptr && std::isnan(expected)) {
            EXPECT_TRUE(std::isnan(*value)) << id;
        } else if (value != nullptr) {
            EXPECT_DOUBLE_EQ(*value, expected) << id;
        }
    };
    single("metrics.ratios.sharpe_ratio", fincore::sharpe_ratio(frame));
    single("metrics.ratios.sortino_ratio", fincore::sortino_ratio(frame));
    single("metrics.ratios.calmar_ratio", fincore::calmar_ratio(frame));
    single("metrics.ratios.mar_ratio", fincore::mar_ratio(frame));
    single("metrics.ratios.omega_ratio", fincore::omega_ratio(frame));
    single("metrics.drawdown.max_drawdown", fincore::max_drawdown(frame));
    single("metrics.risk.annual_volatility",
           fincore::annual_volatility(frame));
    single("metrics.risk.tail_ratio", fincore::tail_ratio(frame));
    single("metrics.risk.conditional_value_at_risk",
           fincore::conditional_value_at_risk(frame));
    single("metrics.yearly.annual_return", fincore::annual_return(frame));
    single("metrics.returns.cum_returns_final",
           fincore::cumulative_return(frame));
    single("metrics.ratios.adjusted_sharpe_ratio",
           fincore::adjusted_sharpe_ratio(frame));
    single("metrics.ratios.burke_ratio", fincore::burke_ratio(frame));
    single("metrics.ratios.conditional_sharpe_ratio",
           fincore::conditional_sharpe_ratio(frame));
    single("metrics.ratios.sterling_ratio", fincore::sterling_ratio(frame));
    single("metrics.drawdown.max_drawdown_days",
           fincore::max_drawdown_days(frame));
    single("metrics.drawdown.max_drawdown_weeks",
           fincore::max_drawdown_weeks(frame));
    single("metrics.drawdown.max_drawdown_months",
           fincore::max_drawdown_months(frame));
    single("metrics.risk.downside_risk", fincore::downside_risk(frame));
    single("metrics.stats.skewness", fincore::skewness(frame));
    single("metrics.stats.kurtosis", fincore::kurtosis(frame));
    single("metrics.stats.win_rate", fincore::win_rate(frame));
    single("metrics.stats.loss_rate", fincore::loss_rate(frame));
    single("metrics.stats.hurst_exponent",
           fincore::hurst_exponent(frame));
    single("metrics.stats.stutzer_index", fincore::stutzer_index(frame));
    single("metrics.stats.r_cubed_turtle", fincore::r_cubed_turtle(frame));

    const auto paired = [&catalog, &pair](const std::string& id,
                                          const double expected) {
        const auto result = fincore::run(id, pair, {}, catalog);
        const auto* value = std::get_if<double>(&result.value.data);
        EXPECT_NE(value, nullptr);
        if (value != nullptr && std::isnan(expected)) {
            EXPECT_TRUE(std::isnan(*value)) << id;
        } else if (value != nullptr) {
            EXPECT_DOUBLE_EQ(*value, expected) << id;
        }
    };
    paired("metrics.ratios.information_ratio",
           fincore::information_ratio(frame, benchmark));
    paired("metrics.ratios.up_down_capture",
           fincore::up_down_capture(frame, benchmark));
    paired("metrics.risk.tracking_error",
           fincore::tracking_error(frame, benchmark));
    paired("metrics.alpha_beta.alpha",
           fincore::alpha(frame, benchmark));
    paired("metrics.alpha_beta.beta", fincore::beta(frame, benchmark));
    paired("metrics.ratios.m_squared",
           fincore::m_squared(frame, benchmark));
    paired("metrics.risk.residual_risk",
           fincore::residual_risk(frame, benchmark));
    paired("metrics.risk.beta_fragility_heuristic",
           fincore::beta_fragility_heuristic(frame, benchmark));

    const auto cumulative = fincore::run(
        "metrics.returns.cum_returns",
        {{"returns", fincore::runtime_value(frame)}}, {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(cumulative.value),
              fincore::runtime_value_digest(
                  fincore::runtime_value(fincore::cumulative_returns(frame))));

    const auto kupiec = fincore::run(
        "risk.backtesting.kupiec_lr",
        {{"observations", fincore::runtime_value(250)},
         {"exceptions", fincore::runtime_value(0)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* lr = std::get_if<double>(&kupiec.value.data);
    ASSERT_NE(lr, nullptr);
    EXPECT_NEAR(*lr, 5.025167926750753, 1e-12);

    const auto rolling = fincore::run(
        "metrics.rolling.roll_sharpe_ratio",
        {{"returns", fincore::runtime_value(frame)},
         {"window", fincore::runtime_value(3)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(rolling.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::roll_sharpe_ratio(frame, 3))));
    const auto volatility = fincore::run(
        "metrics.rolling.roll_annual_volatility",
        {{"returns", fincore::runtime_value(frame)},
         {"window", fincore::runtime_value(2)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(volatility.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::roll_annual_volatility(frame, 2))));
    const auto rolling_beta_result = fincore::run(
        "metrics.rolling.rolling_beta",
        {{"returns", fincore::runtime_value(frame)},
         {"benchmark", fincore::runtime_value(benchmark)},
         {"rolling_window", fincore::runtime_value(2)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(rolling_beta_result.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::rolling_beta(frame, benchmark, 2))));
    const auto roll_beta_result = fincore::run(
        "metrics.rolling.roll_beta",
        {{"returns", fincore::runtime_value(frame)},
         {"benchmark", fincore::runtime_value(benchmark)},
         {"window", fincore::runtime_value(2)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(roll_beta_result.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::roll_beta(frame, benchmark, 2))));

    const auto expected = fincore::run(
        "risk.calibration.expected_exception_count",
        {{"observations", fincore::runtime_value(250)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* count = std::get_if<double>(&expected.value.data);
    ASSERT_NE(count, nullptr);
    EXPECT_DOUBLE_EQ(*count, fincore::expected_exception_count(250, 0.99));
    const auto basel = fincore::run(
        "risk.calibration.basel_traffic_light",
        {{"exceptions", fincore::runtime_value(0)},
         {"observations", fincore::runtime_value(250)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* zone = std::get_if<std::string>(&basel.value.data);
    ASSERT_NE(zone, nullptr);
    EXPECT_EQ(*zone, "green");

    EXPECT_THROW(
        static_cast<void>(fincore::run(
            "metrics.sharpe_ratio",
            {{"returns", fincore::runtime_value(frame)}}, {}, catalog)),
        fincore::operation_resolution_error);
    EXPECT_THROW(
        static_cast<void>(fincore::run(
            "metrics.ratios.information_ratio",
            {{"returns", fincore::runtime_value(frame)}}, {}, catalog)),
        fincore::input_error);

    fincore::artifact_bundle bundle;
    bundle.artifacts["report.html"] = "<html></html>";
    bundle.metadata["schema_version"] = "1";
    EXPECT_EQ(bundle.artifacts.at("report.html"), "<html></html>");
}

}  // namespace
