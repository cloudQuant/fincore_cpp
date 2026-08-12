#pragma once

#include <fincore/simulation/paths.hpp>
#include <fincore/simulation/result.hpp>

#include <cstddef>
#include <cstdint>
#include <optional>
#include <string_view>

namespace fincore {

[[nodiscard]] simulation_result simulate_monte_carlo(
    const dataframe& returns,
    std::size_t path_count = 1000U,
    std::size_t horizon = 252U,
    bool antithetic = false,
    std::optional<std::uint64_t> seed = std::nullopt,
    std::string_view column = returns_column);

}  // namespace fincore
