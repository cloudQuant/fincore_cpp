#include <fincore/portfolio/capacity.hpp>

namespace fincore {
namespace {

void validate_config(const capacity_config& config) {
    if (!(config.max_bar_consumption > 0.0 &&
          config.max_bar_consumption <= 1.0)) {
        throw input_error{"max_bar_consumption must be in (0, 1]"};
    }
    if (!(config.capital_base > 0.0)) {
        throw input_error{"capital_base must be positive"};
    }
    if (config.mean_volume_window < 1U) {
        throw input_error{"mean_volume_window must be positive"};
    }
    if (config.last_n_days.has_value() && *config.last_n_days < 1U) {
        throw input_error{"last_n_days must be positive when provided"};
    }
}

}  // namespace

capacity_assessment assess_liquidity(
    const dataframe& positions, const std::span<const trade_record> transactions,
    const dataframe& prices, const dataframe& volumes,
    const capacity_config& config) {
    validate_config(config);
    // The three views are computed independently over the same inputs, and
    // last_n_days reaches only the per-ticker and transaction ones: the headline
    // days-to-liquidate figure always uses the full history, so a trimmed window
    // narrows two of the three answers and not the first.
    return {
        days_to_liquidate_positions(positions, prices, volumes,
                                    config.max_bar_consumption,
                                    config.capital_base,
                                    config.mean_volume_window),
        get_max_days_to_liquidate_by_ticker(
            positions, prices, volumes, config.max_bar_consumption,
            config.capital_base, config.mean_volume_window,
            config.last_n_days),
        get_low_liquidity_transactions(transactions, prices, volumes,
                                       config.last_n_days)};
}

}  // namespace fincore
