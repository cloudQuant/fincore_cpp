#include <fincore/attribution.hpp>

#include <gtest/gtest.h>

#include <array>
#include <vector>

namespace {

fincore::dataframe make_frame(const std::vector<double>& tech,
                              const std::vector<double>& health) {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{1704153600000000000LL});
    frame.load_column<double>("tech", tech, hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("health", health,
                              hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

TEST(AttributionUmbrella, ExportsTheDataframeNativeBrinsonSurface) {
    constexpr std::array<std::string_view, 2U> sectors{"tech", "health"};
    const auto portfolio_returns = make_frame({.10}, {.05});
    const auto benchmark_returns = make_frame({.08}, {.04});
    const auto portfolio_weights = make_frame({.60}, {.40});
    const auto benchmark_weights = make_frame({.50}, {.50});

    const auto period = fincore::brinson_attribution(
        portfolio_returns, benchmark_returns, portfolio_weights,
        benchmark_weights, sectors);
    const auto results = fincore::brinson_results(
        portfolio_returns, benchmark_returns, portfolio_weights,
        benchmark_weights, sectors);
    const auto cumulative = fincore::brinson_cumulative(
        portfolio_returns, benchmark_returns, portfolio_weights,
        benchmark_weights, sectors);

    EXPECT_NEAR(period.allocation, .004, 1.0e-12);
    EXPECT_NEAR(results.get_column<double>("total").at(0), .020, 1.0e-12);
    // With the single period the helper builds, compounding one return is the
    // same arithmetic as passing it through, so this pins the cumulative surface
    // and not the wealth path; a summed cumulative would read .080 as well.
    EXPECT_NEAR(cumulative.portfolio_cumulative, .080, 1.0e-12);
}

}  // namespace
