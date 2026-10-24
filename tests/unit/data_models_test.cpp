#include <fincore/calendar.hpp>
#include <fincore/data.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/errors.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <map>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

fincore::dataframe make_prices() {
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2), fincore::utc_timestamp(2020, 1, 3),
      fincore::utc_timestamp(2020, 1, 6)});
  frame.load_column<double>(
      "Adj Close", std::vector<double>{100.0, 110.0, 99.0},
      hmdf::nan_policy::dont_pad_with_nans);
  frame.load_column<double>(
      "Close", std::vector<double>{101.0, 111.0, 100.0},
      hmdf::nan_policy::dont_pad_with_nans);
  return frame;
}

class fake_provider final : public fincore::data_provider {
 public:
  std::string provider_name() const override { return "Fake"; }

  fincore::dataframe fetch(std::string_view symbol, std::string_view,
                           std::string_view, std::string_view,
                           bool) const override {
    if (symbol == "BAD") {
      throw std::runtime_error("no data for BAD");
    }
    return make_prices();
  }

  std::map<std::string, std::string> get_info(
      std::string_view symbol) const override {
    return {{"symbol", std::string(symbol)}, {"name", "Fake Corp"}};
  }
};

TEST(RequestPolicyTest, DefaultsAndValidation) {
  fincore::request_policy policy;
  EXPECT_DOUBLE_EQ(policy.connect_timeout, 10.0);
  EXPECT_DOUBLE_EQ(policy.read_timeout, 30.0);
  EXPECT_DOUBLE_EQ(policy.total_timeout, 60.0);
  EXPECT_EQ(policy.max_attempts, 3);
  EXPECT_NO_THROW(policy.validate());
  EXPECT_THROW(fincore::request_policy(0.0, 30.0, 60.0, 3),
               fincore::fincore_error);
  EXPECT_THROW(fincore::request_policy(10.0, -1.0, 60.0, 3),
               fincore::fincore_error);
  EXPECT_THROW(fincore::request_policy(10.0, 30.0, 60.0, 0),
               fincore::fincore_error);
}

TEST(RequestPolicyTest, RetryClassificationAndAttempts) {
  const fincore::request_policy policy;
  EXPECT_TRUE(policy.should_retry(fincore::transport_error_kind::connection));
  EXPECT_TRUE(policy.should_retry(fincore::transport_error_kind::timeout));
  EXPECT_TRUE(
      policy.should_retry(fincore::transport_error_kind::operating_system));
  EXPECT_FALSE(policy.should_retry(fincore::transport_error_kind::validation));
  EXPECT_FALSE(policy.should_retry(fincore::transport_error_kind::none));
  EXPECT_EQ(policy.remaining_attempts(1), 2);
  EXPECT_EQ(policy.remaining_attempts(3), 0);
  EXPECT_EQ(policy.remaining_attempts(9), 0);
}

TEST(DataProviderTest, ValidateDatesParsesIsoAndRejectsInvalidOrder) {
  const auto [start, end] =
      fincore::data_provider::validate_dates("2020-01-01", "2020-12-31");
  EXPECT_EQ(start, fincore::utc_timestamp(2020, 1, 1));
  EXPECT_EQ(end, fincore::utc_timestamp(2020, 12, 31));

  const auto [with_time_start, with_time_end] =
      fincore::data_provider::validate_dates("2020-01-01T09:30:00",
                                             "2020-01-02 16:00:00");
  EXPECT_EQ(with_time_start, fincore::utc_timestamp(2020, 1, 1, 9, 30));
  EXPECT_EQ(with_time_end, fincore::utc_timestamp(2020, 1, 2, 16, 0));

  EXPECT_THROW(fincore::data_provider::validate_dates("2020-01-01",
                                                      "2020-01-01"),
               fincore::fincore_error);
  EXPECT_THROW(fincore::data_provider::validate_dates("not-a-date",
                                                      "2020-01-01"),
               fincore::fincore_error);
  EXPECT_THROW(fincore::data_provider::validate_dates("2020-02-30",
                                                      "2020-03-01"),
               fincore::fincore_error);
}

TEST(DataProviderTest, ToReturnsUsesAdjCloseAndFallsBackToClose) {
  const fake_provider provider;
  const auto prices = make_prices();
  const auto returns = provider.to_returns(prices);
  ASSERT_EQ(returns.size(), 2U);
  EXPECT_DOUBLE_EQ(returns[0], 0.1);
  EXPECT_DOUBLE_EQ(returns[1], -0.1);

  fincore::dataframe close_only;
  close_only.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2), fincore::utc_timestamp(2020, 1, 3)});
  close_only.load_column<double>(
      "Close", std::vector<double>{50.0, 75.0},
      hmdf::nan_policy::dont_pad_with_nans);
  const auto fallback = provider.to_returns(close_only, "Adj Close");
  ASSERT_EQ(fallback.size(), 1U);
  EXPECT_DOUBLE_EQ(fallback[0], 0.5);
}

