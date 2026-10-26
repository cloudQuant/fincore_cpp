#include <fincore/operations.hpp>

#include <array>
#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace fincore {
namespace {

// attribution (17 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 17> attribution_table{{
    operation_descriptor{"attribution.brinson.brinson_attribution"},
    operation_descriptor{"attribution.brinson.brinson_cumulative"},
    operation_descriptor{"attribution.brinson.brinson_results"},
    operation_descriptor{"attribution.fama_french.calculate_idiosyncratic_risk"},
    operation_descriptor{"attribution.fama_french.fetch_ff_factors"},
    operation_descriptor{"attribution.performance.align_and_warn"},
    operation_descriptor{"attribution.performance.compute_exposures"},
    operation_descriptor{"attribution.performance.compute_exposures_internal"},
    operation_descriptor{"attribution.performance.create_perf_attrib_stats"},
    operation_descriptor{"attribution.performance.cumulative_returns_less_costs"},
    operation_descriptor{"attribution.performance.perf_attrib"},
    operation_descriptor{"attribution.performance.perf_attrib_core"},
    operation_descriptor{"attribution.style.analyze_performance_by_style"},
    operation_descriptor{"attribution.style.calculate_regression_attribution"},
    operation_descriptor{"attribution.style.calculate_style_tilts"},
    operation_descriptor{"attribution.style.fetch_style_factors"},
    operation_descriptor{"attribution.style.style_analysis"},
}};

// optimization (3 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 3> optimization_table{{
    operation_descriptor{"optimization.frontier.efficient_frontier"},
    operation_descriptor{"optimization.objectives.optimize"},
    operation_descriptor{"optimization.risk_parity.risk_parity"},
}};

// performance (9 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 9> performance_table{{
    operation_descriptor{"performance.cashflows.cashflow_adjusted_returns"},
    operation_descriptor{"performance.cashflows.cashflow_adjusted_twr"},
    operation_descriptor{"performance.disclosures.render_disclosure"},
    operation_descriptor{"performance.inference.sharpe_confidence_interval"},
    operation_descriptor{"performance.inference.sharpe_standard_error"},
    operation_descriptor{"performance.inference.standard_error_of_mean"},
    operation_descriptor{"performance.returns.mwr"},
    operation_descriptor{"performance.returns.twr"},
    operation_descriptor{"performance.returns.xirr"},
}};

// portfolio (30 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 30> portfolio_table{{
    operation_descriptor{"portfolio.capacity.assess_liquidity"},
    operation_descriptor{"portfolio.positions.compute_cap_exposures"},
    operation_descriptor{"portfolio.positions.compute_sector_exposures"},
    operation_descriptor{"portfolio.positions.compute_style_factor_exposures"},
    operation_descriptor{"portfolio.positions.compute_volume_exposures"},
    operation_descriptor{"portfolio.positions.extract_pos"},
    operation_descriptor{"portfolio.positions.get_long_short_notional"},
    operation_descriptor{"portfolio.positions.get_long_short_pos"},
    operation_descriptor{"portfolio.positions.get_max_median_position_concentration"},
    operation_descriptor{"portfolio.positions.get_percent_alloc"},
    operation_descriptor{"portfolio.positions.get_sector_exposures"},
    operation_descriptor{"portfolio.positions.get_top_long_short_abs"},
    operation_descriptor{"portfolio.positions.gross_lev"},
    operation_descriptor{"portfolio.positions.stack_positions"},
    operation_descriptor{"portfolio.round_trips.add_closing_transactions"},
    operation_descriptor{"portfolio.round_trips.agg_all_long_short"},
    operation_descriptor{"portfolio.round_trips.apply_sector_mappings_to_round_trips"},
    operation_descriptor{"portfolio.round_trips.extract_round_trips"},
    operation_descriptor{"portfolio.round_trips.gen_round_trip_stats"},
    operation_descriptor{"portfolio.round_trips.groupby_consecutive"},
    operation_descriptor{"portfolio.transactions.adjust_returns_for_slippage"},
    operation_descriptor{"portfolio.transactions.apply_slippage_penalty"},
    operation_descriptor{"portfolio.transactions.daily_txns_with_bar_data"},
    operation_descriptor{"portfolio.transactions.days_to_liquidate_positions"},
    operation_descriptor{"portfolio.transactions.get_low_liquidity_transactions"},
    operation_descriptor{"portfolio.transactions.get_max_days_to_liquidate_by_ticker"},
    operation_descriptor{"portfolio.transactions.get_turnover"},
    operation_descriptor{"portfolio.transactions.get_txn_vol"},
    operation_descriptor{"portfolio.transactions.make_transaction_frame"},
    operation_descriptor{"portfolio.transactions.map_transaction"},
}};

