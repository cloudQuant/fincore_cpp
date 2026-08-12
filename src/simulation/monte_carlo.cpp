#include <fincore/simulation/monte_carlo.hpp>

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

}  // namespace fincore
