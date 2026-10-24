#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/error.hpp>
#include <fincore/runtime/operations_factor.hpp>
#include <fincore/runtime/runtime.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace {

constexpr std::int64_t day = 86'400'000'000'000LL;

fincore::dataframe empty_frame(std::vector<fincore::timestamp> index) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    return frame;
}

fincore::runtime_result run(const fincore::operation_catalog& catalog,
                            const std::string& id,
                            const fincore::runtime_inputs& inputs) {
    return fincore::run(id, inputs, {}, catalog);
}

fincore::dataframe brinson_returns(double tech, double health) {
    auto frame = empty_frame({1});
    frame.load_column<double>("tech", {tech},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("health", {health},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe brinson_weights(double tech, double health) {
    auto frame = empty_frame({1});
    frame.load_column<double>("tech", {tech},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("health", {health},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

// Three dates x three assets, quantile and one forward-return label.
fincore::dataframe prepared_tiny() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    auto frame = empty_frame(std::vector<fincore::timestamp>(9U));
    std::vector<fincore::timestamp> dates;
    std::vector<std::string> assets;
    std::vector<int> quantiles;
    std::vector<double> factors;
    std::vector<double> forwards;
    for (int date = 0; date < 3; ++date) {
        for (int asset = 1; asset <= 3; ++asset) {
            dates.push_back(start + date * day);
            assets.push_back(std::string(1, static_cast<char>('A' + asset)));
            quantiles.push_back(asset);
            factors.push_back(static_cast<double>(asset));
            forwards.push_back(0.01 * static_cast<double>(asset));
        }
    }
    frame.load_column<std::int64_t>("date", std::move(dates),
                                    hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<std::string>("asset", std::move(assets),
                                   hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<int>("quantile", std::move(quantiles),
                           hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("factor", std::move(factors),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("1D", std::move(forwards),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe optimization_fixture() {
    std::vector<fincore::timestamp> index;
    std::vector<std::vector<double>> columns(4U);
    for (int position = 0; position < 300; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        for (int asset = 0; asset < 4; ++asset) {
            columns[static_cast<std::size_t>(asset)].push_back(
                0.0004 * asset +
                0.01 * std::sin((position + 1) * (0.7 + 0.1 * asset)) +
                0.003 *
                    std::cos((position + 1) * (0.13 + 0.07 * asset)));
        }
    }
    auto frame = empty_frame(std::move(index));
    static const char* names[] = {"A", "B", "C", "D"};
    for (int asset = 0; asset < 4; ++asset) {
        frame.load_column<double>(
            names[asset], std::move(columns[static_cast<std::size_t>(asset)]),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return frame;
}

fincore::dataframe loadings_frame() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    auto frame = empty_frame({start, start + day, start, start + day});
    frame.load_column<std::string>("symbol", {"A", "A", "B", "B"},
                                   hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("MKT", {1.0, 1.0, 0.5, 0.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("SML", {0.5, 0.0, 1.0, 1.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

std::vector<std::string> expected_ids() {
    return {
        "attribution.brinson.brinson_attribution",
        "attribution.brinson.brinson_cumulative",
        "attribution.brinson.brinson_results",
        "attribution.fama_french.calculate_idiosyncratic_risk",
        "attribution.fama_french.fetch_ff_factors",
        "attribution.performance.align_and_warn",
        "attribution.performance.compute_exposures",
        "attribution.performance.compute_exposures_internal",
        "attribution.performance.create_perf_attrib_stats",
        "attribution.performance.cumulative_returns_less_costs",
        "attribution.performance.perf_attrib",
        "attribution.performance.perf_attrib_core",
        "attribution.style.analyze_performance_by_style",
        "attribution.style.calculate_regression_attribution",
        "attribution.style.calculate_style_tilts",
        "attribution.style.fetch_style_factors",
        "attribution.style.style_analysis",
        "data.snapshots.DataSnapshot.from_frame",
        "factor_analysis.analysis.analyze_factor",
        "factor_analysis.calendar.add_custom_calendar_timedelta",
        "factor_analysis.calendar.backshift_returns_series",
        "factor_analysis.calendar.diff_custom_calendar_timedeltas",
        "factor_analysis.calendar.get_forward_returns_columns",
        "factor_analysis.calendar.infer_trading_calendar",
        "factor_analysis.calendar.timedelta_strings_to_integers",
        "factor_analysis.calendar.timedelta_to_string",
        "factor_analysis.costs.apply_factor_costs",
        "factor_analysis.costs.estimate_factor_capacity",
        "factor_analysis.data.compute_forward_returns",
        "factor_analysis.data.prepare_factor_data",
        "factor_analysis.data.prepare_factor_data_by_horizon",
        "factor_analysis.data.prepare_factor_data_from_forward_returns",
        "factor_analysis.data.prepare_pit_factor_data",
        "factor_analysis.data.quantize_factor",
        "factor_analysis.inference.benjamini_hochberg",
        "factor_analysis.inference.factor_model_inference",
        "factor_analysis.inference.fama_macbeth",
        "factor_analysis.inference.ic_confidence_interval",
        "factor_analysis.inference.ic_mean",
        "factor_analysis.inference.ic_t_stat",
        "factor_analysis.inference.information_coefficient_inference",
        "factor_analysis.performance.average_cumulative_return_by_quantile",
        "factor_analysis.performance.common_start_returns",
        "factor_analysis.performance.compute_mean_returns_spread",
        "factor_analysis.performance.cumulative_returns",
        "factor_analysis.performance.factor_alpha_beta",
        "factor_analysis.performance.factor_information_coefficient",
        "factor_analysis.performance.factor_rank_autocorrelation",
        "factor_analysis.performance.factor_returns",
        "factor_analysis.performance.factor_weights",
        "factor_analysis.performance.mean_information_coefficient",
        "factor_analysis.performance.mean_return_by_quantile",
        "factor_analysis.performance.quantile_turnover",
        "factor_analysis.pit.materialize_pit_factor",
        "factor_analysis.pit.validate_pit_alignment",
        "factor_analysis.portfolio.build_factor_portfolio_inputs",
        "factor_analysis.portfolio.factor_cumulative_returns",
        "factor_analysis.portfolio.factor_positions",
        "factor_analysis.portfolio.positions",
        "optimization.frontier.efficient_frontier",
        "optimization.objectives.optimize",
        "optimization.risk_parity.risk_parity",
        "report.factor.build_factor_report"};
}

TEST(RuntimeOperationsFactor, DeclaresEveryMissingNativeOperation) {
    const auto operations = fincore::attribution_factor_operations();
    ASSERT_EQ(operations.size(), 63U);
    const auto expected = expected_ids();
    ASSERT_EQ(expected.size(), operations.size());
    const std::set<std::string> expected_set(expected.begin(), expected.end());
    std::set<std::string> ids;
    std::map<std::string, std::size_t> domain_counts;
    for (std::size_t position = 0U; position < operations.size();
         ++position) {
        const auto& spec = operations[position];
        EXPECT_EQ(expected_set.count(spec.operation_id), 1U)
            << spec.operation_id;
        EXPECT_TRUE(ids.insert(spec.operation_id).second)
            << spec.operation_id;
        EXPECT_EQ(spec.capability_id.rfind("fincore.", 0U), 0U);
        EXPECT_EQ(spec.domain,
                  spec.operation_id.substr(0U,
                                           spec.operation_id.find('.')));
        EXPECT_TRUE(spec.deterministic);
        EXPECT_EQ(spec.rng_policy, "none");
        EXPECT_TRUE(static_cast<bool>(spec.callable));
        ++domain_counts[spec.domain];
        const auto kernel = spec.provenance.find("kernel_symbol");
        const auto source = spec.provenance.find("source_operation_id");
        const auto schema = spec.provenance.find("input_schema");
        ASSERT_NE(kernel, spec.provenance.end()) << spec.operation_id;
        ASSERT_NE(source, spec.provenance.end()) << spec.operation_id;
        ASSERT_NE(schema, spec.provenance.end()) << spec.operation_id;
        EXPECT_FALSE(kernel->second.empty());
        EXPECT_EQ(source->second, spec.operation_id);
        EXPECT_EQ(schema->second.rfind("{\"required\":", 0U), 0U)
            << spec.operation_id;
        EXPECT_NE(schema->second.find("\"optional\":"),
                  std::string::npos);
        EXPECT_EQ(spec.implementation_fingerprint(), kernel->second);
    }
    EXPECT_EQ(ids.size(), 63U);
    EXPECT_EQ(domain_counts["attribution"], 17U);
    EXPECT_EQ(domain_counts["factor_analysis"], 41U);
    EXPECT_EQ(domain_counts["optimization"], 3U);
    EXPECT_EQ(domain_counts["report"], 1U);
    EXPECT_EQ(domain_counts["data"], 1U);
}

TEST(RuntimeOperationsFactor, ExternalProviderRoutesAreNotRegistered) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    EXPECT_THROW(static_cast<void>(catalog.resolve("data.providers.get_provider")),
                 fincore::operation_resolution_error);
    EXPECT_THROW(
        static_cast<void>(catalog.resolve("data.providers.fetch_price_data")),
        fincore::operation_resolution_error);
    EXPECT_THROW(static_cast<void>(
                     catalog.resolve("data.providers.fetch_multiple_prices")),
                 fincore::operation_resolution_error);
}

TEST(RuntimeOperationsFactor, CatalogAndResultDigestsAreDeterministic) {
    const auto first = fincore::operation_catalog(
        fincore::attribution_factor_operations());
    const auto second = fincore::operation_catalog(
        fincore::attribution_factor_operations());
    EXPECT_EQ(first.digest(), second.digest());

    fincore::dataframe ic;
    ic.load_index(std::vector<fincore::timestamp>{0, 1, 2});
    ic.load_column<double>("value", {0.1, 0.2, 0.3},
                           hmdf::nan_policy::dont_pad_with_nans);
    const fincore::runtime_inputs inputs{{"ic_series", ic}};
    const auto first_result = run(first, "factor_analysis.inference.ic_mean",
                                  inputs);
    const auto second_result = run(first, "factor_analysis.inference.ic_mean",
                                   inputs);
    EXPECT_EQ(fincore::runtime_value_digest(first_result.value),
              fincore::runtime_value_digest(second_result.value));
    const auto* mean = std::get_if<double>(&first_result.value.data);
    ASSERT_NE(mean, nullptr);
    EXPECT_DOUBLE_EQ(*mean, 0.2);
}

TEST(RuntimeOperationsFactor, BrinsonAdaptersUseTheNativeKernel) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    const auto portfolio_returns = brinson_returns(0.10, 0.05);
    const auto benchmark_returns = brinson_returns(0.08, 0.04);
    const auto portfolio_weights = brinson_weights(0.5, 0.3);
    const auto benchmark_weights = brinson_weights(0.4, 0.4);
    const fincore::runtime_inputs inputs{
        {"portfolio_returns", portfolio_returns},
        {"benchmark_returns", benchmark_returns},
        {"portfolio_weights", portfolio_weights},
        {"benchmark_weights", benchmark_weights}};

    const auto period =
        run(catalog, "attribution.brinson.brinson_attribution", inputs);
    const auto* frame = std::get_if<fincore::dataframe>(&period.value.data);
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->get_index().size(), 1U);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("allocation")[0], 0.004);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("selection")[0], 0.012);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("interaction")[0], 0.001);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("total")[0], 0.017);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("portfolio_return")[0], 0.065);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("benchmark_return")[0], 0.048);

    auto results_inputs = inputs;
    fincore::dataframe periods;
    periods.load_index(std::vector<fincore::timestamp>{0});
    periods.load_column<std::string>("value", {"P1"},
                                     hmdf::nan_policy::dont_pad_with_nans);
    results_inputs["periods"] = periods;
    const auto results = run(catalog, "attribution.brinson.brinson_results",
                             results_inputs);
    const auto* result_frame =
        std::get_if<fincore::dataframe>(&results.value.data);
    ASSERT_NE(result_frame, nullptr);
    ASSERT_EQ(result_frame->get_columns_info<std::string>().size(), 1U);
    EXPECT_EQ(
        result_frame->get_column<std::string>("period")[0], "P1");
}

TEST(RuntimeOperationsFactor, PerfAttributionExposureAdapterDispatches) {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    auto positions = empty_frame({start, start + day});
    positions.load_column<double>("A", {0.5, 0.6},
                                  hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>("B", {0.5, 0.4},
                                  hmdf::nan_policy::dont_pad_with_nans);
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    const auto result = run(
        catalog, "attribution.performance.compute_exposures_internal",
        {{"positions", positions},
         {"factor_loadings", loadings_frame()}});
    const auto* frame = std::get_if<fincore::dataframe>(&result.value.data);
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->get_index().size(), 2U);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("MKT")[0], 0.75);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("MKT")[1], 0.6);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("SML")[0], 0.75);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("SML")[1], 0.4);
}

TEST(RuntimeOperationsFactor, FactorPerformanceAdaptersDispatch) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    const auto weights = run(
        catalog, "factor_analysis.performance.factor_weights",
        {{"factor_data", prepared_tiny()}});
    const auto* frame = std::get_if<fincore::dataframe>(&weights.value.data);
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->get_index().size(), 9U);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("weight")[0], -0.5);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("weight")[1], 0.0);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("weight")[2], 0.5);

    const auto ic = run(
        catalog, "factor_analysis.performance.factor_information_coefficient",
        {{"factor_data", prepared_tiny()}});
    const auto* ic_frame = std::get_if<fincore::dataframe>(&ic.value.data);
    ASSERT_NE(ic_frame, nullptr);
    ASSERT_EQ(ic_frame->get_index().size(), 3U);
    EXPECT_EQ(ic_frame->get_column<std::string>("label")[0], "1D");
    EXPECT_DOUBLE_EQ(ic_frame->get_column<double>("value")[0], 1.0);

    const auto summary = run(
        catalog,
        "factor_analysis.performance.mean_information_coefficient",
        {{"factor_data", prepared_tiny()}});
    const auto* summary_frame =
        std::get_if<fincore::dataframe>(&summary.value.data);
    ASSERT_NE(summary_frame, nullptr);
    ASSERT_EQ(summary_frame->get_index().size(), 1U);
    EXPECT_DOUBLE_EQ(summary_frame->get_column<double>("mean")[0], 1.0);

    const auto cumulative = run(
        catalog, "factor_analysis.performance.cumulative_returns",
        {{"returns",
          [] {
              auto returns = empty_frame({0, 1, 2});
              returns.load_column<double>(
                  "returns", {0.1, -0.05, 0.02},
                  hmdf::nan_policy::dont_pad_with_nans);
              return returns;
          }()}});
    const auto* cumulative_frame =
        std::get_if<fincore::dataframe>(&cumulative.value.data);
    ASSERT_NE(cumulative_frame, nullptr);
    EXPECT_NEAR(cumulative_frame->get_column<double>("returns")[2], 1.0659,
                1e-15);
}

