#include <fincore/attribution/factor_prediction.hpp>

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
#include <utility>
#include <vector>

namespace {

using fincore::timestamp;

constexpr std::array<std::string_view, 3U> factor_names{"MKT", "SMB", "HML"};
constexpr std::array<std::string_view, 3U> reordered_factor_names{
    "HML", "MKT", "SMB"};
constexpr std::array<timestamp, 8U> oracle_index{
    1704067200000000000LL, 1704153600000000000LL, 1704240000000000000LL,
    1704326400000000000LL, 1704412800000000000LL, 1704499200000000000LL,
    1704585600000000000LL, 1704672000000000000LL};

using prediction_function = fincore::dataframe (*) (
    const fincore::factor_regression_result&,
    const fincore::dataframe&,
    std::span<const std::string_view>);

static_assert(std::is_same_v<decltype(&fincore::factor_regression_predictions),
                             prediction_function>);

struct factor_fixture {
    fincore::dataframe returns;
    fincore::dataframe factors;
};

fincore::dataframe make_returns(std::vector<timestamp> index,
                                std::vector<double> values) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>("returns", std::move(values),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe make_factors(std::vector<timestamp> index,
                                std::vector<double> mkt,
                                std::vector<double> smb,
                                std::vector<double> hml) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>("MKT", std::move(mkt),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("SMB", std::move(smb),
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("HML", std::move(hml),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::dataframe make_one_factor_frame(std::vector<timestamp> index,
                                          std::vector<double> values) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<double>("MKT", std::move(values),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

factor_fixture oracle_fixture() {
    const std::vector<double> mkt{.01, -.02, .03, .00, .015, -.01, .025, .005};
    const std::vector<double> smb{.02, .01, -.01, .03, .00, .015, -.02, .01};
    const std::vector<double> hml{.00, .02, .01, -.01, .025, -.005, .015, -.02};
    const std::vector<double> residual{.0005, -.001, .0, .0015,
                                       -.0005, .00025, -.00075, .00025};
    std::vector<double> returns;
    returns.reserve(oracle_index.size());
    for (std::size_t row = 0; row < oracle_index.size(); ++row) {
        returns.push_back(.001 + 1.5 * mkt[row] - .5 * smb[row] +
                          .25 * hml[row] + residual[row]);
    }
    return {make_returns(std::vector<timestamp>{oracle_index.begin(), oracle_index.end()},
                         std::move(returns)),
            make_factors(std::vector<timestamp>{oracle_index.begin(), oracle_index.end()},
                         mkt, smb, hml)};
}

fincore::dataframe make_coefficients(std::vector<timestamp> index,
                                     std::vector<std::string> labels,
                                     std::vector<double> values) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    frame.load_column<std::string>(fincore::factor_regression_factor_column.data(),
                                   std::move(labels),
                                   hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>(fincore::factor_regression_coefficient_column.data(),
                              std::move(values),
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

fincore::factor_regression_result make_model(
    const double alpha,
    const std::span<const std::string_view> names,
    std::vector<double> betas) {
    std::vector<timestamp> index;
    std::vector<std::string> labels;
    index.reserve(names.size() + 1U);
    labels.reserve(names.size() + 1U);
    index.push_back(0);
    labels.emplace_back(fincore::factor_regression_alpha_name);
    for (std::size_t factor = 0; factor < names.size(); ++factor) {
        index.push_back(static_cast<timestamp>(factor + 1U));
        labels.emplace_back(names[factor]);
    }
    betas.insert(betas.begin(), alpha);
    return {alpha, 0.0, make_coefficients(std::move(index), std::move(labels),
                                           std::move(betas)),
            {}, {}};
}

void expect_prediction_schema(const fincore::dataframe& prediction,
                              const std::vector<timestamp>& expected_index) {
    ASSERT_EQ(prediction.shape().second, 1U);
    const auto columns = prediction.get_columns_info<double>();
    ASSERT_EQ(columns.size(), 1U);
    EXPECT_STREQ(std::get<0>(columns[0]).c_str(),
                 std::string{fincore::factor_prediction_column}.c_str());
    EXPECT_EQ(std::get<1>(columns[0]), expected_index.size());
    EXPECT_EQ(std::get<2>(columns[0]), typeid(double));
    EXPECT_EQ(prediction.get_index(), expected_index);
    EXPECT_EQ(prediction.get_column<double>(fincore::factor_prediction_column.data()).size(),
              expected_index.size());
}

void expect_same_predictions(const fincore::dataframe& actual,
                             const fincore::dataframe& expected) {
    EXPECT_EQ(actual.get_index(), expected.get_index());
    const auto& actual_values = actual.get_column<double>(
        fincore::factor_prediction_column.data());
    const auto& expected_values = expected.get_column<double>(
        fincore::factor_prediction_column.data());
    ASSERT_EQ(actual_values.size(), expected_values.size());
    for (std::size_t row = 0; row < actual_values.size(); ++row) {
        EXPECT_NEAR(actual_values[row], expected_values[row], 1.0e-12);
    }
}

TEST(FactorPrediction, PublicHeaderDeclaresPredictionApi) {
    EXPECT_EQ(fincore::factor_prediction_column, "predicted_returns");
}

TEST(FactorPrediction, MatchesSourceOracleAndPreservesOneColumnRawIndex) {
    const auto fixture = oracle_fixture();
    const auto regression = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);
    const auto prediction = fincore::factor_regression_predictions(
        regression, fixture.factors, factor_names);

    expect_prediction_schema(prediction,
                             std::vector<timestamp>{oracle_index.begin(), oracle_index.end()});
    const auto& values = prediction.get_column<double>(
        fincore::factor_prediction_column.data());
    ASSERT_EQ(values.size(), 8U);
    EXPECT_NEAR(values[0], .0068203580214942685, 1.0e-12);
    EXPECT_NEAR(values[1], -.02992443386996989, 1.0e-12);
    EXPECT_NEAR(values[2], .0533598324629174, 1.0e-12);
    EXPECT_NEAR(values[3], -.015348761635518682, 1.0e-12);
    EXPECT_NEAR(values[4], .029356958651646607, 1.0e-12);
    EXPECT_NEAR(values[5], -.0227002927762671, 1.0e-12);
    EXPECT_NEAR(values[6], .05139868826161392, 1.0e-12);
    EXPECT_NEAR(values[7], -.0009623491159163553, 1.0e-12);
}

TEST(FactorPrediction, RetainsNonmonotonicDuplicateIndexAndBindsRequestedOrder) {
    const auto fixture = oracle_fixture();
    const auto regression = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);
    const auto reordered_regression = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, reordered_factor_names);
    const std::vector<timestamp> prediction_index{42, 7, 42, 3};
    const auto prediction_factors = make_factors(
        prediction_index, {.04, -.01, .03, .02}, {.01, .02, -.03, .04},
        {-.02, .03, .01, -.04});

    const auto prediction = fincore::factor_regression_predictions(
        regression, prediction_factors, factor_names);
    const auto reordered_prediction = fincore::factor_regression_predictions(
        reordered_regression, prediction_factors, reordered_factor_names);

    expect_prediction_schema(prediction, prediction_index);
    expect_prediction_schema(reordered_prediction, prediction_index);
    const auto& coefficients = regression.coefficients.get_column<double>(
        fincore::factor_regression_coefficient_column.data());
    const auto& mkt = prediction_factors.get_column<double>("MKT");
    const auto& smb = prediction_factors.get_column<double>("SMB");
    const auto& hml = prediction_factors.get_column<double>("HML");
    const auto& values = prediction.get_column<double>(
        fincore::factor_prediction_column.data());
    for (std::size_t row = 0; row < values.size(); ++row) {
        const double expected = coefficients[0] + coefficients[1] * mkt[row] +
                                coefficients[2] * smb[row] + coefficients[3] * hml[row];
        EXPECT_NEAR(values[row], expected, 1.0e-12);
    }
    expect_same_predictions(reordered_prediction, prediction);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, prediction_factors, reordered_factor_names),
                 fincore::input_error);
}

TEST(FactorPrediction, SupportsTypedEmptyOutputAndOwnsInputs) {
    const auto fixture = oracle_fixture();
    const auto regression = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);
    const auto empty_factors = make_factors({}, {}, {}, {});
    const auto empty_prediction = fincore::factor_regression_predictions(
        regression, empty_factors, factor_names);
    expect_prediction_schema(empty_prediction, {});
    EXPECT_TRUE(empty_prediction.get_column<double>(
                    fincore::factor_prediction_column.data()).empty());

    const auto owned_prediction = [] {
        auto local_fixture = oracle_fixture();
        auto local_regression = fincore::fit_factor_regression(
            local_fixture.returns, local_fixture.factors, factor_names);
        std::array<std::string, 3U> names{"MKT", "SMB", "HML"};
        const std::array<std::string_view, 3U> views{
            names[0], names[1], names[2]};
        auto output = fincore::factor_regression_predictions(
            local_regression, local_fixture.factors, views);
        local_regression.alpha = 123.0;
        local_regression.coefficients.get_column<double>(
            fincore::factor_regression_coefficient_column.data())[0] = 456.0;
        local_fixture.factors.get_column<double>("MKT")[0] = -789.0;
        local_fixture.factors.load_index({11, 12, 13, 14, 15, 16, 17, 18});
        names[0] = "mutated-market";
        return output;
    }();

    expect_prediction_schema(owned_prediction,
                             std::vector<timestamp>{oracle_index.begin(), oracle_index.end()});
    const auto& values = owned_prediction.get_column<double>(
        fincore::factor_prediction_column.data());
    ASSERT_EQ(values.size(), 8U);
    EXPECT_NEAR(values[0], .0068203580214942685, 1.0e-12);
    EXPECT_NEAR(values[7], -.0009623491159163553, 1.0e-12);
}

TEST(FactorPrediction, AcceptsStructurallyValidMaterializedModel) {
    constexpr std::array<std::string_view, 1U> one_factor{"MKT"};
    const auto model = make_model(.5, one_factor, {2.0});
    const std::vector<timestamp> index{9, 4, 9};
    const auto factors = make_one_factor_frame(index, {1.0, -.5, .0});

    const auto prediction = fincore::factor_regression_predictions(
        model, factors, one_factor);

    expect_prediction_schema(prediction, index);
    const auto& values = prediction.get_column<double>(
        fincore::factor_prediction_column.data());
    ASSERT_EQ(values.size(), 3U);
    EXPECT_DOUBLE_EQ(values[0], 2.5);
    EXPECT_DOUBLE_EQ(values[1], -.5);
    EXPECT_DOUBLE_EQ(values[2], .5);
}

TEST(FactorPrediction, RejectsInvalidSelectorsAndSelectedInputs) {
    const auto fixture = oracle_fixture();
    const auto regression = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);
    constexpr std::array<std::string_view, 0U> no_factors{};
    constexpr std::array<std::string_view, 1U> empty_factor{""};
    constexpr std::array<std::string_view, 2U> duplicate_factors{"MKT", "MKT"};
    constexpr std::array<std::string_view, 1U> alpha_factor{"alpha"};
    constexpr std::array<char, 8U> embedded_storage{
        'M', 'K', 'T', '\0', 'e', 'v', 'i', 'l'};
    const std::array<std::string_view, 3U> embedded_factors{
        std::string_view{embedded_storage.data(), embedded_storage.size()}, "SMB", "HML"};

    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, fixture.factors, no_factors),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, fixture.factors, empty_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, fixture.factors, duplicate_factors),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, fixture.factors, alpha_factor),
                 fincore::input_error);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, fixture.factors, embedded_factors),
                 fincore::input_error);

    constexpr std::array<char, 3U> bounded_storage{'M', 'K', 'T'};
    const std::array<std::string_view, 3U> bounded_names{
        std::string_view{bounded_storage.data(), bounded_storage.size()}, "SMB", "HML"};
    const auto bounded_prediction = fincore::factor_regression_predictions(
        regression, fixture.factors, bounded_names);
    EXPECT_NEAR(bounded_prediction.get_column<double>(
                    fincore::factor_prediction_column.data())[0],
                .0068203580214942685, 1.0e-12);

    auto missing_factor = fixture.factors;
    missing_factor.remove_column<double>("MKT");
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, missing_factor, factor_names),
                 fincore::input_error);

    auto wrong_type_factor = fixture.factors;
    wrong_type_factor.remove_column<double>("MKT");
    wrong_type_factor.load_column<int>("MKT", {1, 2, 3, 4, 5, 6, 7, 8},
                                       hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, wrong_type_factor, factor_names),
                 fincore::input_error);

    auto short_factor = fixture.factors;
    short_factor.remove_column<double>("SMB");
    short_factor.load_column<double>("SMB", {.1},
                                           hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, short_factor, factor_names),
                 fincore::input_error);

    auto nan_factor = fixture.factors;
    nan_factor.get_column<double>("MKT")[0] = std::numeric_limits<double>::quiet_NaN();
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, nan_factor, factor_names),
                 fincore::input_error);

    auto positive_infinite_factor = fixture.factors;
    positive_infinite_factor.get_column<double>("SMB")[0] =
        std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, positive_infinite_factor, factor_names),
                 fincore::input_error);

    auto negative_infinite_factor = fixture.factors;
    negative_infinite_factor.get_column<double>("HML")[0] =
        -std::numeric_limits<double>::infinity();
    EXPECT_THROW(fincore::factor_regression_predictions(
                     regression, negative_infinite_factor, factor_names),
                 fincore::input_error);

    constexpr std::array<std::string_view, 1U> one_factor{"MKT"};
    const double maximum = std::numeric_limits<double>::max();
    const auto product_overflow_model = make_model(0.0, one_factor, {maximum});
    const auto product_overflow_frame = make_one_factor_frame({1}, {2.0});
    EXPECT_THROW(fincore::factor_regression_predictions(
                     product_overflow_model, product_overflow_frame, one_factor),
                 fincore::input_error);

    const auto sum_overflow_model = make_model(maximum, one_factor, {1.0});
    const auto sum_overflow_frame = make_one_factor_frame({1}, {maximum});
    EXPECT_THROW(fincore::factor_regression_predictions(
                     sum_overflow_model, sum_overflow_frame, one_factor),
                 fincore::input_error);
}

