#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/pit.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <limits>
#include <string>
#include <tuple>
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
    // B is known and effective from the first evaluation date, so its
    // universe flag is the only disqualifier: its absence here tests
    // universe filtering rather than the point-in-time cut.
    for (const auto& asset : factor.assets) {
        EXPECT_NE(asset, "B");
    }

    fincore::prepare_factor_config config;
    config.periods = {1};
    config.quantiles = 2;
    config.max_loss = 1.0;
    const auto prepared = fincore::prepare_pit_factor_data(
        fixture.observations, fixture.prices, fixture.dates, config);
    // The first two evaluation dates materialize a single asset, which
    // cannot fill two buckets, so only the last date's A/C cross-section
    // reaches the prepared panel.
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

TEST(FactorPit, FuturePublishedAndEffectiveRevisionsDoNotChangeEarlierRows) {
  const auto day = 86'400'000'000'000LL;
  const auto d0 = fincore::utc_timestamp(2022, 1, 3);
  std::vector<fincore::timestamp> evaluation_dates;
  for (int position = 1; position <= 6; ++position) {
    evaluation_dates.push_back(d0 + position * day);
  }

  std::vector<fincore::pit_observation> observations{
      {"A", {d0, d0 + day, d0 + day, 1.0}, true},
      {"B", {d0, d0 + day, d0 + day, 2.0}, true},
      {"C", {d0, d0 + day, d0 + day, 3.0}, true}};

  fincore::dataframe prices;
  std::vector<fincore::timestamp> price_dates;
  for (int position = 0; position <= 7; ++position) {
    price_dates.push_back(d0 + position * day);
  }
  prices.load_index(std::move(price_dates));
  for (const auto &[asset, start, increment] :
       {std::tuple{"A", 100.0, 1.0}, std::tuple{"B", 200.0, 2.0},
        std::tuple{"C", 300.0, 3.0}}) {
    std::vector<double> values;
    for (int position = 0; position <= 7; ++position) {
      values.push_back(start + increment * position);
    }
    prices.load_column<double>(asset, std::move(values),
                               hmdf::nan_policy::dont_pad_with_nans);
  }

  fincore::prepare_factor_config config;
  config.periods = {1};
  config.quantiles = 3;
  config.max_loss = 1.0;
  const auto baseline = fincore::prepare_pit_factor_data(
      observations, prices, evaluation_dates, config);

  auto with_future_revisions = observations;
  // A's later publication is not known until d5.
  with_future_revisions.push_back(
      {"A", {d0 + 2 * day, d0 + 5 * day, d0 + 5 * day, 4.0}, true});
  // B knows its later value at d2, but it does not take effect until d5.
  with_future_revisions.push_back(
      {"B", {d0 + 2 * day, d0 + 2 * day, d0 + 5 * day, -2.0}, true});
  const auto revised = fincore::prepare_pit_factor_data(
      with_future_revisions, prices, evaluation_dates, config);

  const auto find_row =
      [](const fincore::prepared_factor_data &prepared,
         const fincore::timestamp date,
         const std::string &asset) -> const fincore::factor_row * {
    const auto found =
        std::find_if(prepared.rows.begin(), prepared.rows.end(),
                     [date, &asset](const auto &row) {
                       return row.date == date && row.asset == asset;
                     });
    return found == prepared.rows.end() ? nullptr : &*found;
  };

  for (int position = 1; position < 5; ++position) {
    const auto date = d0 + position * day;
    for (const std::string asset : {"A", "B", "C"}) {
      const auto *before = find_row(baseline, date, asset);
      const auto *after = find_row(revised, date, asset);
      ASSERT_NE(before, nullptr);
      ASSERT_NE(after, nullptr);
      EXPECT_DOUBLE_EQ(after->factor, before->factor);
      EXPECT_EQ(after->quantile, before->quantile);
      ASSERT_EQ(after->forward_returns.size(), 1U);
      ASSERT_EQ(before->forward_returns.size(), 1U);
      EXPECT_DOUBLE_EQ(after->forward_returns[0], before->forward_returns[0]);
    }
  }

  const auto effective_date = d0 + 5 * day;
  const auto *original_a = find_row(baseline, effective_date, "A");
  const auto *revised_a = find_row(revised, effective_date, "A");
  const auto *original_b = find_row(baseline, effective_date, "B");
  const auto *revised_b = find_row(revised, effective_date, "B");
  ASSERT_NE(original_a, nullptr);
  ASSERT_NE(revised_a, nullptr);
  ASSERT_NE(original_b, nullptr);
  ASSERT_NE(revised_b, nullptr);
  EXPECT_DOUBLE_EQ(original_a->factor, 1.0);
  EXPECT_DOUBLE_EQ(revised_a->factor, 4.0);
  EXPECT_DOUBLE_EQ(original_b->factor, 2.0);
  EXPECT_DOUBLE_EQ(revised_b->factor, -2.0);
}

TEST(FactorPit, RejectsInfinitePointValues) {
  const std::vector<fincore::pit_point> points{
      {1, 1, 1, std::numeric_limits<double>::infinity()}};
  EXPECT_THROW(fincore::validate_pit_alignment(points),
               fincore::factor_data_error);
}

} // namespace
