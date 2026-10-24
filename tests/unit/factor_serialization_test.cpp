#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/research.hpp>
#include <fincore/factor/serialization.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <map>
#include <string>
#include <vector>

namespace {

fincore::factor_analysis_model tiny_model() {
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const char* assets[] = {"A", "B", "C"};
    for (int index = 0; index < 3; ++index) {
        for (int asset = 0; asset < 3; ++asset) {
            fincore::factor_row row;
            row.date = start + index * day;
            row.asset = assets[asset];
            row.factor = static_cast<double>(asset + 1);
            row.quantile = asset + 1;
            row.forward_returns = {0.01 * (asset + 1) + 0.001 * index};
            data.rows.push_back(row);
        }
    }
    data.loss_report.input_count = 9U;
    data.loss_report.forward_returns_count = 8U;
    fincore::factor_analysis_config config;
    config.include_portfolio_inputs = false;
    return fincore::analyze_factor(data, config);
}

bool is_hex_digest(const std::string& value) {
    if (value.size() != 64U) {
        return false;
    }
    for (const char character : value) {
        if (!((character >= '0' && character <= '9') ||
              (character >= 'a' && character <= 'f'))) {
            return false;
        }
    }
    return true;
}

TEST(FactorSerialization, ModelSerializationRoundTripsAndIsDeterministic) {
    const auto model = tiny_model();
    const std::string first = fincore::to_serializable(model);
    const std::string second = fincore::to_serializable(model);
    EXPECT_EQ(first, second);
    EXPECT_EQ(first, model.to_serializable());

    const auto value = fincore::parse_json_value(first);
    ASSERT_TRUE(value.is_object());
    EXPECT_EQ(value.at("schema").as_string(),
              "fincore-factor-analysis-native-v1");
    EXPECT_EQ(value.at("result_fingerprint").as_string(),
              model.result_fingerprint);
    EXPECT_EQ(value.at("fingerprint_version").as_integer(),
              static_cast<std::int64_t>(model.result_fingerprint_version));

    const auto envelope = fincore::deserialize_factor_model(first);
    EXPECT_EQ(envelope.schema, "fincore-factor-analysis-native-v1");
    EXPECT_EQ(envelope.result_fingerprint, model.result_fingerprint);
    EXPECT_EQ(envelope.forward_periods,
              (std::vector<std::string>{"1D"}));
    EXPECT_EQ(envelope.loss_report.input_count, 9U);
    EXPECT_EQ(envelope.loss_report.forward_returns_count, 8U);
    EXPECT_FALSE(envelope.config.include_portfolio_inputs);

    EXPECT_THROW(fincore::parse_json_value("{"), fincore::input_error);
    EXPECT_THROW(fincore::deserialize_factor_model("{}"),
                 fincore::input_error);
    EXPECT_THROW(fincore::deserialize_factor_model(""),
                 fincore::input_error);
}

TEST(FactorSerialization, FingerprintIsStableAndSensitive) {
    const auto model = tiny_model();
    const std::string fingerprint = fincore::fingerprint_value(model);
    EXPECT_TRUE(is_hex_digest(fingerprint));
    EXPECT_EQ(fingerprint, fincore::fingerprint_value(model));

    fincore::factor_loss_report changed = model.factor_data.loss_report;
    changed.input_count += 1U;
    EXPECT_NE(fincore::fingerprint_value(model),
              fincore::fingerprint_value(changed));

    fincore::factor_value value;
    value = fincore::factor_value::object_type{{"a", 1}, {"b", "text"}};
    const std::string value_fingerprint = fincore::fingerprint_value(value);
    EXPECT_TRUE(is_hex_digest(value_fingerprint));
    EXPECT_EQ(value_fingerprint, fincore::fingerprint_value(value));
    value = fincore::factor_value::object_type{{"a", 1}, {"b", "other"}};
    EXPECT_NE(value_fingerprint, fincore::fingerprint_value(value));
}

TEST(FactorSerialization, FrozenMappingIsImmutableAndOwnsItsEntries) {
    std::map<std::string, fincore::factor_value> source{
        {"b", fincore::factor_value{2}},
        {"a", fincore::factor_value{"one"}}};
    const fincore::frozen_mapping<fincore::factor_value> frozen(source);
    source["a"] = fincore::factor_value{"mutated"};
    source["c"] = fincore::factor_value{3};

    EXPECT_EQ(frozen.size(), 2U);
    EXPECT_FALSE(frozen.contains("c"));
    EXPECT_EQ(frozen.at("a").as_string(), "one");
    EXPECT_EQ(frozen.find("c"), nullptr);
    const fincore::factor_value* b = frozen.find("b");
    ASSERT_NE(b, nullptr);
    EXPECT_EQ(b->as_integer(), 2);
    std::vector<std::string> keys;
    for (const auto& [key, value] : frozen) {
        keys.push_back(key);
        (void)value;
    }
    EXPECT_EQ(keys, (std::vector<std::string>{"a", "b"}));

    const fincore::frozen_mapping<fincore::factor_value> copy = frozen;
    EXPECT_TRUE(copy == frozen);
}

TEST(FactorSerialization, SnapshotOwnsNativeDataframe) {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{
        fincore::utc_timestamp(2022, 1, 3),
        fincore::utc_timestamp(2022, 1, 4)});
    frame.load_column<double>("value", {1.0, 2.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    const auto snapshot = fincore::snapshot_pandas(frame);
    EXPECT_TRUE(is_hex_digest(snapshot.content_sha256()));
    EXPECT_EQ(snapshot.content_sha256(), fincore::snapshot_pandas(frame).content_sha256());

    auto& column = frame.get_column<double>("value");
    column[0] = 99.0;
    EXPECT_EQ(snapshot.data().get_column<double>("value")[0], 1.0);
    EXPECT_EQ(snapshot.copy().get_column<double>("value")[1], 2.0);
}

TEST(FactorSerialization, NormalizesResearchEvaluationSpec) {
    fincore::research_evaluation_input input;
    input.targets = std::vector<std::string>{"A", "B"};
    input.start = fincore::utc_timestamp(2021, 1, 4);
    input.end = fincore::utc_timestamp(2021, 3, 4);
    const auto spec = fincore::normalize_research_evaluation_spec(input);
    EXPECT_EQ(spec.targets, (std::vector<std::string>{"A", "B"}));
    EXPECT_EQ(spec.periods, (std::vector<int>{1, 5, 20}));
    EXPECT_EQ(spec.quantiles, 5);
    EXPECT_EQ(spec.min_assets, 10);
    EXPECT_EQ(spec.min_quantile_assets, 1);
    EXPECT_EQ(spec.label, "next_open_to_open");
    EXPECT_EQ(spec.preprocess, "cross_section_rank");
    EXPECT_EQ(spec.max_loss, 1.0);
    EXPECT_EQ(spec.source_timezone, "UTC");
    EXPECT_EQ(fincore::EVALUATION_CONFIG_SCHEMA_VERSION, 1);

    const fincore::research_spec execution = fincore::to_research_spec(spec);
    EXPECT_EQ(execution.start, spec.start);
    EXPECT_EQ(execution.label, spec.label);
    EXPECT_EQ(execution.min_assets, 10);

    auto bad = input;
    bad.start = input.end;
    bad.end = input.start;
    EXPECT_THROW(fincore::normalize_research_evaluation_spec(bad),
                 fincore::research_config_error);
    bad = input;
    bad.unknown_fields = {"mystery"};
    EXPECT_THROW(fincore::normalize_research_evaluation_spec(bad),
                 fincore::research_config_error);
    bad = input;
    bad.label = std::string{"unknown"};
    EXPECT_THROW(fincore::normalize_research_evaluation_spec(bad),
                 fincore::research_config_error);
    bad = input;
    bad.preprocess = std::string{"unknown"};
    EXPECT_THROW(fincore::normalize_research_evaluation_spec(bad),
                 fincore::research_config_error);
    bad = input;
    bad.direction_overrides = std::map<std::string, int>{{"A", 2}};
    EXPECT_THROW(fincore::normalize_research_evaluation_spec(bad),
                 fincore::research_config_error);
    bad = input;
    bad.targets = std::vector<std::string>{"A", "A"};
    EXPECT_THROW(fincore::normalize_research_evaluation_spec(bad),
                 fincore::research_config_error);

    try {
        fincore::normalize_research_evaluation_spec(bad);
        FAIL() << "expected a config error";
    } catch (const fincore::fincore_error& error) {
        EXPECT_EQ(error.category(), "ConfigError");
        EXPECT_EQ(error.code(), "spec_field_duplicate");
        EXPECT_EQ(error.report().at("field"), "targets");
    }
}

TEST(FactorSerialization, ResearchTablesAndSerialization) {
    fincore::research_result result;
    result.status = "OK";
    result.manifest = {{"evaluation_id", "abc"}};
    fincore::research_signal_row signal;
    signal.date = fincore::utc_timestamp(2021, 1, 4);
    signal.asset = "A";
    signal.factor = "momentum";
    signal.direction = 1;
    signal.value = 0.5;
    signal.processed_value = 0.4;
    signal.preprocess = "cross_section_rank";
    result.signals.push_back(signal);
    fincore::research_label_row label;
    label.date = signal.date;
    label.asset = "A";
    label.period = "1D";
    label.value = 0.01;
    result.labels.push_back(label);
    result.information_coefficient.dates = {signal.date};
    result.information_coefficient.labels = {"1D"};
    result.information_coefficient.values = {0.25};
    result.information_coefficient_summary.labels = {"1D"};
    result.information_coefficient_summary.means = {0.25};
    result.spread = {0.02};
    result.coverage = {5U};
    result.diagnostics = {"ok"};

    const auto signals = result.table("signals");
    EXPECT_EQ(signals.name, "signals");
    ASSERT_EQ(signals.columns.size(), 8U);
    ASSERT_EQ(signals.rows.size(), 1U);
    EXPECT_EQ(signals.rows[0][1].as_string(), "A");
    EXPECT_EQ(signals.rows[0][2].as_string(), "momentum");

    const auto labels = result.table("labels");
    ASSERT_EQ(labels.rows.size(), 1U);
    EXPECT_NEAR(labels.rows[0][3].as_number(), 0.01, 1e-12);

    const auto ic = result.table("ic");
    ASSERT_EQ(ic.rows.size(), 1U);
    EXPECT_EQ(ic.rows[0][2].as_string(), "1D");
    EXPECT_NEAR(ic.rows[0][3].as_number(), 0.25, 1e-12);

    EXPECT_EQ(result.table("diagnostics").rows[0][0].as_string(), "ok");
    EXPECT_EQ(result.table("correlation").columns[0], "left");
    EXPECT_THROW(result.table("nope"), fincore::research_dataset_error);

    const std::string json = result.to_serializable();
    const auto value = fincore::parse_json_value(json);
    ASSERT_TRUE(value.is_object());
    EXPECT_EQ(value.at("status").as_string(), "OK");
    EXPECT_EQ(value.at("manifest").at("evaluation_id").as_string(), "abc");
    EXPECT_EQ(value.at("tables").as_object().size(), 12U);
    EXPECT_EQ(value.at("columns").as_object().size(), 12U);
    EXPECT_TRUE(is_hex_digest(fincore::fingerprint_value(result)));
    EXPECT_EQ(fincore::fingerprint_value(result),
              fincore::fingerprint_value(result));
}

TEST(FactorSerialization, ResearchErrorsUseStructuredCategories) {
    try {
        throw fincore::research_dependency_error("missing scipy");
    } catch (const fincore::fincore_error& error) {
        EXPECT_EQ(error.category(), "DependencyError");
        EXPECT_EQ(error.code(), "missing_dependency");
        EXPECT_STREQ(error.what(), "missing scipy");
    }
    try {
        throw fincore::research_data_quality_error(
            "bad data", "integrity", {{"field", "prices"}});
    } catch (const fincore::research_error& error) {
        EXPECT_EQ(error.category(), "DataQualityError");
        EXPECT_EQ(error.code(), "integrity");
        EXPECT_EQ(error.report().at("field"), "prices");
    }
    try {
        throw fincore::research_factor_computation_error("failed");
    } catch (const fincore::research_error& error) {
        EXPECT_EQ(error.category(), "FactorComputationError");
        EXPECT_EQ(error.code(), "factor_computation_failed");
    }
}

}  // namespace
