#include <fincore/simulation/paths.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <tuple>
#include <type_traits>
#include <typeindex>
#include <utility>
#include <vector>

namespace {

using antithetic_variates_function = fincore::dataframe (*) (const fincore::dataframe&);

static_assert(std::is_same_v<decltype(&fincore::antithetic_variates),
                             antithetic_variates_function>);

fincore::dataframe canonical_paths() {
    fincore::dataframe paths;
    paths.load_index(std::vector<fincore::timestamp>{9, 3, 9});
    paths.load_column<double>("path_0", std::vector<double>{100.0, 110.0, 120.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    paths.load_column<double>("path_1", std::vector<double>{100.0, 90.0, 80.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    return paths;
}

void expect_exact_schema(const fincore::dataframe& paths,
                         const std::size_t expected_rows,
                         const std::vector<std::string>& expected_names) {
    ASSERT_EQ(paths.shape().first, expected_rows);
    ASSERT_EQ(paths.shape().second, expected_names.size());

    const auto columns = paths.get_columns_info<double>();
    ASSERT_EQ(columns.size(), expected_names.size());
    for (std::size_t column = 0U; column < expected_names.size(); ++column) {
        EXPECT_EQ(std::get<0>(columns[column]), expected_names[column].c_str());
        EXPECT_EQ(std::get<1>(columns[column]), expected_rows);
        EXPECT_EQ(std::get<2>(columns[column]), typeid(double));
    }
}

TEST(AntitheticVariates, PublishesNativePathContractAndMirrorsEachColumn) {
    EXPECT_EQ(fincore::antithetic_path_column_prefix, "antithetic_");

    const auto result = fincore::antithetic_variates(canonical_paths());

    EXPECT_EQ(result.get_index(), (std::vector<fincore::timestamp>{9, 3, 9}));
    expect_exact_schema(result, 3U,
                        {"path_0", "path_1", "antithetic_path_0",
                         "antithetic_path_1"});
    EXPECT_EQ(result.get_column<double>("path_0"),
              (std::vector<double>{100.0, 110.0, 120.0}));
    EXPECT_EQ(result.get_column<double>("path_1"),
              (std::vector<double>{100.0, 90.0, 80.0}));
    EXPECT_EQ(result.get_column<double>("antithetic_path_0"),
              (std::vector<double>{100.0, 90.0, 80.0}));
    EXPECT_EQ(result.get_column<double>("antithetic_path_1"),
              (std::vector<double>{100.0, 110.0, 120.0}));
}

TEST(AntitheticVariates, MirrorsTheSingletonPathAroundItsOnlyInitialValue) {
    fincore::dataframe paths;
    paths.load_index(std::vector<fincore::timestamp>{42});
    paths.load_column<double>("path", std::vector<double>{1.0},
                              hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::antithetic_variates(paths);

    EXPECT_EQ(result.get_index(), (std::vector<fincore::timestamp>{42}));
    expect_exact_schema(result, 1U, {"path", "antithetic_path"});
    EXPECT_EQ(result.get_column<double>("path"), (std::vector<double>{1.0}));
    EXPECT_EQ(result.get_column<double>("antithetic_path"),
              (std::vector<double>{1.0}));
}

TEST(AntitheticVariates, PreservesPythonStyleNanAndInfinityArithmetic) {
    fincore::dataframe paths;
    paths.load_index(std::vector<fincore::timestamp>{1, 2});
    paths.load_column<double>("path_0",
                              std::vector<double>{1.0,
                                                  std::numeric_limits<double>::infinity()},
                              hmdf::nan_policy::dont_pad_with_nans);
    paths.load_column<double>("path_1",
                              std::vector<double>{std::numeric_limits<double>::quiet_NaN(),
                                                  -std::numeric_limits<double>::infinity()},
                              hmdf::nan_policy::dont_pad_with_nans);

    const auto result = fincore::antithetic_variates(paths);

    expect_exact_schema(result, 2U,
                        {"path_0", "path_1", "antithetic_path_0",
                         "antithetic_path_1"});
    EXPECT_EQ(result.get_column<double>("antithetic_path_0")[0], 1.0);
    EXPECT_EQ(result.get_column<double>("antithetic_path_0")[1],
              -std::numeric_limits<double>::infinity());
    EXPECT_TRUE(std::isnan(result.get_column<double>("antithetic_path_1")[0]));
    EXPECT_EQ(result.get_column<double>("antithetic_path_1")[1],
              std::numeric_limits<double>::infinity());
}

TEST(AntitheticVariates, RejectsEmptyAndNonDoublePathTables) {
    fincore::dataframe empty_rows;
    empty_rows.load_index(std::vector<fincore::timestamp>{});
    empty_rows.load_column<double>("path", std::vector<double>{},
                                   hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::antithetic_variates(empty_rows), fincore::input_error);

    fincore::dataframe empty_columns;
    empty_columns.load_index(std::vector<fincore::timestamp>{1});
    EXPECT_THROW(fincore::antithetic_variates(empty_columns), fincore::input_error);

    fincore::dataframe only_integer;
    only_integer.load_index(std::vector<fincore::timestamp>{1});
    only_integer.load_column<int>("path", std::vector<int>{1},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::antithetic_variates(only_integer), fincore::input_error);

    auto mixed = canonical_paths();
    mixed.load_column<int>("metadata", std::vector<int>{1, 2, 3},
                           hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::antithetic_variates(mixed), fincore::input_error);
}

TEST(AntitheticVariates, RejectsMalformedColumnsAndDerivedNameCollisions) {
    fincore::dataframe malformed;
    malformed.load_index(std::vector<fincore::timestamp>{1, 2});
    malformed.load_column<double>("path", std::vector<double>{1.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::antithetic_variates(malformed), fincore::input_error);

    auto colliding = canonical_paths();
    colliding.load_column<double>("antithetic_path_0",
                                  std::vector<double>{100.0, 90.0, 80.0},
                                  hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::antithetic_variates(colliding), fincore::input_error);

    const std::string source_name(52U, 'x');
    fincore::dataframe too_long;
    too_long.load_index(std::vector<fincore::timestamp>{1});
    too_long.load_column<double>(source_name.c_str(), std::vector<double>{1.0},
                                 hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::antithetic_variates(too_long), fincore::input_error);
}

TEST(AntitheticVariates, OwnsCopiedIndexAndValuesAfterSourceMutationAndDestruction) {
    const auto result = [] {
        auto paths = canonical_paths();
        const auto transformed = fincore::antithetic_variates(paths);
        paths.get_column<double>("path_0")[1] = -999.0;
        paths.get_column<double>("path_1")[2] = -888.0;
        paths.load_index(std::vector<fincore::timestamp>{1, 2, 3});
        return transformed;
    }();

    EXPECT_EQ(result.get_index(), (std::vector<fincore::timestamp>{9, 3, 9}));
    EXPECT_EQ(result.get_column<double>("path_0"),
              (std::vector<double>{100.0, 110.0, 120.0}));
    EXPECT_EQ(result.get_column<double>("path_1"),
              (std::vector<double>{100.0, 90.0, 80.0}));
    EXPECT_EQ(result.get_column<double>("antithetic_path_0"),
              (std::vector<double>{100.0, 90.0, 80.0}));
    EXPECT_EQ(result.get_column<double>("antithetic_path_1"),
              (std::vector<double>{100.0, 110.0, 120.0}));
}

}  // namespace
