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

}  // namespace
