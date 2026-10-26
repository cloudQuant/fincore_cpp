#include <fincore/calendar.hpp>
#include <fincore/risk/report.hpp>
#include <fincore/risk/specs.hpp>
#include <fincore/risk/walk_forward.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

// A negative shift_from disables the shift entirely, so the default fixture is a
// pure deterministic sine path; the only caller that overrides it rebuilds its
// input mid-test and checks that an already built report does not follow.
fincore::dataframe report_fixture(int count, int shift_from = -1,
                                  double future_shift = 0.0) {
    std::vector<fincore::timestamp> index;
    index.reserve(static_cast<std::size_t>(count));
    std::vector<double> values;
    values.reserve(static_cast<std::size_t>(count));
    for (int position = 0; position < count; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 1) +
                        position * 86'400'000'000'000LL);
        double value = 0.01 * std::sin(static_cast<double>(position) * 0.7);
        if (shift_from >= 0 && position >= shift_from) {
            value += future_shift;
        }
        values.push_back(value);
    }
    return fincore::make_series(index, "returns", values);
}

fincore::risk_model_spec normal_spec() {
    fincore::risk_model_spec spec;
    spec.distribution = "normal";
    spec.window = 50U;
    spec.refit_cadence = 5U;
    spec.confidence_level = 0.99;
    return spec;
}

fincore::risk_model_spec historical_spec() {
    fincore::risk_model_spec spec;
    spec.distribution = "historical";
    spec.window = 100U;
    spec.refit_cadence = 10U;
    spec.confidence_level = 0.95;
    return spec;
}

fincore::risk_model_spec small_spec() {
    fincore::risk_model_spec spec;
    spec.distribution = "normal";
    spec.window = 5U;
    spec.refit_cadence = 2U;
    spec.confidence_level = 0.90;
    return spec;
}

const std::string& disclosure_text() {
    static const std::string value{fincore::basel_reference_disclosure};
    return value;
}

TEST(RiskReport, ValidatesSpecificationContract) {
    EXPECT_NO_THROW(fincore::validate_risk_model_spec({}));

    fincore::risk_model_spec spec;
    spec.confidence_level = 1.0;
    EXPECT_THROW(fincore::validate_risk_model_spec(spec),
                 fincore::input_error);
    spec.confidence_level = 0.0;
    EXPECT_THROW(fincore::validate_risk_model_spec(spec),
                 fincore::input_error);
    spec.confidence_level = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::validate_risk_model_spec(spec),
                 fincore::input_error);

    fincore::risk_model_spec bad;
    bad.distribution = "student_t";
    EXPECT_THROW(fincore::validate_risk_model_spec(bad),
                 fincore::input_error);
    bad = {};
    bad.window = 1U;
    EXPECT_THROW(fincore::validate_risk_model_spec(bad),
                 fincore::input_error);
    bad = {};
    bad.refit_cadence = 0U;
    EXPECT_THROW(fincore::validate_risk_model_spec(bad),
                 fincore::input_error);
    bad = {};
    bad.model_version = "2.0";
    EXPECT_THROW(fincore::validate_risk_model_spec(bad),
                 fincore::input_error);
    bad = {};
    bad.sign_convention = "losses_positive";
    EXPECT_THROW(fincore::validate_risk_model_spec(bad),
                 fincore::input_error);
    bad = {};
    bad.horizon = 0U;
    EXPECT_THROW(fincore::validate_risk_model_spec(bad),
                 fincore::input_error);

    EXPECT_EQ(fincore::risk_forecast_target_name(
                  fincore::risk_forecast_target::var),
              "var");
    EXPECT_EQ(fincore::risk_forecast_target_name(
                  fincore::risk_forecast_target::pair),
              "pair");
    EXPECT_EQ(fincore::risk_tail_name(fincore::risk_tail::upper), "upper");
}