// report (3 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 3> report_table{{
    operation_descriptor{"report.factor.build_factor_report"},
    operation_descriptor{"report.portfolio.build_portfolio_report"},
    operation_descriptor{"report.risk.build_risk_report"},
}};

// risk (20 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 20> risk_table{{
    operation_descriptor{"risk.backtesting.backtest_es"},
    operation_descriptor{"risk.backtesting.backtest_var"},
    operation_descriptor{"risk.backtesting.christoffersen_lr"},
    operation_descriptor{"risk.backtesting.kupiec_lr"},
    operation_descriptor{"risk.calibration.basel_traffic_light"},
    operation_descriptor{"risk.calibration.es_calibration_score"},
    operation_descriptor{"risk.calibration.expected_exception_count"},
    operation_descriptor{"risk.diagnostics.walk_forward_var"},
    operation_descriptor{"risk.evt.evt_cvar"},
    operation_descriptor{"risk.evt.evt_var"},
    operation_descriptor{"risk.evt.extreme_risk"},
    operation_descriptor{"risk.evt.gev_fit"},
    operation_descriptor{"risk.evt.gpd_fit"},
    operation_descriptor{"risk.evt.hill_estimator"},
    operation_descriptor{"risk.garch.conditional_es"},
    operation_descriptor{"risk.garch.conditional_var"},
    operation_descriptor{"risk.garch.forecast_volatility"},
    operation_descriptor{"risk.models.forecast_es"},
    operation_descriptor{"risk.models.forecast_var"},
    operation_descriptor{"risk.report.build_risk_validation_report"},
}};

// simulation (15 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 15> simulation_table{{
    operation_descriptor{"simulation.base.annualize"},
    operation_descriptor{"simulation.base.compute_statistics"},
    operation_descriptor{"simulation.base.estimate_parameters"},
    operation_descriptor{"simulation.base.validate_returns"},
    operation_descriptor{"simulation.bootstrap.bootstrap", false, "explicit_seed"},
    operation_descriptor{"simulation.bootstrap.bootstrap_ci", false, "explicit_seed"},
    operation_descriptor{"simulation.bootstrap.bootstrap_summary", false, "explicit_seed"},
    operation_descriptor{"simulation.monte_carlo.MonteCarlo.from_parameters", false, "explicit_seed"},
    operation_descriptor{"simulation.paths.antithetic_variates"},
    operation_descriptor{"simulation.paths.gbm_from_returns", false, "explicit_seed_or_generator"},
    operation_descriptor{"simulation.paths.geometric_brownian_motion", false, "explicit_seed_or_generator"},
    operation_descriptor{"simulation.paths.latin_hypercube_sampling", false, "explicit_seed_or_generator"},
    operation_descriptor{"simulation.scenarios.generate_correlation_breakdown"},
    operation_descriptor{"simulation.scenarios.scenario_table"},
    operation_descriptor{"simulation.scenarios.stress_test"},
}};

