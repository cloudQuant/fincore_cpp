#pragma once

#include <fincore/dataframe.hpp>

#include <cstddef>
#include <optional>
#include <string>
#include <vector>

namespace fincore {

/// Risk-parity weights and their risk contributions.
struct risk_parity_result {
    /// Asset weights in `asset_names` order, normalized to sum to 1.
    std::vector<double> weights;
    /// `w_i * (Sigma w)_i / volatility` per asset; the values sum to the
    /// portfolio volatility.
    std::vector<double> risk_contributions;
    /// Annualized portfolio volatility, using 252 periods per year.
    double volatility = 0.0;
    /// Asset names, in returns column order.
    std::vector<std::string> asset_names;
};

/// Computes risk-parity weights with equal or user-supplied risk budgets.
///
/// The solver minimizes squared deviations between realized risk-contribution
/// shares and the normalized risk budget, then renormalizes the weights to sum
/// to 1. Returns and volatility use a fixed 252-period year. Weights are
/// bounded to `[1e-6, 1.0]` during the solve.
///
/// @param returns Daily simple returns with one finite double column per asset
///     and at least two observations.
/// @param risk_budget Optional per-asset budget with finite, non-negative
///     entries and a positive sum; it is normalized internally. A uniform
///     budget is used when omitted.
/// @param max_iter Maximum projected-gradient iterations per penalty stage;
///     must be at least 1.
/// @return Risk-parity weights, risk contributions, annualized volatility and
///     asset names.
/// @throws input_error If `returns` is empty, too short or non-finite,
///     `max_iter` is zero, or the risk budget is malformed.
/// @throws optimization_error If the solved weights cannot be normalized to a
///     positive sum.
risk_parity_result risk_parity(
    const dataframe& returns,
    std::optional<std::vector<double>> risk_budget = std::nullopt,
    std::size_t max_iter = 1000U);

}  // namespace fincore