TEST(RiskReport, NormalReportMatchesPythonOracle) {
    const auto result = fincore::walk_forward_var(report_fixture(300),
                                                  normal_spec());
    const auto report = fincore::build_risk_validation_report(result);

    EXPECT_EQ(report.status, fincore::walk_forward_status::ok);
    EXPECT_EQ(report.schema_version, 1);
    EXPECT_EQ(report.disclosure, disclosure_text());
    EXPECT_FALSE(report.timestamp_index_name.has_value());
    EXPECT_FALSE(report.timestamp_timezone.has_value());
    EXPECT_EQ(report.specification.window, 50U);
    EXPECT_EQ(report.specification.refit_cadence, 5U);
    EXPECT_EQ(report.specification.horizon, 1U);

    ASSERT_EQ(report.forecast_events.size(), 250U);
    ASSERT_EQ(report.refits.size(), 50U);
    EXPECT_EQ(report.forecast_events.front().date,
              fincore::utc_timestamp(2021, 2, 20));
    EXPECT_NEAR(report.forecast_events.front().forecast,
                -0.015891935566622144, 1e-12);
    EXPECT_NEAR(report.forecast_events.front().realized,
                -0.00428182669496151, 1e-12);
    EXPECT_FALSE(report.forecast_events.front().exception);
    EXPECT_TRUE(report.forecast_events.front().refit);
    EXPECT_EQ(report.forecast_events.back().date,
              fincore::utc_timestamp(2021, 10, 27));
    EXPECT_NEAR(report.forecast_events.back().forecast,
                -0.016807926786139734, 1e-12);
    EXPECT_NEAR(report.forecast_events.back().realized,
                0.009271403561777842, 1e-12);
    EXPECT_FALSE(report.forecast_events.back().refit);

    EXPECT_EQ(report.refits.front().date,
              fincore::utc_timestamp(2021, 2, 20));
    EXPECT_NEAR(report.refits.front().mean, 0.000564337061249735, 1e-12);
    EXPECT_NEAR(report.refits.front().standard_deviation,
                0.007073865784005689, 1e-12);
    EXPECT_EQ(report.refits.front().n_observations, 50U);

    EXPECT_EQ(report.diagnostics.method, "normal");
    EXPECT_EQ(report.diagnostics.quantile_method, "normal_ppf");
    EXPECT_EQ(report.diagnostics.window, 50U);
    EXPECT_EQ(report.diagnostics.refit_cadence, 5U);
    EXPECT_EQ(report.diagnostics.forecast_count, 250U);
    EXPECT_EQ(report.diagnostics.backtest_status, "inconclusive");

    EXPECT_TRUE(report.has_backtest);
    EXPECT_EQ(report.backtest.observations, 250U);
    EXPECT_EQ(report.backtest.exceptions, 0U);
    EXPECT_NEAR(report.backtest.expected_exceptions, 2.500000000000002,
                1e-15);
    EXPECT_NEAR(report.backtest.kupiec_lr, 5.025167926750753, 1e-12);
    EXPECT_NEAR(report.backtest.kupiec_pvalue, 0.02498150305344931, 1e-12);
    EXPECT_DOUBLE_EQ(report.backtest.christoffersen_lr, 0.0);
    EXPECT_DOUBLE_EQ(report.backtest.christoffersen_pvalue, 1.0);
    EXPECT_EQ(report.backtest.status,
              fincore::risk_backtest_status::inconclusive);
    EXPECT_DOUBLE_EQ(report.backtest.diagnostics.at("small_sample"), 1.0);
    EXPECT_EQ(report.traffic_light_zone, "green");

    const std::string json = report.to_json();
    EXPECT_EQ(json, report.to_json());
    EXPECT_EQ(json.back(), '\n');
    EXPECT_NE(json.find("\"forecast_count\": 250"), std::string::npos);
    EXPECT_NE(json.find("\"quantile_method\": \"normal_ppf\""),
              std::string::npos);
    EXPECT_NE(json.find("\"n_observations\": 50.0"), std::string::npos);
    EXPECT_NE(json.find("\"timestamp\": \"2021-02-20T00:00:00\""),
              std::string::npos);
    EXPECT_NE(json.find("\"small_sample\": true"), std::string::npos);
    EXPECT_NE(json.find("\"zone\": \"green\""), std::string::npos);
    EXPECT_NE(json.find("\"timestamp_index_name\": null"),
              std::string::npos);
    EXPECT_NE(json.find("\"timestamp_timezone\": null"),
              std::string::npos);
}

