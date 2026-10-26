#include <fincore/portfolio.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

fincore::dataframe returns_frame() {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{101, 205});
    frame.load_column<double>(
        "return", {0.1, std::numeric_limits<double>::quiet_NaN()},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe positions_frame() {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{101, 205});
    frame.load_column<double>("cash", {20.0, 10.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<std::string>("symbol", {"A", "B"},
                                   hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<int>("shares", {3, -4},
                           hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe transactions_frame() {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{300, 301});
    frame.load_column<std::string>("symbol", {"X", "Y"},
                                   hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("amount", {1.5, -2.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<std::string>("venue", {"lit", "dark"},
                                   hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe benchmark_frame() {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{101, 205});
    frame.load_column<double>("benchmark", {0.03, 0.04},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(PortfolioInputs, PreservesOmittedAndEmptySuppliedFields) {
    const fincore::portfolio_inputs omitted;
    // "Not supplied" and "supplied but empty" are different states: the first
    // materializes to nothing, the second to one key holding an empty frame.
    EXPECT_TRUE(omitted.materialize().empty());

    fincore::dataframe empty;
    const fincore::portfolio_inputs supplied_empty(
        std::nullopt, empty, std::nullopt, std::nullopt);
    const auto materialized = supplied_empty.materialize();
    ASSERT_EQ(materialized.size(), 1U);
    ASSERT_NE(materialized.find("positions"), materialized.end());
    EXPECT_TRUE(materialized.at("positions").get_index().empty());
    EXPECT_EQ(materialized.at("positions").shape().second, 0U);
}

TEST(PortfolioInputs, CopiesAtIngestionAndMaterializesIndependentNativeFrames) {
    auto returns = returns_frame();
    auto positions = positions_frame();
    auto transactions = transactions_frame();
    auto benchmark = benchmark_frame();

    const fincore::portfolio_inputs inputs(returns, positions, transactions,
                                           benchmark);

    // The fixture keeps a NaN return and a negative share count on purpose:
    // ingestion copies what it is given instead of validating it, so those values
    // have to survive unchanged.
    returns.get_column<double>("return")[0] = 9.0;
    positions.get_column<double>("cash")[0] = 999.0;
    positions.get_column<std::string>("symbol")[0] = "changed";
    positions.get_column<int>("shares")[0] = 88;
    transactions.get_column<std::string>("venue")[0] = "changed";
    benchmark.get_column<double>("benchmark")[0] = -9.0;

    auto first = inputs.materialize();
    ASSERT_EQ(first.size(), 4U);
    ASSERT_EQ(first.count("returns"), 1U);
    ASSERT_EQ(first.count("positions"), 1U);
    ASSERT_EQ(first.count("transactions"), 1U);
    ASSERT_EQ(first.count("benchmark_returns"), 1U);

    EXPECT_EQ(first.at("returns").get_index(),
              (std::vector<fincore::timestamp>{101, 205}));
    EXPECT_DOUBLE_EQ(first.at("returns").get_column<double>("return")[0],
                     0.1);
    EXPECT_TRUE(std::isnan(
        first.at("returns").get_column<double>("return")[1]));
    EXPECT_DOUBLE_EQ(first.at("positions").get_column<double>("cash")[0],
                     20.0);
    EXPECT_EQ(first.at("positions").get_column<std::string>("symbol")[0],
              "A");
    EXPECT_EQ(first.at("positions").get_column<int>("shares")[1], -4);
    EXPECT_EQ(first.at("transactions").get_column<std::string>("venue")[0],
              "lit");
    EXPECT_DOUBLE_EQ(
        first.at("transactions").get_column<double>("amount")[1], -2.0);
    EXPECT_DOUBLE_EQ(
        first.at("benchmark_returns").get_column<double>("benchmark")[0],
        0.03);

    first.at("returns").get_column<double>("return")[0] = -100.0;
    first.at("positions").get_column<int>("shares")[1] = 700;
    first.at("transactions").get_column<std::string>("symbol")[0] = "mutated";
    first.at("benchmark_returns").get_column<double>("benchmark")[0] = 100.0;

    const auto second = inputs.materialize();
    EXPECT_DOUBLE_EQ(second.at("returns").get_column<double>("return")[0],
                     0.1);
    EXPECT_EQ(second.at("positions").get_column<int>("shares")[1], -4);
    EXPECT_EQ(second.at("transactions").get_column<std::string>("symbol")[0],
              "X");
    EXPECT_DOUBLE_EQ(
        second.at("benchmark_returns").get_column<double>("benchmark")[0],
        0.03);
}

TEST(PortfolioInputs, OwnsValuesBeyondCallerLifetimeAndSupportsRepeatedMaterialization) {
    const auto make_owned_inputs = [] {
        auto returns = returns_frame();
        auto transactions = transactions_frame();
        return fincore::portfolio_inputs(std::move(returns), std::nullopt,
                                         std::move(transactions), std::nullopt);
    };

    const auto inputs = make_owned_inputs();
    const auto first = inputs.materialize();
    const auto second = inputs.materialize();
    ASSERT_EQ(first.size(), 2U);
    ASSERT_EQ(second.size(), 2U);
    EXPECT_EQ(first.at("returns").get_index(),
              (std::vector<fincore::timestamp>{101, 205}));
    EXPECT_EQ(second.at("transactions").get_column<std::string>("symbol")[1],
              "Y");
    EXPECT_NE(&first.at("returns"), &second.at("returns"));
}

}  // namespace