TEST(FactorPrediction, RejectsMalformedPublicModelState) {
    const auto fixture = oracle_fixture();
    const auto valid_model = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);
    const auto expect_invalid = [&](const fincore::factor_regression_result& model) {
        EXPECT_THROW(fincore::factor_regression_predictions(
                         model, fixture.factors, factor_names),
                     fincore::input_error);
    };

    for (const double alpha : {std::numeric_limits<double>::quiet_NaN(),
                               std::numeric_limits<double>::infinity(),
                               -std::numeric_limits<double>::infinity()}) {
        auto model = valid_model;
        model.alpha = alpha;
        expect_invalid(model);
    }

    auto mismatched_alpha = valid_model;
    mismatched_alpha.alpha += 1.0;
    expect_invalid(mismatched_alpha);

    auto nonordinal_index = valid_model;
    nonordinal_index.coefficients.load_index({0, 2, 3, 4});
    expect_invalid(nonordinal_index);

    auto missing_factor_column = valid_model;
    missing_factor_column.coefficients.remove_column<std::string>(
        fincore::factor_regression_factor_column.data());
    expect_invalid(missing_factor_column);

    auto missing_coefficient_column = valid_model;
    missing_coefficient_column.coefficients.remove_column<double>(
        fincore::factor_regression_coefficient_column.data());
    expect_invalid(missing_coefficient_column);

    auto extra_column = valid_model;
    extra_column.coefficients.load_column<int>("extra", {1, 2, 3, 4},
                                               hmdf::nan_policy::dont_pad_with_nans);
    expect_invalid(extra_column);

    auto wrong_factor_type = valid_model;
    wrong_factor_type.coefficients.remove_column<std::string>(
        fincore::factor_regression_factor_column.data());
    wrong_factor_type.coefficients.load_column<int>(
        fincore::factor_regression_factor_column.data(), {1, 2, 3, 4},
        hmdf::nan_policy::dont_pad_with_nans);
    expect_invalid(wrong_factor_type);

    auto wrong_coefficient_type = valid_model;
    wrong_coefficient_type.coefficients.remove_column<double>(
        fincore::factor_regression_coefficient_column.data());
    wrong_coefficient_type.coefficients.load_column<int>(
        fincore::factor_regression_coefficient_column.data(), {1, 2, 3, 4},
        hmdf::nan_policy::dont_pad_with_nans);
    expect_invalid(wrong_coefficient_type);

    auto short_factor_column = valid_model;
    short_factor_column.coefficients.remove_column<std::string>(
        fincore::factor_regression_factor_column.data());
    short_factor_column.coefficients.load_column<std::string>(
        fincore::factor_regression_factor_column.data(), {"alpha"},
        hmdf::nan_policy::dont_pad_with_nans);
    expect_invalid(short_factor_column);

    auto short_coefficient_column = valid_model;
    short_coefficient_column.coefficients.remove_column<double>(
        fincore::factor_regression_coefficient_column.data());
    short_coefficient_column.coefficients.load_column<double>(
        fincore::factor_regression_coefficient_column.data(), {0.0},
        hmdf::nan_policy::dont_pad_with_nans);
    expect_invalid(short_coefficient_column);

    auto wrong_alpha_label = valid_model;
    wrong_alpha_label.coefficients.get_column<std::string>(
        fincore::factor_regression_factor_column.data())[0] = "not_alpha";
    expect_invalid(wrong_alpha_label);

    auto missing_label = valid_model;
    missing_label.coefficients.get_column<std::string>(
        fincore::factor_regression_factor_column.data())[1] = "missing";
    expect_invalid(missing_label);

    auto reordered_labels = valid_model;
    auto& labels = reordered_labels.coefficients.get_column<std::string>(
        fincore::factor_regression_factor_column.data());
    std::swap(labels[1], labels[2]);
    expect_invalid(reordered_labels);

    for (const double coefficient : {std::numeric_limits<double>::quiet_NaN(),
                                     std::numeric_limits<double>::infinity(),
                                     -std::numeric_limits<double>::infinity()}) {
        auto model = valid_model;
        model.coefficients.get_column<double>(
            fincore::factor_regression_coefficient_column.data())[1] = coefficient;
        expect_invalid(model);
    }
}

