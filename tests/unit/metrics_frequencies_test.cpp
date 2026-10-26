#include <fincore/metrics/frequencies.hpp>

#include <gtest/gtest.h>

#include <string>
#include <string_view>

namespace {

TEST(MetricsFrequencies, MapsCanonicalSourceNamesToNativeEnum) {
    EXPECT_EQ(fincore::pandas_frequency("daily"), fincore::frequency::daily);
    EXPECT_EQ(fincore::pandas_frequency("weekly"), fincore::frequency::weekly);
    EXPECT_EQ(fincore::pandas_frequency("monthly"),
              fincore::frequency::monthly);
    EXPECT_EQ(fincore::pandas_frequency("quarterly"),
              fincore::frequency::quarterly);
    EXPECT_EQ(fincore::pandas_frequency("yearly"), fincore::frequency::yearly);
}

TEST(MetricsFrequencies, PublishesSourceNamesAndPandasAliases) {
    EXPECT_EQ(fincore::daily_frequency_name, "daily");
    EXPECT_EQ(fincore::weekly_frequency_name, "weekly");
    EXPECT_EQ(fincore::monthly_frequency_name, "monthly");
    EXPECT_EQ(fincore::quarterly_frequency_name, "quarterly");
    EXPECT_EQ(fincore::yearly_frequency_name, "yearly");

    EXPECT_EQ(fincore::frequency_name(fincore::frequency::daily), "daily");
    EXPECT_EQ(fincore::frequency_name(fincore::frequency::weekly), "weekly");
    EXPECT_EQ(fincore::frequency_name(fincore::frequency::monthly), "monthly");
    EXPECT_EQ(fincore::frequency_name(fincore::frequency::quarterly),
              "quarterly");
    EXPECT_EQ(fincore::frequency_name(fincore::frequency::yearly), "yearly");

    // Pinned pandas (>= 2.2) aliases: D, W, ME, QE, YE.
    EXPECT_EQ(fincore::pandas_frequency_alias(fincore::frequency::daily), "D");
    EXPECT_EQ(fincore::pandas_frequency_alias(fincore::frequency::weekly), "W");
    EXPECT_EQ(fincore::pandas_frequency_alias(fincore::frequency::monthly),
              "ME");
    EXPECT_EQ(fincore::pandas_frequency_alias(fincore::frequency::quarterly),
              "QE");
    EXPECT_EQ(fincore::pandas_frequency_alias(fincore::frequency::yearly),
              "YE");
}

TEST(MetricsFrequencies, RoundTripsEveryCanonicalName) {
    constexpr std::string_view names[]{
        fincore::daily_frequency_name, fincore::weekly_frequency_name,
        fincore::monthly_frequency_name, fincore::quarterly_frequency_name,
        fincore::yearly_frequency_name};
    for (const std::string_view name : names) {
        EXPECT_EQ(fincore::frequency_name(fincore::pandas_frequency(name)),
                  name);
    }
}

TEST(MetricsFrequencies, RejectsUnknownNamesWithSourceMessage) {
    // Only the five canonical words are matched, so no letter form is accepted
    // even though aliases are published further up; the trailing-space entry
    // shows the input is compared verbatim and never trimmed.
    constexpr std::string_view invalid[]{
        "", "Daily", "annually", "M", "Q", "A", "daily "};
    for (const std::string_view name : invalid) {
        try {
            static_cast<void>(fincore::pandas_frequency(name));
            FAIL() << "expected input_error for " << name;
        } catch (const fincore::input_error& error) {
            const std::string message{error.what()};
            EXPECT_NE(message.find("Period cannot be"), std::string::npos);
            EXPECT_NE(message.find("unknown frequency"), std::string::npos);
        }
    }
}

TEST(MetricsFrequencies, RejectsInvalidNativeFrequencyValues) {
    // A scoped enum has a fixed integer underlying type, so 99 is a legal value
    // to hand over; the rejection comes from the range guard each accessor runs
    // before its switch, not from the enumeration itself.
    const auto invalid = static_cast<fincore::frequency>(99);
    EXPECT_THROW(static_cast<void>(fincore::frequency_name(invalid)),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::pandas_frequency_alias(invalid)),
                 fincore::input_error);
}

}  // namespace