TEST(RiskReport, HistoricalReportProjectsWeibullParameters) {
    const auto result = fincore::walk_forward_var(report_fixture(300),
                                                  historical_spec());
    const auto report = fincore::build_risk_validation_report(result);

    EXPECT_EQ(report.status, fincore::walk_forward_status::ok);
    ASSERT_EQ(report.forecast_events.size(), 200U);
    ASSERT_EQ(report.refits.size(), 20U);
    EXPECT_EQ(report.forecast_events.front().date,
              fincore::utc_timestamp(2021, 4, 11));
    EXPECT_NEAR(report.forecast_events.front().forecast,
                -0.009674773367508039, 1e-12);
    EXPECT_NEAR(report.forecast_events.front().realized,
                0.007738906815578891, 1e-12);
    EXPECT_NEAR(report.forecast_events.back().forecast,
                -0.009982395371701296, 1e-12);

    EXPECT_EQ(report.diagnostics.method, "historical");
    EXPECT_EQ(report.diagnostics.quantile_method, "weibull");
    EXPECT_EQ(report.diagnostics.forecast_count, 200U);
    EXPECT_EQ(report.diagnostics.backtest_status, "fail");
    EXPECT_EQ(report.refits.front().n_observations, 100U);
    EXPECT_NEAR(report.refits.front().forecast, -0.009674773367508039,
                1e-12);

    EXPECT_EQ(report.backtest.observations, 200U);
    EXPECT_EQ(report.backtest.exceptions, 19U);
    EXPECT_NEAR(report.backtest.expected_exceptions, 10.000000000000009,
                1e-13);
    EXPECT_NEAR(report.backtest.kupiec_lr, 6.823658870683893, 1e-12);
    EXPECT_EQ(report.backtest.status, fincore::risk_backtest_status::fail);
    EXPECT_DOUBLE_EQ(report.backtest.diagnostics.at("small_sample"), 0.0);
    EXPECT_EQ(report.traffic_light_zone, "yellow");

    const std::string json = report.to_json();
    EXPECT_NE(json.find("\"quantile_method\": \"weibull\""),
              std::string::npos);
    EXPECT_NE(json.find("\"n_observations\": 100.0"), std::string::npos);
    EXPECT_EQ(json.find("\"mean\":"), std::string::npos);
    EXPECT_NE(json.find("\"small_sample\": false"), std::string::npos);
    EXPECT_NE(json.find("\"zone\": \"yellow\""), std::string::npos);
}

