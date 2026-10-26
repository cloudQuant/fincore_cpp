#include <fincore/performance/disclosures.hpp>

#include <gtest/gtest.h>

#include <string>

namespace {

TEST(PerformanceDisclosures, RendersDefaultContextExactly) {
    const fincore::disclosure_context context;
    const std::string expected =
        "Convention: TWR\n"
        "Return type: unspecified\n"
        "Units: unspecified\n"
        "Frequency: unspecified\n"
        "Sample period: unspecified\n"
        "Data quality: unspecified\n"
        "Fees: gross-of-fees\n"
        "Cashflows: none\n"
        "Benchmark: none\n"
        "Risk-free: unspecified\n"
        "Annualized: yes\n";
    EXPECT_EQ(fincore::render_disclosure(context), expected);
}

TEST(PerformanceDisclosures, RendersCustomContextWithNotes) {
    fincore::disclosure_context context;
    context.convention = "MWR";
    context.sample_period = "2020-2021";
    context.data_quality = "audited";
    context.fees = "net-of-fees";
    context.cashflows = "contributions";
    context.benchmark = "SPX";
    context.risk_free = "3%";
    context.annualized = false;
    context.notes = {"A", "B"};
    context.return_type = "arithmetic";
    context.units = "USD";
    context.frequency = "monthly";

    // Notes are appended without a line terminator, so this expected block ends
    // mid-line whereas the note-free default block ends after a newline.
    const std::string expected =
        "Convention: MWR\n"
        "Return type: arithmetic\n"
        "Units: USD\n"
        "Frequency: monthly\n"
        "Sample period: 2020-2021\n"
        "Data quality: audited\n"
        "Fees: net-of-fees\n"
        "Cashflows: contributions\n"
        "Benchmark: SPX\n"
        "Risk-free: 3%\n"
        "Annualized: no\n"
        "Notes: A; B";
    EXPECT_EQ(fincore::render_disclosure(context), expected);
}

TEST(PerformanceDisclosures, RendersEmptyFieldsAsUnspecified) {
    fincore::disclosure_context context;
    // Clearing every field is not the same as the default context: benchmark is
    // the one text field whose empty value renders "none" rather than
    // "unspecified", because a missing benchmark is itself a stated fact.
    context.convention.clear();
    context.return_type.clear();
    context.units.clear();
    context.frequency.clear();
    context.sample_period.clear();
    context.data_quality.clear();
    context.fees.clear();
    context.cashflows.clear();
    context.benchmark.clear();
    context.risk_free.clear();
    context.notes.clear();

    const std::string expected =
        "Convention: unspecified\n"
        "Return type: unspecified\n"
        "Units: unspecified\n"
        "Frequency: unspecified\n"
        "Sample period: unspecified\n"
        "Data quality: unspecified\n"
        "Fees: unspecified\n"
        "Cashflows: unspecified\n"
        "Benchmark: none\n"
        "Risk-free: unspecified\n"
        "Annualized: yes\n";
    EXPECT_EQ(fincore::render_disclosure(context), expected);
}

TEST(PerformanceDisclosures, PreservesUnicodeAndRepeatedNotesDeterministically) {
    fincore::disclosure_context context;
    context.convention = "时间加权收益率（TWR）";
    context.sample_period = "2021年第一季度";
    context.data_quality = "经审计的数据";
    context.fees = "扣除管理费后";
    context.cashflows = "月度申购与赎回";
    context.benchmark = "沪深300指数";
    context.risk_free = "中国国债收益率";
    context.annualized = false;
    context.notes = {"流动性提示：低", "相同说明", "相同说明"};
    context.return_type = "简单收益率";
    context.units = "人民币";
    context.frequency = "月度";

    const std::string expected =
        "Convention: 时间加权收益率（TWR）\n"
        "Return type: 简单收益率\n"
        "Units: 人民币\n"
        "Frequency: 月度\n"
        "Sample period: 2021年第一季度\n"
        "Data quality: 经审计的数据\n"
        "Fees: 扣除管理费后\n"
        "Cashflows: 月度申购与赎回\n"
        "Benchmark: 沪深300指数\n"
        "Risk-free: 中国国债收益率\n"
        "Annualized: no\n"
        "Notes: 流动性提示：低; 相同说明; 相同说明";

    const auto first = fincore::render_disclosure(context);
    EXPECT_EQ(first, expected);
    EXPECT_EQ(fincore::render_disclosure(context), first);
    EXPECT_NE(first.find("相同说明; 相同说明"), std::string::npos);
}

}  // namespace