TEST(RuntimeOperationsFactor, CalendarAndInferenceAdaptersDispatch) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    const auto string_result = run(
        catalog, "factor_analysis.calendar.timedelta_to_string",
        {{"timedelta", day}});
    const auto* text = std::get_if<std::string>(&string_result.value.data);
    ASSERT_NE(text, nullptr);
    EXPECT_EQ(*text, "1D");

    auto columns = empty_frame({0, 1, 2});
    columns.load_column<std::string>("value", {"1D", "bad", "5D"},
                                     hmdf::nan_policy::dont_pad_with_nans);
    const auto parsed = run(
        catalog, "factor_analysis.calendar.get_forward_returns_columns",
        {{"columns", columns}});
    const auto* parsed_frame =
        std::get_if<fincore::dataframe>(&parsed.value.data);
    ASSERT_NE(parsed_frame, nullptr);
    ASSERT_EQ(parsed_frame->get_index().size(), 2U);
    EXPECT_EQ(parsed_frame->get_column<std::string>("column")[0], "1D");
    EXPECT_EQ(parsed_frame->get_column<std::string>("column")[1], "5D");

    const auto start = fincore::utc_timestamp(2022, 1, 3);
    auto dates = empty_frame({0, 1, 2});
    dates.load_column<std::int64_t>(
        "value", {start, start + day, start + 2 * day},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto calendar = run(
        catalog, "factor_analysis.calendar.infer_trading_calendar",
        {{"factor_idx", dates}, {"prices_idx", dates}});
    const auto* calendar_frame =
        std::get_if<fincore::dataframe>(&calendar.value.data);
    ASSERT_NE(calendar_frame, nullptr);
    EXPECT_GE(calendar_frame->get_index().size(), 7U);

    const auto lower = run(
        catalog, "factor_analysis.inference.ic_confidence_interval",
        {{"ic_series", [&] {
              auto values = empty_frame({0, 1, 2});
              values.load_column<double>("value", {0.1, 0.2, 0.3},
                                         hmdf::nan_policy::dont_pad_with_nans);
              return values;
          }()}});
    const auto* interval =
        std::get_if<fincore::dataframe>(&lower.value.data);
    ASSERT_NE(interval, nullptr);
    EXPECT_LT(interval->get_column<double>("lower")[0],
              interval->get_column<double>("upper")[0]);
}

