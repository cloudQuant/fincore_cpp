#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/workflows.hpp>

#include <gtest/gtest.h>

#include <cstddef>
#include <string>
#include <vector>

namespace {

fincore::prepared_factor_data tiny_data() {
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
            row.forward_returns = {0.01 * (asset + 1)};
            data.rows.push_back(row);
        }
    }
    return data;
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

}  // namespace

TEST(FactorWorkflows, DescribesPinnedWorkflowPlans) {
    const auto& workflows = fincore::factor_workflows();
    ASSERT_EQ(workflows.size(), 7U);
    EXPECT_EQ(workflows[0].workflow_id, "summary");
    EXPECT_EQ(workflows[0].model_operation_id,
              "factor_analysis.analysis.analyze_factor");
    EXPECT_EQ(workflows[0].sections,
              (std::vector<std::string>{"returns", "information", "turnover"}));
    EXPECT_FALSE(workflows[0].renderer.has_value());
    EXPECT_EQ(workflows[3].workflow_id, "turnover");
    EXPECT_EQ(workflows[3].sections, (std::vector<std::string>{"turnover"}));
    EXPECT_EQ(workflows[5].workflow_id, "event_returns");
    EXPECT_EQ(workflows[6].workflow_id, "event_study");
    EXPECT_EQ(workflows[6].sections,
              (std::vector<std::string>{"event_distribution", "event_returns",
                                        "returns"}));
    for (const auto& workflow : workflows) {
        EXPECT_NE(workflow.build, nullptr);
        EXPECT_EQ(workflow.model_operation_id,
                  "factor_analysis.analysis.analyze_factor");
    }
    EXPECT_EQ(fincore::find_factor_workflow("full"), &workflows[4]);
    EXPECT_EQ(fincore::find_factor_workflow("missing"), nullptr);
}

TEST(FactorWorkflows, CallableMatchesTheCanonicalAnalysis) {
    const auto data = tiny_data();
    const fincore::factor_analysis_config config;
    const auto workflow = fincore::find_factor_workflow("summary");
    ASSERT_NE(workflow, nullptr);
    const auto from_workflow = workflow->build(data, config);
    const auto direct = fincore::analyze_factor(data, config);
    EXPECT_EQ(from_workflow.result_fingerprint, direct.result_fingerprint);
    EXPECT_EQ(from_workflow.information_coefficient.labels,
              direct.information_coefficient.labels);
}

TEST(FactorWorkflows, ExposesStableOperationDescriptors) {
    const auto& operations = fincore::factor_operation_descriptors();
    ASSERT_EQ(operations.size(), 41U);
    EXPECT_EQ(operations.front().operation_id,
              "factor_analysis.analysis.analyze_factor");
    EXPECT_EQ(operations.back().operation_id,
              "factor_analysis.portfolio.positions");
    EXPECT_EQ(operations.front().capability_id,
              operations.front().operation_id);
    EXPECT_EQ(operations.front().domain, "factor_analysis");
    EXPECT_EQ(operations.front().provenance.at("owner"), "factor_analysis");
    EXPECT_EQ(operations.front().provenance.at("kernel_module"),
              "fincore.factor_analysis.analysis");
    EXPECT_EQ(operations[3].operation_id,
              "factor_analysis.calendar.diff_custom_calendar_timedeltas");
    EXPECT_EQ(operations[3].provenance.at("kernel_module"),
              "fincore.factor_analysis.calendar");

    const std::string digest = fincore::factor_operations_digest();
    EXPECT_TRUE(is_hex_digest(digest));
    EXPECT_EQ(digest, fincore::factor_operations_digest());
}

TEST(FactorWorkflows, ComputesLegacyForwardReturnsLoss) {
    fincore::factor_loss_report report;
    report.input_count = 10U;
    report.forward_returns_count = 7U;
    EXPECT_DOUBLE_EQ(fincore::legacy_forward_returns_loss(report), 0.3);

    fincore::factor_loss_report empty;
    EXPECT_DOUBLE_EQ(fincore::legacy_forward_returns_loss(empty), 0.0);
}