// data (4 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 4> data_table{{
    operation_descriptor{"data.providers.fetch_multiple_prices"},
    operation_descriptor{"data.providers.fetch_price_data"},
    operation_descriptor{"data.providers.get_provider"},
    operation_descriptor{"data.snapshots.DataSnapshot.from_frame"},
}};

// factor_analysis (41 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 41> factor_analysis_table{{
    operation_descriptor{"factor_analysis.analysis.analyze_factor"},
    operation_descriptor{"factor_analysis.calendar.add_custom_calendar_timedelta"},
    operation_descriptor{"factor_analysis.calendar.backshift_returns_series"},
    operation_descriptor{"factor_analysis.calendar.diff_custom_calendar_timedeltas"},
    operation_descriptor{"factor_analysis.calendar.get_forward_returns_columns"},
    operation_descriptor{"factor_analysis.calendar.infer_trading_calendar"},
    operation_descriptor{"factor_analysis.calendar.timedelta_strings_to_integers"},
    operation_descriptor{"factor_analysis.calendar.timedelta_to_string"},
    operation_descriptor{"factor_analysis.costs.apply_factor_costs"},
    operation_descriptor{"factor_analysis.costs.estimate_factor_capacity"},
    operation_descriptor{"factor_analysis.data.compute_forward_returns"},
    operation_descriptor{"factor_analysis.data.prepare_factor_data"},
    operation_descriptor{"factor_analysis.data.prepare_factor_data_by_horizon"},
    operation_descriptor{"factor_analysis.data.prepare_factor_data_from_forward_returns"},
    operation_descriptor{"factor_analysis.data.prepare_pit_factor_data"},
    operation_descriptor{"factor_analysis.data.quantize_factor"},
    operation_descriptor{"factor_analysis.inference.benjamini_hochberg"},
    operation_descriptor{"factor_analysis.inference.factor_model_inference"},
    operation_descriptor{"factor_analysis.inference.fama_macbeth"},
    operation_descriptor{"factor_analysis.inference.ic_confidence_interval"},
    operation_descriptor{"factor_analysis.inference.ic_mean"},
    operation_descriptor{"factor_analysis.inference.ic_t_stat"},
    operation_descriptor{"factor_analysis.inference.information_coefficient_inference"},
    operation_descriptor{"factor_analysis.performance.average_cumulative_return_by_quantile"},
    operation_descriptor{"factor_analysis.performance.common_start_returns"},
    operation_descriptor{"factor_analysis.performance.compute_mean_returns_spread"},
    operation_descriptor{"factor_analysis.performance.cumulative_returns"},
    operation_descriptor{"factor_analysis.performance.factor_alpha_beta"},
    operation_descriptor{"factor_analysis.performance.factor_information_coefficient"},
    operation_descriptor{"factor_analysis.performance.factor_rank_autocorrelation"},
    operation_descriptor{"factor_analysis.performance.factor_returns"},
    operation_descriptor{"factor_analysis.performance.factor_weights"},
    operation_descriptor{"factor_analysis.performance.mean_information_coefficient"},
    operation_descriptor{"factor_analysis.performance.mean_return_by_quantile"},
    operation_descriptor{"factor_analysis.performance.quantile_turnover"},
    operation_descriptor{"factor_analysis.pit.materialize_pit_factor"},
    operation_descriptor{"factor_analysis.pit.validate_pit_alignment"},
    operation_descriptor{"factor_analysis.portfolio.build_factor_portfolio_inputs"},
    operation_descriptor{"factor_analysis.portfolio.factor_cumulative_returns"},
    operation_descriptor{"factor_analysis.portfolio.factor_positions"},
    operation_descriptor{"factor_analysis.portfolio.positions"},
}};