TEST(RuntimeOperationsFactor, DataAndPortfolioAdaptersDispatch) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    auto factor = empty_frame({start, start, start + day, start + day});
    factor.load_column<std::string>("asset", {"A", "B", "A", "B"},
                                    hmdf::nan_policy::dont_pad_with_nans);
    factor.load_column<double>("factor", {1.0, 2.0, 1.0, 2.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    auto prices = empty_frame(
        {start, start + day, start + 2 * day, start + 3 * day});
    prices.load_column<double>("A", {100.0, 110.0, 120.0, 130.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    prices.load_column<double>("B", {50.0, 55.0, 60.0, 66.0},
                               hmdf::nan_policy::dont_pad_with_nans);
    const auto forward = run(
        catalog, "factor_analysis.data.compute_forward_returns",
        {{"factor", factor}, {"prices", prices},
         {"periods", [&] {
              auto periods = empty_frame({0});
              periods.load_column<int>("value", {1},
                                       hmdf::nan_policy::dont_pad_with_nans);
              return periods;
          }()}});
    const auto* forward_frame =
        std::get_if<fincore::dataframe>(&forward.value.data);
    ASSERT_NE(forward_frame, nullptr);
    ASSERT_EQ(forward_frame->get_index().size(), 4U);
    EXPECT_NEAR(forward_frame->get_column<double>("1D")[0], 0.1, 1e-15);
    EXPECT_NEAR(forward_frame->get_column<double>("1D")[1], 0.1, 1e-15);

    auto weights = empty_frame({start, start, start + day, start + day});
    weights.load_column<std::string>("asset", {"A", "B", "A", "B"},
                                     hmdf::nan_policy::dont_pad_with_nans);
    weights.load_column<double>("weight", {0.6, 0.4, 0.3, 0.7},
                                hmdf::nan_policy::dont_pad_with_nans);
    const auto positions = run(
        catalog, "factor_analysis.portfolio.positions",
        {{"weights", weights}, {"period", std::string{"1D"}}});
    const auto* position_frame =
        std::get_if<fincore::dataframe>(&positions.value.data);
    ASSERT_NE(position_frame, nullptr);
    EXPECT_FALSE(position_frame->get_index().empty());
    EXPECT_EQ(position_frame->get_column<std::string>("asset")[0], "A");
    EXPECT_DOUBLE_EQ(position_frame->get_column<double>("weight")[0], 0.6);
}

TEST(RuntimeOperationsFactor, ReportOptimizationAndSnapshotAdaptersDispatch) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    const auto report = run(
        catalog, "report.factor.build_factor_report",
        {{"factor_data", prepared_tiny()},
         {"title", std::string{"Tiny Factor"}}});
    const auto* document =
        std::get_if<fincore::report_document>(&report.value.data);
    ASSERT_NE(document, nullptr);
    EXPECT_EQ(document->domain, "factor");
    EXPECT_EQ(document->title, "Tiny Factor");
    EXPECT_FALSE(document->sections.empty());

    const auto risk = run(
        catalog, "optimization.risk_parity.risk_parity",
        {{"returns", optimization_fixture()}});
    const auto* risk_document =
        std::get_if<fincore::report_document>(&risk.value.data);
    ASSERT_NE(risk_document, nullptr);
    EXPECT_EQ(risk_document->domain, "optimization");
    ASSERT_FALSE(risk_document->sections.empty());
    ASSERT_FALSE(risk_document->sections[0].tables.empty());
    EXPECT_EQ(risk_document->sections[0].tables[0].rows.size(), 4U);

    auto snapshot_frame = empty_frame({fincore::utc_timestamp(2020, 1, 2)});
    snapshot_frame.load_column<double>(
        "Adj Close", {100.0}, hmdf::nan_policy::dont_pad_with_nans);
    const auto snapshot = run(
        catalog, "data.snapshots.DataSnapshot.from_frame",
        {{"frame", snapshot_frame},
         {"provider", std::string{"offline"}},
         {"requested_start", std::string{"2020-01-01"}},
         {"requested_end", std::string{"2020-12-31"}},
         {"as_of", std::string{"2020-12-31T23:59:59"}}});
    const auto* manifest = std::get_if<std::string>(&snapshot.value.data);
    ASSERT_NE(manifest, nullptr);
    EXPECT_NE(manifest->find("\"content_sha256\""), std::string::npos);
    EXPECT_NE(manifest->find("\"provider\": \"offline\""),
              std::string::npos);
    EXPECT_EQ(manifest->find("api_key"), std::string::npos);
}

TEST(RuntimeOperationsFactor, PitValidationAndSpreadAdaptersDispatch) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    auto points = empty_frame({0});
    points.load_column<std::int64_t>("as_of", {10},
                                     hmdf::nan_policy::dont_pad_with_nans);
    points.load_column<std::int64_t>("known_at", {11},
                                     hmdf::nan_policy::dont_pad_with_nans);
    points.load_column<std::int64_t>("effective_from", {12},
                                     hmdf::nan_policy::dont_pad_with_nans);
    points.load_column<double>("value", {1.5},
                               hmdf::nan_policy::dont_pad_with_nans);
    const auto validated = run(
        catalog, "factor_analysis.pit.validate_pit_alignment",
        {{"points", points}});
    const auto* valid = std::get_if<bool>(&validated.value.data);
    ASSERT_NE(valid, nullptr);
    EXPECT_TRUE(*valid);

    auto mean_returns = empty_frame({0, 1});
    mean_returns.load_column<int>("quantile", {1, 3},
                                  hmdf::nan_policy::dont_pad_with_nans);
    mean_returns.load_column<std::string>(
        "label", {"1D", "1D"}, hmdf::nan_policy::dont_pad_with_nans);
    mean_returns.load_column<double>("mean", {0.01, 0.03},
                                     hmdf::nan_policy::dont_pad_with_nans);
    mean_returns.load_column<double>(
        "standard_error", {0.001, 0.001},
        hmdf::nan_policy::dont_pad_with_nans);
    const auto spread = run(
        catalog, "factor_analysis.performance.compute_mean_returns_spread",
        {{"mean_returns", mean_returns},
         {"upper_quant", 3},
         {"lower_quant", 1}});
    const auto* spread_frame =
        std::get_if<fincore::dataframe>(&spread.value.data);
    ASSERT_NE(spread_frame, nullptr);
    ASSERT_EQ(spread_frame->get_index().size(), 1U);
    EXPECT_NEAR(spread_frame->get_column<double>("spread")[0], 0.02, 1e-15);
}

TEST(RuntimeOperationsFactor, InjectedProviderAdaptersDetachFrames) {
    const fincore::operation_catalog catalog(
        fincore::attribution_factor_operations());
    auto provider = empty_frame({0, 1});
    provider.load_column<double>("MKT", {0.01, 0.02},
                                 hmdf::nan_policy::dont_pad_with_nans);
    const auto ff = run(
        catalog, "attribution.fama_french.fetch_ff_factors",
        {{"start", std::string{"2020-01-01"}},
         {"end", std::string{"2020-01-31"}},
         {"provider", provider}});
    const auto* ff_frame = std::get_if<fincore::dataframe>(&ff.value.data);
    ASSERT_NE(ff_frame, nullptr);
    EXPECT_DOUBLE_EQ(ff_frame->get_column<double>("MKT")[1], 0.02);

    auto tickers = empty_frame({0, 1});
    tickers.load_column<std::string>("value", {"A", "B"},
                                     hmdf::nan_policy::dont_pad_with_nans);
    const auto style = run(
        catalog, "attribution.style.fetch_style_factors",
        {{"tickers", tickers}, {"provider", provider}});
    const auto* style_frame =
        std::get_if<fincore::dataframe>(&style.value.data);
    ASSERT_NE(style_frame, nullptr);
    EXPECT_EQ(style_frame->get_column<double>("MKT")[0], 0.01);
}

}  // namespace
