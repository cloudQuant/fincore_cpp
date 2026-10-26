#include <fincore/errors.hpp>
#include <fincore/simulation/base.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <limits>
#include <string>
#include <vector>

namespace {

TEST(SimulationBase, ValidatesAndCopiesFiniteReturns) {
    const std::vector<double> input{.01, -.02, .03, 0.0};
    auto validated = fincore::validate_returns(input);
    ASSERT_EQ(validated.size(), input.size());
    EXPECT_EQ(validated, input);
    // Writing through the result is what proves it owns its values: a view over
    // the caller's vector would leave `input` reading 9.0 here.
    validated[0] = 9.0;
    EXPECT_DOUBLE_EQ(input[0], .01);

    const auto frame = fincore::make_series(
        {1, 2, 3}, "returns", {.01, -.02, .03});
    const auto from_frame = fincore::validate_returns(frame);
    EXPECT_EQ(from_frame, (std::vector<double>{.01, -.02, .03}));
}

TEST(SimulationBase, RejectsEmptyNonFiniteAndMissingColumns) {
    try {
        static_cast<void>(fincore::validate_returns(std::span<const double>{}));
        FAIL() << "empty returns must fail";
    } catch (const fincore::fincore_error& error) {
        EXPECT_EQ(error.category(), "ValidationError");
        EXPECT_EQ(error.code(), "fincore.validation");
        EXPECT_NE(std::string{error.what()}.find("empty"), std::string::npos);
        EXPECT_EQ(error.fields().at("param_name"), "returns");
    }

    const std::vector<double> with_nan{
        .01, std::numeric_limits<double>::quiet_NaN()};
    try {
        static_cast<void>(fincore::validate_returns(with_nan));
        FAIL() << "NaN returns must fail";
    } catch (const fincore::fincore_error& error) {
        EXPECT_NE(std::string{error.what()}.find("NaN"), std::string::npos);
    }

    const std::vector<double> with_inf{
        .01, std::numeric_limits<double>::infinity()};
    EXPECT_THROW(static_cast<void>(fincore::validate_returns(with_inf)),
                 fincore::fincore_error);

    const auto empty_frame = fincore::make_series({}, "returns", {});
    EXPECT_THROW(static_cast<void>(fincore::validate_returns(empty_frame)),
                 fincore::fincore_error);
    const auto missing = fincore::make_series({1, 2}, "other", {.01, .02});
    EXPECT_THROW(static_cast<void>(fincore::validate_returns(missing)),
                 fincore::fincore_error);
}

}  // namespace
