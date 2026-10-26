# Metrics

Return, risk, drawdown, distribution, rolling, yearly and performance-attribution statistics.

See the [usage guide](../guides/metrics.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::matrix
```

```{doxygenstruct} fincore::bootstrap_metric_stats
```

```{doxygenstruct} fincore::daily_consecutive_statistics
```

```{doxygenstruct} fincore::distribution_stats
```

```{doxygenstruct} fincore::drawdown_period
```

```{doxygenstruct} fincore::drawdown_table_row
```

```{doxygenstruct} fincore::drawdown_window
```

```{doxygenstruct} fincore::gpd_risk_estimates_result
```

```{doxygenstruct} fincore::named_returns
```

```{doxygenstruct} fincore::perf_attribution_result
```

```{doxygenstruct} fincore::perf_attribution_statistics
```

```{doxygenenum} fincore::frequency
```

```{doxygenenum} fincore::return_aggregation_period
```

```{doxygenfunction} fincore::adjust_returns(const dataframe &returns, double adjustment_factor, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::adjusted_sharpe_ratio(const dataframe &returns, double risk_free=0.0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::aggregate_returns(const dataframe &returns, return_aggregation_period period=return_aggregation_period::monthly, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::aligned_series(const dataframe &left, std::string_view left_column, const dataframe &right, std::string_view right_column)
```

```{doxygenfunction} fincore::aligned_series(const dataframe &left, const dataframe &right)
```

```{doxygenfunction} fincore::alpha(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::alpha_beta(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::alpha_percentile_rank(const dataframe &strategy_returns, std::span< const dataframe > peer_returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::annual_active_return(const dataframe &returns, const dataframe &benchmark, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::annual_active_return_by_year(const dataframe &returns, const dataframe &benchmark, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::annual_alpha(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::annual_beta(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::annual_return(const dataframe &returns, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::annual_return_by_year(const dataframe &returns, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::annual_volatility(const dataframe &returns, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::annual_volatility_by_year(const dataframe &returns, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::annualization_factor(frequency period, std::optional< double > annualization=std::nullopt)
```

```{doxygenfunction} fincore::beta(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::beta_fragility_heuristic(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::burke_ratio(const dataframe &returns, double risk_free=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cal_treynor_ratio(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::calc_distribution_stats(std::span< const double > values)
```

```{doxygenfunction} fincore::calmar_ratio(const dataframe &returns, double risk_free=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::capm_r_squared(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::capture(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::common_sense_ratio(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::compute_bayes_cone(const dataframe &predicted_returns, double starting_value=1.0)
```

```{doxygenfunction} fincore::compute_consistency_score(const dataframe &returns_test, const matrix &predicted_returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::compute_exposures(const dataframe &positions, const dataframe &factor_loadings, std::span< const std::string_view > factor_column_names, std::string_view cash_column_name=cash_column, std::string_view symbol_column_name=attribution_symbol_column)
```

```{doxygenfunction} fincore::conditional_sharpe_ratio(const dataframe &returns, double cutoff=0.05, double risk_free=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::conditional_value_at_risk(const dataframe &returns, double cutoff=0.05, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cone_bounds_column_name(double num_std)
```

```{doxygenfunction} fincore::consecutive_stats(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cornell_timing(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::create_perf_attrib_stats(const perf_attribution_result &result, double annualization=daily_annualization)
```

```{doxygenfunction} fincore::cumulative_return(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cumulative_return(const dataframe &returns, std::initializer_list< std::string_view > legacy_column)
```

```{doxygenfunction} fincore::cumulative_return(const dataframe &returns, double starting_value, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cumulative_returns(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cumulative_returns(const dataframe &returns, std::initializer_list< std::string_view > legacy_column)
```

```{doxygenfunction} fincore::cumulative_returns(const dataframe &returns, double starting_value, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::cumulative_returns_less_costs(const dataframe &returns, std::string_view returns_column_name=returns_column)
```

```{doxygenfunction} fincore::cumulative_returns_less_costs(const dataframe &returns, const dataframe &costs, std::string_view returns_column_name=returns_column, std::string_view costs_column_name=returns_column)
```

```{doxygenfunction} fincore::daily_consecutive_stats(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::deflated_sharpe_ratio(const dataframe &returns, double risk_free=0.0, int num_trials=1, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::down_alpha_beta(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::down_capture(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::down_capture_return(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::downside_risk(const dataframe &returns, double required_return=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::drawdown_series(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::ensure_datetime_index_series(const dataframe &frame, frequency period=frequency::daily)
```

```{doxygenfunction} fincore::ensure_datetime_index_series(std::span< const double > values, frequency period=frequency::daily, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::excess_sharpe(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::extract_interesting_date_ranges(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::flatten(const dataframe &frame)
```

```{doxygenfunction} fincore::flatten(const matrix &values)
```

```{doxygenfunction} fincore::forecast_cone_bootstrap(const dataframe &in_sample_returns, std::size_t num_days, std::span< const double > cone_std=default_cone_standard_deviations, double starting_value=1.0, std::size_t num_samples=1000U, std::uint32_t random_seed=0U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::forecast_cone_bootstrap(const dataframe &in_sample_returns, std::size_t num_days, double cone_std, double starting_value=1.0, std::size_t num_samples=1000U, std::uint32_t random_seed=0U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::frequency_name(frequency period)
```

```{doxygenfunction} fincore::gen_drawdown_table(const dataframe &returns, std::size_t top=10, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::get_all_drawdowns(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::get_all_drawdowns_detailed(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::get_top_drawdowns(const dataframe &returns, std::size_t top=10, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::gpd_risk_estimates(const dataframe &returns, double var_p=0.01, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::henriksson_merton_timing(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::historical_value_at_risk(const dataframe &returns, double cutoff=0.05, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::hurst_exponent(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::information_ratio(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::information_ratio_by_year(const dataframe &returns, const dataframe &benchmark, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::kappa_three_ratio(const dataframe &returns, double mar=0.0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::kurtosis(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::loss_rate(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::m_squared(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::mar_ratio(const dataframe &returns, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::market_timing_return(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::max_consecutive_down_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_down_end_date(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_down_months(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_down_start_date(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_down_weeks(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_gain(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_loss(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_up_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_up_end_date(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_up_months(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_up_start_date(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_consecutive_up_weeks(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_by_year(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_months(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_period(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_recovery_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_recovery_months(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_recovery_weeks(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_underwater(const dataframe &underwater, std::string_view column="drawdown")
```

```{doxygenfunction} fincore::max_drawdown_weeks(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_drawdown_window(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_single_day_gain(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_single_day_gain_date(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_single_day_loss(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::max_single_day_loss_date(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::normalize(const dataframe &values, double starting_value=1.0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::normalize_attribution_positions(const dataframe &positions, bool positions_in_dollars=true, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::omega_ratio(const dataframe &returns, double risk_free=0.0, double required_return=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::pandas_frequency(std::string_view period)
```

```{doxygenfunction} fincore::pandas_frequency_alias(frequency period)
```

```{doxygenfunction} fincore::perf_attrib(const dataframe &returns, const dataframe &positions, const dataframe &factor_returns, const dataframe &factor_loadings, bool positions_in_dollars=true, std::string_view returns_column_name=returns_column, std::string_view cash_column_name=cash_column, std::string_view symbol_column_name=attribution_symbol_column)
```

```{doxygenfunction} fincore::perf_attrib_core(const dataframe &returns, const dataframe &normalized_positions, const dataframe &factor_returns, const dataframe &factor_loadings, std::string_view returns_column_name=returns_column, std::string_view cash_column_name=cash_column, std::string_view symbol_column_name=attribution_symbol_column)
```

```{doxygenfunction} fincore::perf_stats(const dataframe &returns, frequency period=frequency::daily, std::string_view returns_column_name=returns_column)
```

```{doxygenfunction} fincore::perf_stats(const dataframe &returns, const dataframe &benchmark, frequency period=frequency::daily, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::perf_stats_bootstrap(const dataframe &returns, std::size_t n_samples=1000, std::uint64_t seed=0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::perf_stats_bootstrap(const dataframe &returns, std::span< const std::vector< std::size_t > > draw_indices, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::r_cubed(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::r_cubed_turtle(const dataframe &returns, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::relative_win_rate(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::residual_risk(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::roll_alpha(const dataframe &returns, const dataframe &benchmark, std::size_t window=252U, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::roll_alpha_beta(const dataframe &returns, const dataframe &benchmark, std::size_t window=252U, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::roll_annual_volatility(const dataframe &returns, std::size_t window, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::roll_beta(const dataframe &returns, const dataframe &benchmark, std::size_t window=252U, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::roll_down_capture(const dataframe &returns, const dataframe &benchmark, std::size_t window=252U, frequency period=frequency::daily, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::roll_max_drawdown(const dataframe &returns, std::size_t window=252U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::roll_sharpe_ratio(const dataframe &returns, std::size_t window=252U, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::roll_sortino_ratio(const dataframe &returns, std::size_t window, double required_return=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::roll_up_capture(const dataframe &returns, const dataframe &benchmark, std::size_t window=252U, frequency period=frequency::daily, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::roll_up_down_capture(const dataframe &returns, const dataframe &benchmark, std::size_t window=252U, frequency period=frequency::daily, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::rolling_beta(const dataframe &returns, const dataframe &factor_returns, std::size_t rolling_window=126U, std::string_view returns_column_name=returns_column, std::string_view factor_column_name=benchmark_column)
```

```{doxygenfunction} fincore::rolling_beta(const dataframe &returns, const dataframe &factor_returns, std::span< const std::string_view > factor_column_names, std::size_t rolling_window=126U, std::string_view returns_column_name=returns_column)
```

```{doxygenfunction} fincore::rolling_regression(const dataframe &returns, const dataframe &benchmark, std::size_t rolling_window=126U, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::rolling_sharpe(const dataframe &returns, std::size_t window, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::rolling_volatility(const dataframe &returns, std::size_t window, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::safe_correlation(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::second_max_drawdown(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::second_max_drawdown_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::second_max_drawdown_recovery_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::serial_correlation(const dataframe &returns, std::ptrdiff_t lag=1, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::sharpe_ratio(const dataframe &returns, double risk_free=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::sharpe_ratio_by_year(const dataframe &returns, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::simple_returns(const dataframe &prices, std::string_view price_column)
```

```{doxygenfunction} fincore::simulate_paths(std::span< const double > in_sample_returns, std::size_t num_days, double starting_value=1.0, std::size_t num_samples=1000U, std::uint32_t random_seed=0U)
```

```{doxygenfunction} fincore::simulate_paths(const dataframe &in_sample_returns, std::size_t num_days, double starting_value=1.0, std::size_t num_samples=1000U, std::uint32_t random_seed=0U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::skewness(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::sortino_ratio(const dataframe &returns, double required_return=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::stability_of_timeseries(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::sterling_ratio(const dataframe &returns, double risk_free=0.0, double annualization=daily_annualization, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::stutzer_index(const dataframe &returns, double target_return=0.0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::summarize_paths(const matrix &samples, std::span< const double > cone_std=default_cone_standard_deviations, double starting_value=1.0)
```

```{doxygenfunction} fincore::summarize_paths(const matrix &samples, double cone_std, double starting_value=1.0)
```

```{doxygenfunction} fincore::tail_ratio(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::third_max_drawdown(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::third_max_drawdown_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::third_max_drawdown_recovery_days(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::to_dataframe(std::span< const double > values, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::to_dataframe(const matrix &values, std::string_view column_prefix="column")
```

```{doxygenfunction} fincore::to_dataframe(const dataframe &frame)
```

```{doxygenfunction} fincore::tracking_difference(const dataframe &returns, const dataframe &benchmark, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::tracking_error(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::trading_value_at_risk(const dataframe &returns, double sigma=2.0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::treynor_mazuy_timing(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::up_alpha_beta(const dataframe &returns, const dataframe &benchmark, double risk_free=0.0, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::up_capture(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::up_capture_return(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::up_down_capture(const dataframe &returns, const dataframe &benchmark, double annualization=daily_annualization, std::string_view returns_column_name=returns_column, std::string_view benchmark_column_name=benchmark_column)
```

```{doxygenfunction} fincore::var_cov_var_normal(double portfolio_value, double confidence, double mean=0.0, double standard_deviation=1.0)
```

```{doxygenfunction} fincore::var_excess_return(const dataframe &returns, double cutoff=0.05, double risk_free=0.0, frequency period=frequency::daily, std::optional< double > annualization=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::win_rate(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenvariable} fincore::attribution_common_returns_column
```

```{doxygenvariable} fincore::attribution_factor_column
```

```{doxygenvariable} fincore::attribution_specific_returns_column
```

```{doxygenvariable} fincore::attribution_symbol_column
```

```{doxygenvariable} fincore::attribution_tilt_returns_column
```

```{doxygenvariable} fincore::attribution_timing_returns_column
```

```{doxygenvariable} fincore::attribution_total_returns_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_1_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_25_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_50_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_5_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_75_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_95_column
```

```{doxygenvariable} fincore::bayesian_cone_percentile_99_column
```

```{doxygenvariable} fincore::benchmark_column
```

```{doxygenvariable} fincore::daily_annualization
```

```{doxygenvariable} fincore::daily_frequency_name
```

```{doxygenvariable} fincore::default_cone_standard_deviations
```

```{doxygenvariable} fincore::monthly_frequency_name
```

```{doxygenvariable} fincore::quarterly_frequency_name
```

```{doxygenvariable} fincore::return_aggregation_period_column
```

```{doxygenvariable} fincore::return_aggregation_year_column
```

```{doxygenvariable} fincore::returns_column
```

```{doxygenvariable} fincore::weekly_frequency_name
```

```{doxygenvariable} fincore::yearly_frequency_name
```
