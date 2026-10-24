#include <fincore/calendar.hpp>
#include <fincore/errors.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/portfolio/transactions.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

TEST(TransactionMappingTest, RejectsNonFiniteAndEmptySymbol) {
  fincore::portfolio_transaction transaction{
      fincore::utc_timestamp(2020, 1, 2), "AAA", 10.0, 100.0};
  EXPECT_NO_THROW(fincore::map_transaction(transaction));

  transaction.amount = std::numeric_limits<double>::infinity();
  EXPECT_THROW(fincore::map_transaction(transaction), fincore::fincore_error);

  transaction.amount = 10.0;
  transaction.price = std::nan("");
  EXPECT_THROW(fincore::map_transaction(transaction), fincore::fincore_error);

  transaction.price = 100.0;
  transaction.symbol.clear();
  EXPECT_THROW(fincore::map_transaction(transaction), fincore::fincore_error);
}

TEST(TransactionMappingTest, BuildsOwningFrameWithDuplicateDates) {
  const auto first = fincore::utc_timestamp(2020, 1, 2);
  const std::vector<fincore::portfolio_transaction> transactions{
      {first, "AAA", 10.0, 100.0},
      {first, "BBB", -4.0, 25.0},
      {fincore::utc_timestamp(2020, 1, 3), "AAA", 2.0, 101.0}};
  auto frame = fincore::make_transaction_frame(transactions);
  ASSERT_EQ(frame.shape().first, 3U);
  ASSERT_EQ(frame.shape().second, 3U);
  EXPECT_EQ(frame.get_index()[0], first);
  EXPECT_EQ(frame.get_index()[1], first);
  EXPECT_EQ(frame.get_column<std::string>("symbol")[1], "BBB");
  EXPECT_DOUBLE_EQ(frame.get_column<double>("amount")[2], 2.0);
  EXPECT_DOUBLE_EQ(frame.get_column<double>("price")[0], 100.0);

  frame.get_column<double>("amount")[0] = 999.0;
  EXPECT_DOUBLE_EQ(transactions[0].amount, 10.0);
}

TEST(DrawdownUnderwaterTest, FindsPeakValleyAndRecovery) {
  const auto start = fincore::utc_timestamp(2020, 1, 2);
  constexpr fincore::timestamp day = 86'400'000'000'000LL;
  std::vector<fincore::timestamp> index;
  for (int offset = 0; offset < 7; ++offset) {
    index.push_back(start + static_cast<fincore::timestamp>(offset) * day);
  }
  const auto series = fincore::make_series(
      index, "drawdown", {0.0, -0.1, -0.2, -0.05, 0.0, -0.3, 0.0});
  const auto window = fincore::max_drawdown_underwater(series);
  ASSERT_TRUE(window.has_value());
  EXPECT_EQ(window->peak, index[4]);
  EXPECT_EQ(window->valley, index[5]);
  ASSERT_TRUE(window->recovery.has_value());
  EXPECT_EQ(*window->recovery, index[6]);
}

TEST(DrawdownUnderwaterTest, NeverUnderwaterReturnsNullopt) {
  const auto series = fincore::make_series(
      {fincore::utc_timestamp(2020, 1, 2), fincore::utc_timestamp(2020, 1, 3)},
      "drawdown", {0.0, 0.0});
  EXPECT_FALSE(fincore::max_drawdown_underwater(series).has_value());
}

}  // namespace
