#include <fincore/calendar.hpp>
#include <fincore/errors.hpp>
#include <fincore/risk/report.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <string>

namespace {

fincore::risk_validation_report sample_report() {
    fincore::risk_validation_report report;
    report.status = fincore::walk_forward_status::ok;
    report.inputs_digest = "digest";
    report.schema_version = fincore::risk_validation_report_schema_version;
    report.forecast_events.push_back(
        {fincore::utc_timestamp(2024, 1, 2), -0.02, -0.03, true, true});
    report.forecast_events.push_back(
        {fincore::utc_timestamp(2024, 1, 3), -0.015, 0.004, false, false});
    report.refits.push_back(
        {fincore::utc_timestamp(2024, 1, 2), -0.02, 0.001, 0.012, 2U});
    report.diagnostics.method = "normal";
    report.diagnostics.quantile_method = "normal_ppf";
    report.diagnostics.window = 2U;
    report.diagnostics.refit_cadence = 2U;
    report.diagnostics.forecast_count = 2U;
    report.disclosure = std::string{fincore::basel_reference_disclosure};
    return report;
}

std::string read_text(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    return std::string{std::istreambuf_iterator<char>(input),
                       std::istreambuf_iterator<char>()};
}

}  // namespace

TEST(RiskReportSerialization, DictProjectionMatchesNativeReportFields) {
    const auto report = sample_report();
    const auto dictionary = report.to_dict();
    EXPECT_EQ(dictionary.schema_version,
              fincore::risk_validation_report_schema_version);
    EXPECT_EQ(dictionary.status, "ok");
    EXPECT_EQ(dictionary.inputs_digest, "digest");
    EXPECT_FALSE(dictionary.timestamp_index_name.has_value());
    EXPECT_FALSE(dictionary.timestamp_timezone.has_value());
    ASSERT_EQ(dictionary.forecast_events.size(), 2U);
    EXPECT_DOUBLE_EQ(dictionary.forecast_events[0].forecast, -0.02);
    EXPECT_TRUE(dictionary.forecast_events[0].exception);
    ASSERT_EQ(dictionary.refits.size(), 1U);
    EXPECT_DOUBLE_EQ(dictionary.refits[0].standard_deviation, 0.012);
    EXPECT_FALSE(dictionary.backtest.has_value());
    EXPECT_TRUE(dictionary.traffic_light_zone.empty());
    EXPECT_EQ(dictionary.disclosure, report.disclosure);
}

TEST(RiskReportSerialization, WriteJsonRoundTripsDeterministically) {
    const auto report = sample_report();
    const auto path = std::filesystem::temp_directory_path() /
                      "fincore_risk_report_serialization_test.json";
    std::filesystem::remove(path);
    const auto returned = report.write_json(path);
    EXPECT_EQ(returned, path);
    const std::string first = read_text(path);
    EXPECT_EQ(first, report.to_json());
    EXPECT_FALSE(first.empty());
    // The trailing newline and the byte-identical second file are what make the
    // output safe to diff and hash: same value in, same bytes out, no path in it.
    EXPECT_EQ(first.back(), '\n');
    EXPECT_EQ(report.to_json(), report.to_json());

    const auto second_path = std::filesystem::temp_directory_path() /
                             "fincore_risk_report_serialization_test_2.json";
    static_cast<void>(report.write_json(second_path));
    EXPECT_EQ(read_text(second_path), first);
    std::filesystem::remove(path);
    std::filesystem::remove(second_path);
}

TEST(RiskReportSerialization, RejectsNonFiniteValuesAsStructuredErrors) {
    auto report = sample_report();
    report.forecast_events[0].forecast =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(static_cast<void>(report.to_dict()), fincore::fincore_error);
    const auto path = std::filesystem::temp_directory_path() /
                      "fincore_risk_report_serialization_test_bad.json";
    std::filesystem::remove(path);
    EXPECT_THROW(static_cast<void>(report.write_json(path)),
                 fincore::fincore_error);
    // Rejection happens before the stream is opened, so a failed write must not
    // leave an empty or half-written file behind.
    EXPECT_FALSE(std::filesystem::exists(path));

    auto infinite = sample_report();
    infinite.refits[0].standard_deviation =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(static_cast<void>(infinite.to_dict()), fincore::fincore_error);
}