// metrics (131 operations), source order preserved.
inline constexpr std::array<operation_descriptor, 131> metrics_table{{
    operation_descriptor{"metrics.alpha_beta.alpha"},
    operation_descriptor{"metrics.alpha_beta.alpha_aligned"},
    operation_descriptor{"metrics.alpha_beta.alpha_beta"},
    operation_descriptor{"metrics.alpha_beta.alpha_beta_aligned"},
    operation_descriptor{"metrics.alpha_beta.alpha_percentile_rank"},
    operation_descriptor{"metrics.alpha_beta.annual_alpha"},
    operation_descriptor{"metrics.alpha_beta.annual_beta"},
    operation_descriptor{"metrics.alpha_beta.beta"},
    operation_descriptor{"metrics.alpha_beta.beta_aligned"},
    operation_descriptor{"metrics.alpha_beta.down_alpha_beta"},
    operation_descriptor{"metrics.alpha_beta.up_alpha_beta"},
    operation_descriptor{"metrics.consecutive.consecutive_stats"},
    operation_descriptor{"metrics.consecutive.max_consecutive_down_days"},
    operation_descriptor{"metrics.consecutive.max_consecutive_down_end_date"},
    operation_descriptor{"metrics.consecutive.max_consecutive_down_months"},
    operation_descriptor{"metrics.consecutive.max_consecutive_down_start_date"},
    operation_descriptor{"metrics.consecutive.max_consecutive_down_weeks"},
    operation_descriptor{"metrics.consecutive.max_consecutive_gain"},
    operation_descriptor{"metrics.consecutive.max_consecutive_loss"},
    operation_descriptor{"metrics.consecutive.max_consecutive_up_days"},
    operation_descriptor{"metrics.consecutive.max_consecutive_up_end_date"},
    operation_descriptor{"metrics.consecutive.max_consecutive_up_months"},
    operation_descriptor{"metrics.consecutive.max_consecutive_up_start_date"},
    operation_descriptor{"metrics.consecutive.max_consecutive_up_weeks"},
    operation_descriptor{"metrics.consecutive.max_single_day_gain"},
    operation_descriptor{"metrics.consecutive.max_single_day_gain_date"},
    operation_descriptor{"metrics.consecutive.max_single_day_loss"},
    operation_descriptor{"metrics.consecutive.max_single_day_loss_date"},
    operation_descriptor{"metrics.drawdown.gen_drawdown_table"},
    operation_descriptor{"metrics.drawdown.get_all_drawdowns"},
    operation_descriptor{"metrics.drawdown.get_all_drawdowns_detailed"},
    operation_descriptor{"metrics.drawdown.get_max_drawdown"},
    operation_descriptor{"metrics.drawdown.get_max_drawdown_period"},
    operation_descriptor{"metrics.drawdown.get_max_drawdown_underwater"},
    operation_descriptor{"metrics.drawdown.get_top_drawdowns"},
    operation_descriptor{"metrics.drawdown.max_drawdown"},
    operation_descriptor{"metrics.drawdown.max_drawdown_days"},
    operation_descriptor{"metrics.drawdown.max_drawdown_months"},
    operation_descriptor{"metrics.drawdown.max_drawdown_recovery_days"},
    operation_descriptor{"metrics.drawdown.max_drawdown_recovery_months"},
    operation_descriptor{"metrics.drawdown.max_drawdown_recovery_weeks"},
    operation_descriptor{"metrics.drawdown.max_drawdown_weeks"},
    operation_descriptor{"metrics.drawdown.second_max_drawdown"},
    operation_descriptor{"metrics.drawdown.second_max_drawdown_days"},
    operation_descriptor{"metrics.drawdown.second_max_drawdown_recovery_days"},
    operation_descriptor{"metrics.drawdown.third_max_drawdown"},
    operation_descriptor{"metrics.drawdown.third_max_drawdown_days"},
    operation_descriptor{"metrics.drawdown.third_max_drawdown_recovery_days"},
    operation_descriptor{"metrics.ratios.adjusted_sharpe_ratio"},
    operation_descriptor{"metrics.ratios.burke_ratio"},
    operation_descriptor{"metrics.ratios.cal_treynor_ratio"},
    operation_descriptor{"metrics.ratios.calmar_ratio"},
    operation_descriptor{"metrics.ratios.capture"},
    operation_descriptor{"metrics.ratios.common_sense_ratio"},
    operation_descriptor{"metrics.ratios.conditional_sharpe_ratio"},
    operation_descriptor{"metrics.ratios.down_capture"},
    operation_descriptor{"metrics.ratios.down_capture_return"},
    operation_descriptor{"metrics.ratios.excess_sharpe"},
    operation_descriptor{"metrics.ratios.information_ratio"},
    operation_descriptor{"metrics.ratios.kappa_three_ratio"},
    operation_descriptor{"metrics.ratios.m_squared"},
    operation_descriptor{"metrics.ratios.mar_ratio"},
    operation_descriptor{"metrics.ratios.omega_ratio"},
    operation_descriptor{"metrics.ratios.sharpe_ratio"},
    operation_descriptor{"metrics.ratios.sortino_ratio"},
    operation_descriptor{"metrics.ratios.stability_of_timeseries"},
    operation_descriptor{"metrics.ratios.sterling_ratio"},
    operation_descriptor{"metrics.ratios.treynor_ratio"},
    operation_descriptor{"metrics.ratios.up_capture"},
    operation_descriptor{"metrics.ratios.up_capture_return"},
    operation_descriptor{"metrics.ratios.up_down_capture"},
    operation_descriptor{"metrics.returns.aggregate_returns"},
    operation_descriptor{"metrics.returns.cum_returns"},
    operation_descriptor{"metrics.returns.cum_returns_final"},
    operation_descriptor{"metrics.returns.normalize"},
    operation_descriptor{"metrics.returns.simple_returns"},
    operation_descriptor{"metrics.risk.annual_volatility"},
    operation_descriptor{"metrics.risk.beta_fragility_heuristic"},
    operation_descriptor{"metrics.risk.beta_fragility_heuristic_aligned"},
    operation_descriptor{"metrics.risk.conditional_value_at_risk"},
    operation_descriptor{"metrics.risk.downside_risk"},
    operation_descriptor{"metrics.risk.residual_risk"},
    operation_descriptor{"metrics.risk.tail_ratio"},
    operation_descriptor{"metrics.risk.tracking_error"},
    operation_descriptor{"metrics.risk.value_at_risk"},
    operation_descriptor{"metrics.risk.var_excess_return"},
    operation_descriptor{"metrics.rolling.roll_alpha"},
    operation_descriptor{"metrics.rolling.roll_alpha_aligned"},
    operation_descriptor{"metrics.rolling.roll_alpha_beta"},
    operation_descriptor{"metrics.rolling.roll_alpha_beta_aligned"},
    operation_descriptor{"metrics.rolling.roll_annual_volatility"},
    operation_descriptor{"metrics.rolling.roll_beta"},
    operation_descriptor{"metrics.rolling.roll_beta_aligned"},
    operation_descriptor{"metrics.rolling.roll_down_capture"},
    operation_descriptor{"metrics.rolling.roll_max_drawdown"},
    operation_descriptor{"metrics.rolling.roll_sharpe_ratio"},
    operation_descriptor{"metrics.rolling.roll_sortino_ratio"},
    operation_descriptor{"metrics.rolling.roll_up_capture"},
    operation_descriptor{"metrics.rolling.roll_up_down_capture"},
    operation_descriptor{"metrics.rolling.rolling_beta"},
    operation_descriptor{"metrics.rolling.rolling_regression"},
    operation_descriptor{"metrics.rolling.rolling_sharpe"},
    operation_descriptor{"metrics.rolling.rolling_volatility"},
    operation_descriptor{"metrics.stats.bond_market_correlation"},
    operation_descriptor{"metrics.stats.capm_r_squared"},
    operation_descriptor{"metrics.stats.futures_market_correlation"},
    operation_descriptor{"metrics.stats.hurst_exponent"},
    operation_descriptor{"metrics.stats.kurtosis"},
    operation_descriptor{"metrics.stats.loss_rate"},
    operation_descriptor{"metrics.stats.r_cubed"},
    operation_descriptor{"metrics.stats.r_cubed_turtle"},
    operation_descriptor{"metrics.stats.relative_win_rate"},
    operation_descriptor{"metrics.stats.serial_correlation"},
    operation_descriptor{"metrics.stats.skewness"},
    operation_descriptor{"metrics.stats.stock_market_correlation"},
    operation_descriptor{"metrics.stats.stutzer_index"},
    operation_descriptor{"metrics.stats.tracking_difference"},
    operation_descriptor{"metrics.stats.win_rate"},
    operation_descriptor{"metrics.timing.cornell_timing"},
    operation_descriptor{"metrics.timing.extract_interesting_date_ranges"},
    operation_descriptor{"metrics.timing.henriksson_merton_timing"},
    operation_descriptor{"metrics.timing.market_timing_return"},
    operation_descriptor{"metrics.timing.treynor_mazuy_timing"},
    operation_descriptor{"metrics.yearly.annual_active_return"},
    operation_descriptor{"metrics.yearly.annual_active_return_by_year"},
    operation_descriptor{"metrics.yearly.annual_return"},
    operation_descriptor{"metrics.yearly.annual_return_by_year"},
    operation_descriptor{"metrics.yearly.annual_volatility_by_year"},
    operation_descriptor{"metrics.yearly.information_ratio_by_year"},
    operation_descriptor{"metrics.yearly.max_drawdown_by_year"},
    operation_descriptor{"metrics.yearly.sharpe_ratio_by_year"},
}};

}  // namespace

