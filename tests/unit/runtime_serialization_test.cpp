#include <fincore/error.hpp>
#include <fincore/runtime.hpp>

#include <gtest/gtest.h>

#include <bit>
#include <cmath>
#include <cstdint>
#include <limits>
#include <map>
#include <string>
#include <vector>

namespace {

fincore::risk_backtest_result backtest_fixture() {
  fincore::risk_backtest_result result{};
  result.method = fincore::risk_backtest_method::es;
  result.confidence_level = 0.975;
  result.observations = 91U;
  result.exceptions = 3U;
  result.expected_exceptions = 2.275;
  result.exception_rate = 3.0 / 91.0;
  result.kupiec_lr = 0.125;
  result.kupiec_pvalue = 0.7;
  result.christoffersen_lr = 0.25;
  result.christoffersen_pvalue = 0.6;
  result.inputs_digest = "risk-inputs-v1";
  result.diagnostics = {{"tail_score", -0.125}, {"volatility", 0.25}};
  result.status = fincore::risk_backtest_status::experimental;
  return result;
}

fincore::risk_model_spec risk_spec_fixture() {
  fincore::risk_model_spec result;
  result.forecast_target = fincore::risk_forecast_target::pair;
  result.confidence_level = 0.975;
  result.horizon = 3U;
  result.distribution = "historical";
  result.tail = fincore::risk_tail::upper;
  result.sign_convention = "positive_losses";
  result.window = 31U;
  result.refit_cadence = 5U;
  result.model_version = "spec-β-2";
  return result;
}

fincore::walk_forward_var_result walk_forward_fixture() {
  fincore::walk_forward_var_result result{};
  result.spec = risk_spec_fixture();
  result.index = {std::numeric_limits<fincore::timestamp>::min(),
                  std::numeric_limits<fincore::timestamp>::max()};
  result.forecast = {std::numeric_limits<double>::quiet_NaN(), -0.0};
  result.realized = {std::numeric_limits<double>::infinity(),
                     -std::numeric_limits<double>::infinity()};
  result.refit_timestamps = {std::numeric_limits<fincore::timestamp>::min()};
  result.fits = {{std::numeric_limits<fincore::timestamp>::max(), -0.0,
                  std::numeric_limits<double>::infinity(), 0.25, 17U}};
  result.inputs_digest = "wf-inputs";
  result.status = fincore::walk_forward_status::unsupported;
  result.unsupported_reason = "unsupported test fixture";
  result.n_observations = 29U;
  result.backtest = backtest_fixture();
  result.has_backtest = true;
  return result;
}

fincore::dataframe dataframe_fixture() {
  fincore::dataframe result;
  result.load_index(std::vector<fincore::timestamp>{
      std::numeric_limits<fincore::timestamp>::min(), 42,
      std::numeric_limits<fincore::timestamp>::max()});
  result.load_column<bool>("bool", {true, false, true},
                           hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<char>("char", {'a', '\0', 'z'},
                           hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<signed char>("signed_char", {-2, 0, 2},
                                  hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<unsigned char>("unsigned_char", {0U, 1U, 255U},
                                    hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<short>("short", {-3, 0, 3},
                            hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<unsigned short>("unsigned_short", {0U, 3U, 65535U},
                                     hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<int>("int", {-4, 0, 4},
                          hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<unsigned int>("unsigned_int", {0U, 4U, 42U},
                                   hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<long>("long", {-5L, 0L, 5L},
                           hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<unsigned long>("unsigned_long", {0UL, 5UL, 42UL},
                                    hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<long long>(
      "long_long", {-6LL, 0LL, std::numeric_limits<long long>::min()},
      hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<unsigned long long>(
      "unsigned_long_long",
      {0ULL, 6ULL, std::numeric_limits<unsigned long long>::max()},
      hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<float>(
      "float", {1.25F, -0.0F, std::numeric_limits<float>::infinity()},
      hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("double",
                             {1.5, std::numeric_limits<double>::quiet_NaN(),
                              -std::numeric_limits<double>::infinity()},
                             hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<std::string>("string", {"alpha", "βeta", "quote\""},
                                  hmdf::nan_policy::dont_pad_with_nans);
  result.load_column<double>("short_column", {42.0},
                             hmdf::nan_policy::dont_pad_with_nans);
  return result;
}

fincore::report_document report_fixture() {
  fincore::report_document result;
  result.domain = "portfolio";
  result.title = "Portfolio β";
  result.metadata = {{"owner", "desk-A"}, {"empty", ""}};
  result.offline_assets = {{"theme.css", "body { color: #123; }"},
                           {"空.json", "{\"ok\":true}"}};
  fincore::report_section section;
  section.key = "summary";
  section.title = "Summary";
  fincore::report_metric metric;
  metric.key = "sharpe";
  metric.label = "Sharpe";
  metric.unit = "ratio";
  metric.number = -0.0;
  metric.boolean = false;
  metric.text = "present";
  section.metrics.push_back(metric);
  fincore::report_table table;
  table.key = "values";
  table.title = "Values";
  table.columns = {"number", "text", "missing"};
  table.rows = {{fincore::report_cell::number_cell(0.5),
                 fincore::report_cell::text_cell("text"),
                 fincore::report_cell::missing()}};
  section.tables.push_back(table);
  fincore::report_series series;
  series.key = "curve";
  series.title = "Curve";
  series.x_label = "date";
  series.y_label = "return";
  series.unit = "fraction";
  series.x = {-0.0, 1.0};
  series.y = {0.5, -1.0};
  section.series.push_back(series);
  section.notes = {"note"};
  section.units = {{"curve", "fraction"}};
  section.legends = {{"curve", "Portfolio"}};
  result.sections.push_back(section);
  return result;
}

fincore::risk_validation_report validation_fixture() {
  fincore::risk_validation_report result;
  result.status = fincore::walk_forward_status::insufficient_data;
  result.inputs_digest = "validation-inputs";
  result.specification = risk_spec_fixture();
  result.forecast_events = {{std::numeric_limits<fincore::timestamp>::min(),
                             std::numeric_limits<double>::quiet_NaN(), -0.0,
                             true, false}};
  result.refits = {
      {std::numeric_limits<fincore::timestamp>::max(), 1.0, -0.0, 0.25, 8U}};
  result.diagnostics.method = "historical";
  result.diagnostics.quantile_method = "linear";
  result.diagnostics.window = 20U;
  result.diagnostics.refit_cadence = 4U;
  result.diagnostics.forecast_count = 7U;
  result.diagnostics.backtest_status = "experimental";
  result.diagnostics.reason = "fixture";
  result.diagnostics.n_observations = 22U;
  result.has_backtest = true;
  result.backtest = backtest_fixture();
  result.traffic_light_zone = "amber";
  result.timestamp_index_name = std::nullopt;
  result.timestamp_timezone = "UTC";
  result.disclosure = "reference only";
  result.schema_version = fincore::risk_validation_report_schema_version;
  return result;
}

std::vector<fincore::runtime_result> all_value_fixtures() {
  std::vector<fincore::runtime_result> result;
  result.push_back({fincore::runtime_value{}, {{"kind", "null"}}});
  result.push_back({fincore::runtime_value(true), {{"kind", "bool"}}});
  result.push_back(
      {fincore::runtime_value(std::numeric_limits<std::int64_t>::min()),
       {{"kind", "int64-min"}}});
  result.push_back(
      {fincore::runtime_value(std::numeric_limits<std::int64_t>::max()),
       {{"kind", "int64-max"}}});
  result.push_back({fincore::runtime_value(-0.0), {{"kind", "negative-zero"}}});
  result.push_back(
      {fincore::runtime_value(std::numeric_limits<double>::quiet_NaN()),
       {{"kind", "nan"}}});
  result.push_back({fincore::runtime_value(std::bit_cast<double>(
                        std::uint64_t{0x7ff8000000001234ULL})),
                    {{"kind", "nan-payload"}}});
  result.push_back(
      {fincore::runtime_value(std::numeric_limits<double>::infinity()),
       {{"kind", "positive-infinity"}}});
  result.push_back(
      {fincore::runtime_value(-std::numeric_limits<double>::infinity()),
       {{"kind", "negative-infinity"}}});
  result.push_back(
      {fincore::runtime_value(std::string("UTF-8 β and \"quotes\"")),
       {{"kind", "string"}}});
  result.push_back(
      {fincore::runtime_value(dataframe_fixture()), {{"kind", "dataframe"}}});
  result.push_back({fincore::runtime_value(fincore::dataframe{}),
                    {{"kind", "empty-dataframe"}}});
  result.push_back({fincore::runtime_value(report_fixture()),
                    {{"kind", "report_document"}}});
  result.push_back({fincore::runtime_value(risk_spec_fixture()),
                    {{"kind", "risk_model_spec"}}});
  result.push_back({fincore::runtime_value(walk_forward_fixture()),
                    {{"kind", "walk_forward_var_result"}}});
  result.push_back({fincore::runtime_value(validation_fixture()),
                    {{"kind", "risk_validation_report"}}});
  result.push_back(
      {fincore::runtime_value(std::vector<fincore::portfolio_transaction>{
           {std::numeric_limits<fincore::timestamp>::min(), "β", -0.0,
            std::numeric_limits<double>::infinity()},
           {std::numeric_limits<fincore::timestamp>::max(), "A", 2.0, 3.0}}),
       {{"kind", "transactions"}}});
  return result;
}

TEST(RuntimeSerialization,
     RoundTripsEveryRuntimeValueAlternativeDeterministically) {
  for (const auto &result : all_value_fixtures()) {
    const auto encoded = fincore::to_json(result);
    const auto decoded = fincore::from_json(encoded);
    EXPECT_EQ(fincore::to_json(decoded), encoded);
    EXPECT_EQ(fincore::semantic_digest(decoded),
              fincore::semantic_digest(result));
    EXPECT_EQ(decoded.value.data.index(), result.value.data.index());
  }
}

TEST(RuntimeSerialization, PreservesSignedZeroNonFiniteAndExactIntegers) {
  for (const auto &result : all_value_fixtures()) {
    const auto decoded = fincore::from_json(fincore::to_json(result));
    // Bit comparison is the point here: value equality would accept 0.0 for -0.0
    // and would fail a NaN against itself.
    if (const auto *original = std::get_if<double>(&result.value.data)) {
      const auto actual = std::get<double>(decoded.value.data);
      EXPECT_EQ(std::bit_cast<std::uint64_t>(actual),
                std::bit_cast<std::uint64_t>(*original));
    }
    if (const auto *original = std::get_if<std::int64_t>(&result.value.data)) {
      EXPECT_EQ(std::get<std::int64_t>(decoded.value.data), *original);
    }
  }
  const auto result = fincore::from_json(
      R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"int64","data":"9223372036854775807"},"metadata":{}})");
  EXPECT_EQ(std::get<std::int64_t>(result.value.data),
            std::numeric_limits<std::int64_t>::max());
}

TEST(RuntimeSerialization,
     PayloadAndMetadataCopiesAreOwningAndMergePredictably) {
  fincore::runtime_result original{fincore::runtime_value(dataframe_fixture()),
                                   {{"keep", "original"}, {"replace", "old"}}};
  const auto payload = fincore::to_payload(original);
  auto from_payload = fincore::from_payload(payload);
  auto merged = fincore::with_metadata(
      original, {{"replace", "new"}, {"added", "value"}});
  EXPECT_EQ(merged.metadata.at("keep"), "original");
  EXPECT_EQ(merged.metadata.at("replace"), "new");
  EXPECT_EQ(merged.metadata.at("added"), "value");
  EXPECT_EQ(original.metadata.at("replace"), "old");

  auto &copied_frame = std::get<fincore::dataframe>(from_payload.value.data);
  copied_frame.get_column<double>("double")[0] = 99.0;
  auto &merged_frame = std::get<fincore::dataframe>(merged.value.data);
  merged_frame.get_column<double>("double")[1] = 88.0;
  const auto &original_frame =
      std::get<fincore::dataframe>(original.value.data);
  EXPECT_EQ(original_frame.get_column<double>("double")[0], 1.5);
  EXPECT_TRUE(std::isnan(original_frame.get_column<double>("double")[1]));
}

TEST(RuntimeSerialization, DigestCoversValueAndMetadataIncludingPresence) {
  fincore::runtime_result base{fincore::runtime_value(std::int64_t{7}), {}};
  auto changed_value = base;
  changed_value.value = fincore::runtime_value(std::int64_t{8});
  auto changed_metadata = base;
  changed_metadata.metadata["source"] = "unit";
  auto present_empty = base;
  present_empty.metadata["source"] = "";
  EXPECT_NE(fincore::semantic_digest(base),
            fincore::semantic_digest(changed_value));
  EXPECT_NE(fincore::semantic_digest(base),
            fincore::semantic_digest(changed_metadata));
  EXPECT_NE(fincore::semantic_digest(changed_metadata),
            fincore::semantic_digest(present_empty));
}

TEST(RuntimeSerialization,
     DigestIgnoresVolatileMetadataButJsonRetainsEveryField) {
  fincore::runtime_result first{fincore::runtime_value(std::int64_t{7}),
                                {{"duration_ns", "10"},
                                 {"finished_ns", "20"},
                                 {"run_id", "first"},
                                 {"started_ns", "5"},
                                 {"temporary_path", "/tmp/one"},
                                 {"workspace_path", "/work/one"},
                                 {"report_path", "/reports/one"},
                                 {"source", "unit"}}};
  auto second = first;
  second.metadata["duration_ns"] = "999";
  second.metadata["finished_ns"] = "200";
  second.metadata["run_id"] = "second";
  second.metadata["started_ns"] = "50";
  second.metadata["temporary_path"] = "/tmp/two";
  second.metadata["workspace_path"] = "/work/two";
  second.metadata["report_path"] = "/reports/two";
  second.metadata["cache_path"] = "/cache/two";

  EXPECT_EQ(fincore::semantic_digest(first), fincore::semantic_digest(second));
  const auto encoded = fincore::to_json(second);
  EXPECT_NE(encoded, fincore::to_json(first));
  EXPECT_EQ(fincore::from_json(encoded).metadata, second.metadata);

  second.metadata["source"] = "changed";
  EXPECT_NE(fincore::semantic_digest(first), fincore::semantic_digest(second));
}

TEST(RuntimeSerialization, FromPayloadValidatesNestedSupportedValueBoundary) {
  fincore::report_document invalid_report;
  invalid_report.schema_version = 1;
  fincore::runtime_result_payload payload{
      fincore::runtime_result_schema_version,
      fincore::runtime_value(std::move(invalid_report)),
      {}};

  EXPECT_THROW(fincore::from_payload(payload), fincore::input_error);
}

TEST(RuntimeSerialization,
     RejectsReportModelSemanticInvalidityAtEveryBoundary) {
  auto invalid_reference = report_fixture();
  invalid_reference.sections.front().units["unknown"] = "fraction";
  const fincore::runtime_result invalid_reference_result{
      fincore::runtime_value(invalid_reference), {}};
  EXPECT_THROW(fincore::to_json(invalid_reference_result),
               fincore::input_error);
  EXPECT_THROW(
      fincore::from_payload(fincore::to_payload(invalid_reference_result)),
      fincore::input_error);

  auto valid_json =
      fincore::to_json({fincore::runtime_value(report_fixture()), {}});
  // Each defect is injected into the serializer's own output, so these searches
  // rely on its key order and compact spelling rather than on a schema.
  const auto units = valid_json.find("\"units\":{\"curve\":\"fraction\"}");
  ASSERT_NE(units, std::string::npos);
  valid_json.replace(units,
                     std::string("\"units\":{\"curve\":\"fraction\"}").size(),
                     "\"units\":{\"unknown\":\"fraction\"}");
  EXPECT_THROW(fincore::from_json(valid_json), fincore::input_error);

  auto invalid_legend = report_fixture();
  invalid_legend.sections.front().legends["unknown"] = "Unknown";
  const fincore::runtime_result invalid_legend_result{
      fincore::runtime_value(invalid_legend), {}};
  EXPECT_THROW(fincore::to_json(invalid_legend_result), fincore::input_error);
  EXPECT_THROW(
      fincore::from_payload(fincore::to_payload(invalid_legend_result)),
      fincore::input_error);
  auto invalid_legend_json =
      fincore::to_json({fincore::runtime_value(report_fixture()), {}});
  const auto legends =
      invalid_legend_json.find("\"legends\":{\"curve\":\"Portfolio\"}");
  ASSERT_NE(legends, std::string::npos);
  invalid_legend_json.replace(
      legends, std::string("\"legends\":{\"curve\":\"Portfolio\"}").size(),
      "\"legends\":{\"unknown\":\"Unknown\"}");
  EXPECT_THROW(fincore::from_json(invalid_legend_json), fincore::input_error);

  auto duplicate_sections = report_fixture();
  duplicate_sections.sections.push_back(duplicate_sections.sections.front());
  const fincore::runtime_result duplicate_result{
      fincore::runtime_value(duplicate_sections), {}};
  EXPECT_THROW(fincore::to_json(duplicate_result), fincore::input_error);
  EXPECT_THROW(fincore::from_payload(fincore::to_payload(duplicate_result)),
               fincore::input_error);

  auto distinct_sections = report_fixture();
  auto second_section = distinct_sections.sections.front();
  second_section.key = "details";
  distinct_sections.sections.push_back(second_section);
  auto duplicate_json =
      fincore::to_json({fincore::runtime_value(distinct_sections), {}});
  const auto second_key = duplicate_json.find("\"key\":\"details\"");
  ASSERT_NE(second_key, std::string::npos);
  duplicate_json.replace(second_key, std::string("\"key\":\"details\"").size(),
                         "\"key\":\"summary\"");
  EXPECT_THROW(fincore::from_json(duplicate_json), fincore::input_error);
}

TEST(RuntimeSerialization, RejectsMalformedUnknownAndInconsistentPayloads) {
  EXPECT_THROW(fincore::from_json(""), fincore::input_error);
  EXPECT_THROW(fincore::from_json("{} trailing"), fincore::input_error);
  EXPECT_THROW(fincore::from_json("{\"a\":1,\"a\":2}"), fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":2,"value":{"type":"null","data":null},"metadata":{}})"),
      fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"unknown","data":null},"metadata":{}})"),
      fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"int64","data":"9223372036854775808"},"metadata":{}})"),
      fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"double","data":{"$f64":"not-a-number!!!!!"}},"metadata":{}})"),
      fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"dataframe","data":{"index":["1"],"columns":[{"name":"x","type":"mystery","values":[1]}]}},"metadata":{}})"),
      fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"dataframe","data":{"index":["1"],"columns":[{"name":"x","type":"double","values":[{"$f64":"3ff0000000000000"},{"$f64":"4000000000000000"}]}]}},"metadata":{}})"),
      fincore::input_error);
  EXPECT_THROW(
      fincore::from_json(
          R"({"schema":"fincore.runtime_result","schema_version":1,"value":{"type":"dataframe","data":{"index":["1"],"columns":[{"name":"INDEX","type":"int","values":["1"]}]}},"metadata":{}})"),
      fincore::input_error);
  auto old_report_schema =
      fincore::to_json({fincore::runtime_value(report_fixture()), {}});
  const auto nested_version = old_report_schema.find("\"schema_version\":2");
  ASSERT_NE(nested_version, std::string::npos);
  old_report_schema.replace(nested_version,
                            std::string("\"schema_version\":2").size(),
                            "\"schema_version\":1");
  EXPECT_THROW(fincore::from_json(old_report_schema), fincore::input_error);
}

} // namespace