TEST(RiskReport, SmallReportMatchesPythonOracleAndJsonShape) {
    const auto result = fincore::walk_forward_var(report_fixture(12),
                                                  small_spec());
    const auto report = fincore::build_risk_validation_report(result);

    EXPECT_EQ(report.status, fincore::walk_forward_status::ok);
    ASSERT_EQ(report.forecast_events.size(), 7U);
    ASSERT_EQ(report.refits.size(), 4U);
    EXPECT_EQ(report.forecast_events.front().date,
              fincore::utc_timestamp(2021, 1, 6));
    EXPECT_NEAR(report.forecast_events.front().forecast,
                0.000513085814624476, 1e-12);
    EXPECT_NEAR(report.forecast_events.front().realized,
                -0.0035078322768961985, 1e-12);
    EXPECT_TRUE(report.forecast_events.front().exception);
    EXPECT_TRUE(report.forecast_events.front().refit);
    EXPECT_EQ(report.forecast_events.back().date,
              fincore::utc_timestamp(2021, 1, 12));
    EXPECT_NEAR(report.forecast_events.back().forecast,
                -0.012453071489010743, 1e-12);
    EXPECT_FALSE(report.forecast_events.back().exception);
    // The two fixtures disagree about the trailing event: 7 events at cadence 2
    // end on a refit boundary, while the 250-event fixture above ends mid-cadence,
    // so the pair pins that refits sit at cadence multiples counted from the first
    // forecast rather than at the end of the sample.
    EXPECT_TRUE(report.forecast_events.back().refit);

    EXPECT_NEAR(report.refits.front().forecast, 0.000513085814624476,
                1e-12);
    EXPECT_NEAR(report.refits.front().mean, 0.00565572986806186, 1e-12);
    EXPECT_NEAR(report.refits.front().standard_deviation,
                0.004012826476671656, 1e-12);
    EXPECT_EQ(report.refits.front().n_observations, 5U);

    EXPECT_EQ(report.backtest.observations, 7U);
    EXPECT_EQ(report.backtest.exceptions, 3U);
    EXPECT_NEAR(report.backtest.expected_exceptions, 0.6999999999999998,
                1e-15);
    EXPECT_NEAR(report.backtest.kupiec_lr, 5.0976812174202815, 1e-12);
    EXPECT_NEAR(report.backtest.kupiec_pvalue, 0.02395784976379568, 1e-12);
    EXPECT_NEAR(report.backtest.christoffersen_lr, 3.81908500976888, 1e-12);
    EXPECT_NEAR(report.backtest.christoffersen_pvalue, 0.050671902346990025,
                1e-12);
    EXPECT_EQ(report.traffic_light_zone, "yellow");
    EXPECT_NEAR(report.backtest.exception_rate, 0.42857142857142855, 1e-15);

    const std::string json = report.to_json();
    EXPECT_NE(json.find("\"n_observations\": 5.0"), std::string::npos);
    EXPECT_NE(json.find("\"refit\": true"), std::string::npos);
    EXPECT_NE(json.find("\"exception\": true"), std::string::npos);
}

TEST(RiskReport, UnsupportedReportJsonMatchesSharedSchema) {
    fincore::risk_model_spec spec;
    spec.distribution = "normal";
    spec.window = 50U;
    spec.forecast_target = fincore::risk_forecast_target::es;
    const auto result =
        fincore::walk_forward_var(report_fixture(300), spec);
    ASSERT_EQ(result.status, fincore::walk_forward_status::unsupported);
    const auto report = fincore::build_risk_validation_report(
        result, std::string("trade_date"));

    EXPECT_EQ(report.status, fincore::walk_forward_status::unsupported);
    EXPECT_TRUE(report.forecast_events.empty());
    EXPECT_TRUE(report.refits.empty());
    EXPECT_FALSE(report.has_backtest);
    EXPECT_EQ(report.diagnostics.reason, result.unsupported_reason);
    EXPECT_EQ(report.timestamp_index_name,
              std::optional<std::string>("trade_date"));

    const std::string expected =
        "{\n"
        "  \"backtest\": null,\n"
        "  \"diagnostics\": {\n"
        "    \"reason\": \"walk_forward_var currently supports "
        "forecast_target='var' only\"\n"
        "  },\n"
        "  \"disclosure\": \"" +
        disclosure_text() +
        "\",\n"
        "  \"forecast_events\": [],\n"
        "  \"inputs_digest\": \"" +
        result.inputs_digest +
        "\",\n"
        "  \"refits\": [],\n"
        "  \"schema_version\": 1,\n"
        "  \"specification\": {\n"
        "    \"confidence_level\": 0.99,\n"
        "    \"distribution\": \"normal\",\n"
        "    \"forecast_target\": \"es\",\n"
        "    \"horizon\": 1,\n"
        "    \"model_version\": \"1.0\",\n"
        "    \"refit_cadence\": 1,\n"
        "    \"sign_convention\": \"losses_negative\",\n"
        "    \"tail\": \"lower\",\n"
        "    \"window\": 50\n"
        "  },\n"
        "  \"status\": \"unsupported\",\n"
        "  \"timestamp_index_name\": \"trade_date\",\n"
        "  \"timestamp_timezone\": null\n"
        "}\n";
    EXPECT_EQ(report.to_json(), expected);
}