std::span<const operation_descriptor> attribution_operations() {
    return attribution_table;
}

std::span<const operation_descriptor> optimization_operations() {
    return optimization_table;
}

std::span<const operation_descriptor> performance_operations() {
    return performance_table;
}

std::span<const operation_descriptor> portfolio_operations() {
    return portfolio_table;
}

std::span<const operation_descriptor> report_operations() {
    return report_table;
}

std::span<const operation_descriptor> risk_operations() {
    return risk_table;
}

std::span<const operation_descriptor> simulation_operations() {
    return simulation_table;
}

std::span<const operation_descriptor> data_operations() {
    return data_table;
}

std::span<const operation_descriptor> factor_analysis_operations() {
    return factor_analysis_table;
}

std::span<const operation_descriptor> metrics_operations() {
    return metrics_table;
}

// The concatenation order is part of what the catalog exposes: the domain tables
// are appended in this fixed sequence rather than alphabetically, so a digest or
// golden enumeration taken from it shifts if a domain is moved here even though no
// operation was added or removed. Built once on first use.
std::span<const operation_descriptor> all_operations() {
    static const std::vector<operation_descriptor> operations = [] {
        std::vector<operation_descriptor> result;
        result.insert(result.end(), attribution_table.begin(), attribution_table.end());
        result.insert(result.end(), optimization_table.begin(), optimization_table.end());
        result.insert(result.end(), performance_table.begin(), performance_table.end());
        result.insert(result.end(), portfolio_table.begin(), portfolio_table.end());
        result.insert(result.end(), report_table.begin(), report_table.end());
        result.insert(result.end(), risk_table.begin(), risk_table.end());
        result.insert(result.end(), simulation_table.begin(), simulation_table.end());
        result.insert(result.end(), data_table.begin(), data_table.end());
        result.insert(result.end(), factor_analysis_table.begin(), factor_analysis_table.end());
        result.insert(result.end(), metrics_table.begin(), metrics_table.end());
        return result;
    }();
    return operations;
}

}  // namespace fincore
