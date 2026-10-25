# Simulation

Bootstrap, Monte Carlo and stress scenarios with seeded reproducibility.

See the [usage guide](../guides/portfolio-risk.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::simulation_result
```

```{doxygenstruct} fincore::bootstrap_confidence_interval_result
```

```{doxygenstruct} fincore::bootstrap_summary_result
```

```{doxygenstruct} fincore::bootstrap_summary_statistic
```

```{doxygenstruct} fincore::custom_stress_scenario
```

```{doxygenstruct} fincore::simulation_parameters
```

```{doxygenstruct} fincore::simulation_terminal_percentiles
```

```{doxygenstruct} fincore::simulation_terminal_statistics
```

```{doxygenstruct} fincore::stress_scenario_result
```

```{doxygenenum} fincore::bootstrap_statistic
```

```{doxygenenum} fincore::predefined_stress_scenario
```

```{doxygenfunction} fincore::analyze_simulation_paths(const dataframe &paths)
```

```{doxygenfunction} fincore::annualize(double value, std::string_view period="daily")
```

```{doxygenfunction} fincore::antithetic_variates(const dataframe &paths)
```

```{doxygenfunction} fincore::bootstrap_confidence_interval(const dataframe &returns, std::size_t resample_count=10000U, double alpha=0.05, bootstrap_statistic statistic=bootstrap_statistic::mean, std::optional< std::uint64_t > seed=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::bootstrap_distribution(const dataframe &returns, std::size_t resample_count=10000U, bootstrap_statistic statistic=bootstrap_statistic::mean, std::optional< std::uint64_t > seed=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::bootstrap_distribution(const dataframe &returns, std::span< const std::vector< std::size_t > > draw_indices, bootstrap_statistic statistic=bootstrap_statistic::mean, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::bootstrap_summary(const dataframe &returns, std::size_t resample_count=10000U, double alpha=0.05, std::optional< std::uint64_t > seed=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::estimate_simulation_parameters(const dataframe &returns, std::size_t periods_per_year=252, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::gbm_from_returns(const dataframe &returns, std::size_t horizon=252U, std::size_t path_count=1000U, std::size_t periods_per_year=252U, std::optional< std::uint64_t > seed=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::generate_correlation_breakdown(std::span< const std::string_view > asset_names, double target_correlation=1.0)
```

```{doxygenfunction} fincore::geometric_brownian_motion(double initial_value, double annualized_drift, double annualized_volatility, double total_years, double time_step, std::size_t path_count, std::optional< std::uint64_t > seed=std::nullopt)
```

```{doxygenfunction} fincore::latin_hypercube_sampling(std::size_t sample_count, std::size_t dimension_count, std::optional< std::uint64_t > seed=std::nullopt)
```

```{doxygenfunction} fincore::scenario_table(std::span< const stress_scenario_result > results)
```

```{doxygenfunction} fincore::simulate_monte_carlo(const dataframe &returns, std::size_t path_count=1000U, std::size_t horizon=252U, bool antithetic=false, std::optional< std::uint64_t > seed=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::simulate_monte_carlo_from_parameters(double annualized_drift, double annualized_volatility, double initial_value=1.0, std::size_t path_count=1000U, std::size_t horizon=252U, std::optional< std::uint64_t > seed=std::nullopt)
```

```{doxygenfunction} fincore::stress_test(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::stress_test(const dataframe &returns, std::span< const predefined_stress_scenario > scenarios, std::span< const custom_stress_scenario > custom_scenarios={}, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::validate_returns(std::span< const double > returns)
```

```{doxygenfunction} fincore::validate_returns(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenvariable} fincore::antithetic_path_column_prefix
```

```{doxygenvariable} fincore::bootstrap_statistic_column
```

```{doxygenvariable} fincore::gbm_path_column_prefix
```

```{doxygenvariable} fincore::latin_hypercube_dimension_column_prefix
```

```{doxygenvariable} fincore::stress_cumulative_return_column
```

```{doxygenvariable} fincore::stress_final_value_column
```

```{doxygenvariable} fincore::stress_max_drawdown_column
```

```{doxygenvariable} fincore::stress_scenario_name_column
```

```{doxygenvariable} fincore::stress_volatility_column
```
