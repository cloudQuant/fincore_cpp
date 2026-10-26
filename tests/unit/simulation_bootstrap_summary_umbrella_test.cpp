#include <fincore/simulation.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>
#include <type_traits>

namespace {

using bootstrap_summary_umbrella_function =
    fincore::bootstrap_summary_result (*)(const fincore::dataframe &,
                                          std::size_t, double,
                                          std::optional<std::uint64_t>,
                                          std::string_view);

static_assert(std::is_same_v<decltype(&fincore::bootstrap_summary),
                             bootstrap_summary_umbrella_function>);

TEST(SimulationBootstrapSummaryUmbrella, FacadeExportsSummaryContract) {
    static_assert(std::is_same_v<
                  decltype(fincore::bootstrap_summary_result::sortino_ratio),
                  fincore::bootstrap_summary_statistic>);

    const auto returns = fincore::make_series({9, -4, 9, 2}, "returns",
                                              {-0.04, -0.03, -0.02, -0.01});
    const auto result = fincore::bootstrap_summary(returns, 17U, 0.2, 1U);
    EXPECT_TRUE(std::isfinite(result.mean.value));
    EXPECT_TRUE(std::isfinite(result.sortino_ratio.confidence_interval.upper));
}

} // namespace