TEST(RiskReport, InsufficientReportProjectsObservationCount) {
    const auto result =
        fincore::walk_forward_var(report_fixture(50), normal_spec());
    ASSERT_EQ(result.status, fincore::walk_forward_status::insufficient_data);
    const auto report = fincore::build_risk_validation_report(result);

    EXPECT_TRUE(report.forecast_events.empty());
    EXPECT_TRUE(report.refits.empty());
    EXPECT_FALSE(report.has_backtest);
    EXPECT_EQ(report.diagnostics.n_observations, 50U);
    EXPECT_EQ(report.diagnostics.window, 50U);
    EXPECT_EQ(report.diagnostics.reason,
              "at least one realized observation after the estimation "
              "window is required");

    const std::string json = report.to_json();
    EXPECT_NE(json.find("\"n_observations\": 50"), std::string::npos);
    EXPECT_NE(json.find("\"backtest\": null"), std::string::npos);
    EXPECT_NE(json.find("\"forecast_events\": []"), std::string::npos);
    EXPECT_NE(json.find("\"refits\": []"), std::string::npos);
    EXPECT_EQ(json.find("fit_parameters"), std::string::npos);
}

TEST(RiskReport, BuildRejectsTamperedResults) {
    const auto good = fincore::walk_forward_var(report_fixture(300),
                                                normal_spec());
    ASSERT_EQ(good.status, fincore::walk_forward_status::ok);

    auto tampered = good;
    tampered.backtest.confidence_level = 0.95;
    EXPECT_THROW(fincore::build_risk_validation_report(tampered),
                 fincore::input_error);

    tampered = good;
    tampered.backtest.exceptions += 1U;
    EXPECT_THROW(fincore::build_risk_validation_report(tampered),
                 fincore::input_error);

    tampered = good;
    tampered.fits.clear();
    EXPECT_THROW(fincore::build_risk_validation_report(tampered),
                 fincore::input_error);

    tampered = good;
    tampered.refit_timestamps.pop_back();
    EXPECT_THROW(fincore::build_risk_validation_report(tampered),
                 fincore::input_error);

    tampered = good;
    tampered.forecast[3] += 0.001;
    EXPECT_THROW(fincore::build_risk_validation_report(tampered),
                 fincore::input_error);

    tampered = good;
    tampered.fits.front().mean += 1.0;
    EXPECT_THROW(fincore::build_risk_validation_report(tampered),
                 fincore::input_error);

    auto empty_path =
        fincore::walk_forward_var(report_fixture(50), normal_spec());
    ASSERT_EQ(empty_path.status,
              fincore::walk_forward_status::insufficient_data);
    empty_path.index.push_back(fincore::utc_timestamp(2021, 1, 1));
    empty_path.forecast.push_back(0.0);
    empty_path.realized.push_back(0.0);
    EXPECT_THROW(fincore::build_risk_validation_report(empty_path),
                 fincore::input_error);
}

