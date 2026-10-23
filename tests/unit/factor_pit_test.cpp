#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/pit.hpp>

#include <gtest/gtest.h>

#include <string>
#include <vector>

namespace {

struct pit_fixture {
    std::vector<fincore::pit_observation> observations;
    std::vector<fincore::timestamp> dates;
    fincore::dataframe prices;
};

pit_fixture make_fixture() {
    const auto day = 86'400'000'000'000LL;
    const auto d0 = fincore::utc_timestamp(2022, 1, 3);
    pit_fixture fixture;
    fixture.dates = {d0 + day, d0 + 2 * day, d0 + 3 * day};
    fixture.observations = {
        {"A", {d0, d0 + day, d0 + day, 1.0}, true},
        {"A", {d0 + day, d0 + 2 * day, d0 + 2 * day, 2.0}, true},
        {"B", {d0, d0 + day, d0 + day, 3.0}, false},
        {"C", {d0, d0 + 3 * day, d0 + 3 * day, 4.0}, true}};
    std::vector<fincore::timestamp> index;
    for (int position = 0; position < 6; ++position) {
        index.push_back(d0 + position * day);
    }
    fixture.prices.load_index(std::move(index));
    fixture.prices.load_column<double>(
        "A", std::vector<double>{100, 101, 102, 103, 104, 105},
        hmdf::nan_policy::dont_pad_with_nans);
    fixture.prices.load_column<double>(
        "C", std::vector<double>{50, 51, 52, 53, 54, 55},
        hmdf::nan_policy::dont_pad_with_nans);
    return fixture;
}

TEST(FactorPit, MaterializesOnlyKnownEffectiveUniverseValues) {
    auto fixture = make_fixture();
    const auto factor =
        fincore::materialize_pit_factor(fixture.observations, fixture.dates);
    ASSERT_EQ(factor.dates.size(), 4U);
    EXPECT_EQ(factor.dates[0], fixture.dates[0]);
    EXPECT_EQ(factor.assets[0], "A");
    EXPECT_DOUBLE_EQ(factor.values[0], 1.0);
    EXPECT_EQ(factor.dates[1], fixture.dates[1]);
    EXPECT_EQ(factor.assets[1], "A");
    EXPECT_DOUBLE_EQ(factor.values[1], 2.0);
    EXPECT_EQ(factor.dates[2], fixture.dates[2]);
    EXPECT_EQ(factor.assets[2], "A");
    EXPECT_DOUBLE_EQ(factor.values[2], 2.0);
    EXPECT_EQ(factor.dates[3], fixture.dates[2]);
    EXPECT_EQ(factor.assets[3], "C");
    EXPECT_DOUBLE_EQ(factor.values[3], 4.0);
    for (const auto& asset : factor.assets) {
        EXPECT_NE(asset, "B");
    }

    fincore::prepare_factor_config config;
    config.periods = {1};
    config.quantiles = 2;
    config.max_loss = 1.0;
    const auto prepared = fincore::prepare_pit_factor_data(
        fixture.observations, fixture.prices, fixture.dates, config);
    ASSERT_EQ(prepared.rows.size(), 2U);
    EXPECT_EQ(prepared.forward_labels.size(), 1U);
    EXPECT_EQ(prepared.rows.front().asset, "A");
    EXPECT_EQ(prepared.rows.back().asset, "C");
}

TEST(FactorPit, RejectsLookAheadAndDuplicateRevisions) {
    auto fixture = make_fixture();
    auto bad = fixture.observations;
    bad[0].point.known_at = bad[0].point.as_of - 1;
    EXPECT_THROW(static_cast<void>(fincore::materialize_pit_factor(
                     bad, fixture.dates)),
                 fincore::factor_data_error);

    bad = fixture.observations;
    bad[0].point.effective_from = bad[0].point.known_at - 1;
    EXPECT_THROW(static_cast<void>(fincore::materialize_pit_factor(
                     bad, fixture.dates)),
                 fincore::factor_data_error);

    bad = fixture.observations;
    bad.push_back(bad[0]);
    EXPECT_THROW(static_cast<void>(fincore::materialize_pit_factor(
                     bad, fixture.dates)),
                 fincore::factor_data_error);

    auto unsorted = fixture.dates;
    std::swap(unsorted[0], unsorted[1]);
    EXPECT_THROW(static_cast<void>(fincore::materialize_pit_factor(
                     fixture.observations, unsorted)),
                 fincore::factor_data_error);

    auto config = fincore::prepare_factor_config{};
    config.periods = {1};
    config.filter_zscore = 2.0;
    EXPECT_THROW(static_cast<void>(fincore::prepare_pit_factor_data(
                     fixture.observations, fixture.prices, fixture.dates,
                     config)),
                 fincore::factor_data_error);
}

}  // namespace
