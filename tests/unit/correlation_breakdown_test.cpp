#include <fincore/simulation/correlation.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace {

using fincore::timestamp;

using correlation_breakdown_function = fincore::dataframe (*) (
    std::span<const std::string_view>, double);

// The pointer type pins the exported parameter list and the by-value return only;
// default arguments are not part of a function's type, so the single-argument call
// in the first test is what pins the default target.
static_assert(
    std::is_same_v<decltype(&fincore::generate_correlation_breakdown),
                   correlation_breakdown_function>);

constexpr std::array<std::string_view, 3U> oracle_asset_names{
    "equity", "credit", "commodity"};

void expect_named_matrix_schema(
    const fincore::dataframe& matrix,
    const std::span<const std::string_view> expected_asset_names) {
    std::vector<timestamp> expected_index;
    expected_index.reserve(expected_asset_names.size());
    for (std::size_t position = 0; position < expected_asset_names.size();
         ++position) {
        expected_index.push_back(static_cast<timestamp>(position));
    }

    EXPECT_EQ(matrix.get_index(), expected_index);
    ASSERT_EQ(matrix.shape().first, expected_asset_names.size());
    ASSERT_EQ(matrix.shape().second, expected_asset_names.size());

    const auto columns = matrix.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected_asset_names.size());
    for (std::size_t column = 0; column < expected_asset_names.size();
         ++column) {
        EXPECT_STREQ(std::get<0>(columns[column]).c_str(),
                     std::string{expected_asset_names[column]}.c_str());
        EXPECT_EQ(std::get<1>(columns[column]), expected_asset_names.size());
        EXPECT_EQ(std::get<2>(columns[column]), typeid(double));
    }
}

void expect_named_matrix_values(
    const fincore::dataframe& matrix,
    const std::span<const std::string_view> asset_names,
    const double target_correlation) {
    expect_named_matrix_schema(matrix, asset_names);
    for (std::size_t row = 0; row < asset_names.size(); ++row) {
        for (std::size_t column = 0; column < asset_names.size(); ++column) {
            const auto& values = matrix.get_column<double>(asset_names[column].data());
            EXPECT_DOUBLE_EQ(values[row],
                             row == column ? 1.0 : target_correlation);
        }
    }
}

TEST(CorrelationBreakdown, MatchesPythonSourceOracleMatrices) {
    const auto default_target = fincore::generate_correlation_breakdown(
        oracle_asset_names);
    const auto quarter_target = fincore::generate_correlation_breakdown(
        oracle_asset_names, 0.25);
    const auto negative_target = fincore::generate_correlation_breakdown(
        oracle_asset_names, -0.5);

    expect_named_matrix_values(default_target, oracle_asset_names, 1.0);
    expect_named_matrix_values(quarter_target, oracle_asset_names, 0.25);
    expect_named_matrix_values(negative_target, oracle_asset_names, -0.5);
}

TEST(CorrelationBreakdown, PreservesCallerNameOrderAndExactOrdinalIndex) {
    constexpr std::array<std::string_view, 3U> ordered_names{
        "commodity", "equity", "credit"};
    const auto matrix = fincore::generate_correlation_breakdown(ordered_names, 0.25);

    expect_named_matrix_values(matrix, ordered_names, 0.25);
}

TEST(CorrelationBreakdown, SupportsOneAssetAndTypedEmptyMatrices) {
    constexpr std::array<std::string_view, 1U> one_asset{"only"};
    constexpr std::array<std::string_view, 0U> no_assets{};

    const auto one_by_one = fincore::generate_correlation_breakdown(one_asset, -0.5);
    expect_named_matrix_values(one_by_one, one_asset, -0.5);

    const auto empty = fincore::generate_correlation_breakdown(no_assets, 0.25);
    expect_named_matrix_schema(empty, no_assets);
    EXPECT_TRUE(empty.get_columns_info<double>().empty());
}

TEST(CorrelationBreakdown, RetainsFiniteOutOfRangeOffDiagonalTargets) {
    constexpr std::array<std::string_view, 2U> asset_names{"one", "two"};

    const auto positive = fincore::generate_correlation_breakdown(asset_names, 2.5);
    const auto negative = fincore::generate_correlation_breakdown(asset_names, -2.5);

    expect_named_matrix_values(positive, asset_names, 2.5);
    expect_named_matrix_values(negative, asset_names, -2.5);
}

TEST(CorrelationBreakdown, RejectsInvalidAssetNamesAndTargets) {
    constexpr std::array<std::string_view, 1U> empty_name{""};
    constexpr std::array<std::string_view, 2U> duplicate_names{"same", "same"};
    constexpr std::array<char, 8U> embedded_nul_storage{
        'e', 'q', 'u', 'i', 't', 'y', '\0', 'x'};
    const std::array<std::string_view, 1U> embedded_nul_name{
        std::string_view{embedded_nul_storage.data(), embedded_nul_storage.size()}};
    constexpr std::array<std::string_view, 1U> valid_name{"valid"};

    EXPECT_THROW(fincore::generate_correlation_breakdown(empty_name),
                 fincore::input_error);
    EXPECT_THROW(fincore::generate_correlation_breakdown(duplicate_names),
                 fincore::input_error);
    EXPECT_THROW(fincore::generate_correlation_breakdown(embedded_nul_name),
                 fincore::input_error);

    for (const double target : {std::numeric_limits<double>::quiet_NaN(),
                                std::numeric_limits<double>::infinity(),
                                -std::numeric_limits<double>::infinity()}) {
        EXPECT_THROW(fincore::generate_correlation_breakdown(valid_name, target),
                     fincore::input_error);
    }
}

TEST(CorrelationBreakdown, AcceptsBoundedNonTerminatedNames) {
    constexpr std::array<char, 4U> bounded_storage{'B', 'O', 'N', 'D'};
    const std::array<std::string_view, 1U> bounded_name{
        std::string_view{bounded_storage.data(), bounded_storage.size()}};

    const auto matrix = fincore::generate_correlation_breakdown(bounded_name, 0.25);

    // The four characters carry no terminator, so a label taken from the view's
    // pointer alone would read past them; finding "BOND" proves the length was
    // honoured when the column name was built.
    expect_named_matrix_schema(matrix, bounded_name);
    EXPECT_DOUBLE_EQ(matrix.get_column<double>("BOND")[0], 1.0);
}

TEST(CorrelationBreakdown, OwnsCopiedNameLabels) {
    const auto matrix = [] {
        std::vector<std::string> source_names{"equity", "credit"};
        const std::array<std::string_view, 2U> name_views{
            source_names[0], source_names[1]};
        auto result = fincore::generate_correlation_breakdown(name_views, 0.25);
        source_names[0] = "mutated-equity";
        source_names[1].clear();
        return result;
    }();
    constexpr std::array<std::string_view, 2U> expected_names{"equity", "credit"};

    expect_named_matrix_values(matrix, expected_names, 0.25);
}

}  // namespace