TEST(RiskReport, JsonFormatsFloatsTimestampsAndEscapesText) {
    fincore::risk_validation_report report;
    report.status = fincore::walk_forward_status::ok;
    report.inputs_digest = std::string(64U, 'a');
    report.specification = normal_spec();
    const fincore::timestamp midnight = fincore::utc_timestamp(2021, 1, 1);
    report.forecast_events = {
        {midnight, 50.0, -0.0, false, false},
        {midnight + 123456789, 1e-5, 1e16, false, false},
        {midnight + 123456000, 1234567890123456.8, 0.0001, false, false},
        {midnight + 500000000, -2.5, 1.2345678901234566e-07, false, false},
        {midnight - 500000000, 0.0, -1.5e15, false, false},
    };
    report.timestamp_index_name =
        std::string("a\"b\\c\n\xe4\xb8\xad");

    const std::string json = report.to_json();
    EXPECT_NE(json.find("\"forecast\": 50.0"), std::string::npos);
    EXPECT_NE(json.find("\"realized\": -0.0"), std::string::npos);
    EXPECT_NE(json.find("\"forecast\": 1e-05"), std::string::npos);
    EXPECT_NE(json.find("\"realized\": 1e+16"), std::string::npos);
    EXPECT_NE(json.find("\"forecast\": 1234567890123456.8"),
              std::string::npos);
    EXPECT_NE(json.find("\"realized\": 0.0001"), std::string::npos);
    EXPECT_NE(json.find("\"forecast\": -2.5"), std::string::npos);
    EXPECT_NE(json.find("\"realized\": 1.2345678901234566e-07"),
              std::string::npos);
    EXPECT_NE(json.find("\"realized\": -1500000000000000.0"),
              std::string::npos);
    EXPECT_NE(json.find("\"timestamp\": \"2021-01-01T00:00:00.123456789\""),
              std::string::npos);
    EXPECT_NE(json.find("\"timestamp\": \"2021-01-01T00:00:00.123456\""),
              std::string::npos);
    EXPECT_NE(json.find("\"timestamp\": \"2021-01-01T00:00:00.500000\""),
              std::string::npos);
    EXPECT_NE(json.find("\"timestamp\": \"2020-12-31T23:59:59.500000\""),
              std::string::npos);
    EXPECT_NE(json.find("\"timestamp_index_name\": "
                        "\"a\\\"b\\\\c\\n\\u4e2d\""),
              std::string::npos);

    auto invalid = report;
    invalid.forecast_events.front().forecast =
        std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(invalid.to_json(), fincore::input_error);

    auto invalid_text = report;
    invalid_text.timestamp_index_name = std::string(1U, '\xff');
    EXPECT_THROW(invalid_text.to_json(), fincore::input_error);
}

TEST(RiskReport, OwnsSnapshotAcrossInputAndResultDestruction) {
    fincore::risk_validation_report report;
    fincore::risk_validation_report copied_report;
    std::string serialized_before;
    {
        auto source_returns = report_fixture(300);
        auto result = fincore::walk_forward_var(source_returns, normal_spec());
        ASSERT_EQ(result.status, fincore::walk_forward_status::ok);
        report = fincore::build_risk_validation_report(result);
        copied_report = report;
        serialized_before = report.to_json();

        source_returns.get_column<double>("returns")[0] += 1.0;
        EXPECT_EQ(report.to_json(), serialized_before);
        source_returns = report_fixture(300, 220, 0.25);
        EXPECT_EQ(report.to_json(), serialized_before);
        result.forecast.front() += 0.1;
        result.index.front() += 1;
        result.fits.front().mean += 1.0;
        result.inputs_digest.assign(64U, '0');
        EXPECT_EQ(report.to_json(), serialized_before);
    }

    EXPECT_EQ(report.to_json(), serialized_before);
    EXPECT_EQ(copied_report.to_json(), serialized_before);

    copied_report.forecast_events.front().forecast += 0.125;
    const std::string copied_after_mutation = copied_report.to_json();
    EXPECT_NE(copied_after_mutation, serialized_before);
    EXPECT_EQ(report.to_json(), serialized_before);

    report.refits.front().mean += 0.125;
    EXPECT_NE(report.to_json(), serialized_before);
    EXPECT_EQ(copied_report.to_json(), copied_after_mutation);
}

}  // namespace
