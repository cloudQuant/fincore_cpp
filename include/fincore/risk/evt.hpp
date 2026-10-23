#pragma once

#include <fincore/metrics/returns.hpp>

#include <cstddef>
#include <optional>
#include <string_view>
#include <vector>

namespace fincore {

enum class evt_tail { lower, upper };
enum class gpd_estimation_method { mle, pwm };
enum class evt_model { gpd, gev };

struct hill_estimate {
    double xi;
    std::vector<double> tail_observations;
};

struct gpd_parameters {
    double xi;
    double beta;
    double threshold;
    std::size_t n_exceed;
};

hill_estimate hill_estimator(const dataframe& data,
                             std::optional<double> threshold = std::nullopt,
                             evt_tail tail = evt_tail::upper,
                             std::string_view column = returns_column);

gpd_parameters gpd_fit(const dataframe& data,
                       std::optional<double> threshold = std::nullopt,
                       gpd_estimation_method method = gpd_estimation_method::mle,
                       evt_tail tail = evt_tail::lower,
                       std::string_view column = returns_column);

double evt_var(const dataframe& data, double alpha = 0.05,
               evt_model model = evt_model::gpd,
               evt_tail tail = evt_tail::lower,
               std::optional<double> threshold = std::nullopt,
               std::optional<std::size_t> block_size = std::nullopt,
               std::string_view column = returns_column);

double evt_cvar(const dataframe& data, double alpha = 0.05,
                evt_model model = evt_model::gpd,
                evt_tail tail = evt_tail::lower,
                std::optional<double> threshold = std::nullopt,
                std::optional<std::size_t> block_size = std::nullopt,
                std::string_view column = returns_column);

struct extreme_risk_result {
    double var;
    double cvar;
    double tail_index;
    double threshold;
    std::size_t n_exceedances;
    double alpha;
};

extreme_risk_result extreme_risk(const dataframe& returns, double alpha = 0.05,
                                 evt_tail tail = evt_tail::lower,
                                 evt_model model = evt_model::gpd,
                                 std::optional<double> threshold = std::nullopt,
                                 std::optional<std::size_t> block_size = std::nullopt,
                                 std::string_view column = returns_column);

}  // namespace fincore