TEST(FactorPrediction, UsesOnlyValidatedModelStateAndSelectedColumns) {
    const auto fixture = oracle_fixture();
    const auto valid_model = fincore::fit_factor_regression(
        fixture.returns, fixture.factors, factor_names);
    const auto baseline = fincore::factor_regression_predictions(
        valid_model, fixture.factors, factor_names);

    auto nonfinite_r_squared = valid_model;
    nonfinite_r_squared.r_squared = std::numeric_limits<double>::quiet_NaN();
    expect_same_predictions(fincore::factor_regression_predictions(
                                nonfinite_r_squared, fixture.factors, factor_names),
                            baseline);

    auto malformed_fitted = valid_model;
    malformed_fitted.fitted_returns.load_index({1});
    malformed_fitted.fitted_returns.load_column<int>("malformed", {7},
                                                      hmdf::nan_policy::dont_pad_with_nans);
    expect_same_predictions(fincore::factor_regression_predictions(
                                malformed_fitted, fixture.factors, factor_names),
                            baseline);

    auto malformed_residuals = valid_model;
    malformed_residuals.residuals.load_index({1});
    malformed_residuals.residuals.load_column<int>("malformed", {7},
                                                    hmdf::nan_policy::dont_pad_with_nans);
    expect_same_predictions(fincore::factor_regression_predictions(
                                malformed_residuals, fixture.factors, factor_names),
                            baseline);

    auto malformed_unselected_metadata = fixture.factors;
    malformed_unselected_metadata.load_column<int>("unrelated_malformed", {7},
                                                    hmdf::nan_policy::dont_pad_with_nans);
    const auto with_metadata = fincore::factor_regression_predictions(
        valid_model, malformed_unselected_metadata, factor_names);
    expect_same_predictions(with_metadata, baseline);
}

}  // namespace
