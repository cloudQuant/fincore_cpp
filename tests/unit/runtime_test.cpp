#include <fincore/calendar.hpp>
#include <fincore/metrics/ratios.hpp>
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
    ASSERT_EQ(catalog.operations().size(), 2U);
    const auto frame = returns_fixture();
    const auto sharpe = fincore::run(
        "metrics.sharpe_ratio", {{"returns", fincore::runtime_value(frame)}},
        {}, catalog);
    const auto* value = std::get_if<double>(&sharpe.value.data);
    ASSERT_NE(value, nullptr);
    EXPECT_NEAR(*value, fincore::sharpe_ratio(frame), 1e-15);
    const auto kupiec = fincore::run(
        "risk.backtesting.kupiec_lr",
        {{"observations", fincore::runtime_value(250)},
         {"exceptions", fincore::runtime_value(0)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* lr = std::get_if<double>(&kupiec.value.data);
    ASSERT_NE(lr, nullptr);
    EXPECT_NEAR(*lr, 5.025167926750753, 1e-12);

    fincore::artifact_bundle bundle;
    bundle.artifacts["report.html"] = "<html></html>";
    bundle.metadata["schema_version"] = "1";
    EXPECT_EQ(bundle.artifacts.at("report.html"), "<html></html>");
}

}  // namespace
