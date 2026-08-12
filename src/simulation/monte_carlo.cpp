#include <fincore/simulation/monte_carlo.hpp>

#include <cmath>

namespace fincore {

simulation_result simulate_monte_carlo(
    const dataframe& returns,
    const std::size_t path_count,
    const std::size_t horizon,
    const bool antithetic,
    const std::optional<std::uint64_t> seed,
    const std::string_view column) {
    dataframe paths = gbm_from_returns(
        returns, horizon, path_count, 252U, seed, column);
    if (antithetic) {
        paths = antithetic_variates(paths);
    }
    return analyze_simulation_paths(paths);
}

simulation_result simulate_monte_carlo_from_parameters(
    const double annualized_drift,
    const double annualized_volatility,
    const double initial_value,
    const std::size_t path_count,
    const std::size_t horizon,
    const std::optional<std::uint64_t> seed) {
    return analyze_simulation_paths(geometric_brownian_motion(
        initial_value,
        annualized_drift / 252.0,
        annualized_volatility / std::sqrt(252.0),
        static_cast<double>(horizon) / 252.0,
        1.0 / 252.0,
        path_count,
        seed));
}

}  // namespace fincore