TEST(DataProviderTest, ToReturnsDropsNaNButKeepsInfiniteChange) {
  const fake_provider provider;
  fincore::dataframe frame;
  frame.load_index(std::vector<fincore::timestamp>{
      fincore::utc_timestamp(2020, 1, 2), fincore::utc_timestamp(2020, 1, 3),
      fincore::utc_timestamp(2020, 1, 6), fincore::utc_timestamp(2020, 1, 7)});
  frame.load_column<double>(
      "Close", std::vector<double>{0.0, 5.0, std::nan(""), 7.0},
      hmdf::nan_policy::dont_pad_with_nans);
  const auto returns = provider.to_returns(frame);
  ASSERT_EQ(returns.size(), 1U);
  EXPECT_TRUE(std::isinf(returns[0]));
  EXPECT_GT(returns[0], 0.0);
}

TEST(DataProviderTest, FetchMultipleNonStrictReturnsEmptyFailure) {
  const fake_provider provider;
  const auto results = provider.fetch_multiple({"GOOD", "BAD"}, "2020-01-01",
                                               "2020-12-31");
  ASSERT_EQ(results.size(), 2U);
  EXPECT_EQ(results.at("GOOD").get_index().size(), 3U);
  EXPECT_TRUE(results.at("BAD").get_index().empty());
}

TEST(DataProviderTest, FetchMultipleStrictRaisesBatchFetchError) {
  const fake_provider provider;
  try {
    provider.fetch_multiple({"GOOD", "BAD"}, "2020-01-01", "2020-12-31",
                            "1d", std::nullopt, true);
    FAIL() << "expected batch_fetch_error";
  } catch (const fincore::batch_fetch_error& error) {
    EXPECT_EQ(error.category(), "BatchFetchError");
    EXPECT_EQ(error.provider(), "Fake");
    ASSERT_EQ(error.errors().size(), 1U);
    EXPECT_NE(error.errors().at("BAD").find("no data for BAD"),
              std::string::npos);
    ASSERT_EQ(error.partial_results().size(), 1U);
    EXPECT_TRUE(error.partial_results().count("GOOD") == 1U);
    EXPECT_NE(std::string(error.what()).find("1 symbol(s)"), std::string::npos);
  }
}

TEST(DataSnapshotTest, FromFrameIsContentAddressedAndDefensivelyCopied) {
  auto frame = make_prices();
  auto snapshot = fincore::data_snapshot::from_frame(
      frame, "offline", "2020-01-01", "2020-12-31", "2020-12-31T23:59:59");
  EXPECT_EQ(snapshot.schema_version(), 1);
  EXPECT_EQ(snapshot.content_sha256().size(), 64U);
  EXPECT_EQ(snapshot.content_sha256(), fincore::dataframe_content_sha256(frame));

  const auto identity = snapshot.identity_kwargs();
  EXPECT_EQ(identity.at("provider"), "offline");
  EXPECT_EQ(identity.at("price_adjustment"), "raw");
  EXPECT_EQ(identity.at("timezone"), "UTC");

  const std::string manifest = snapshot.to_manifest();
  EXPECT_NE(manifest.find("\"schema_version\": 1"), std::string::npos);
  EXPECT_NE(manifest.find("\"content_sha256\""), std::string::npos);
  EXPECT_EQ(manifest.find("api_key"), std::string::npos);

  frame.get_column<double>("Adj Close")[0] = 1.0;
  EXPECT_DOUBLE_EQ(snapshot.stored_data().get_column<double>("Adj Close")[0],
                   100.0);

  auto copy = snapshot.data();
  copy.get_column<double>("Adj Close")[0] = -1.0;
  EXPECT_DOUBLE_EQ(snapshot.stored_data().get_column<double>("Adj Close")[0],
                   100.0);
}

TEST(DataSnapshotTest, CanonicalCsvIncludesIndexAndColumns) {
  const auto frame = make_prices();
  const std::string csv = fincore::canonical_dataframe_csv(frame);
  EXPECT_EQ(csv.rfind("index,Adj Close,Close\n", 0U), 0U);
  EXPECT_NE(csv.find("1577923200000000000,100.0,"), std::string::npos);
  EXPECT_EQ(csv.back(), '\n');
  EXPECT_EQ(csv, fincore::canonical_dataframe_csv(frame));
}

}  // namespace
