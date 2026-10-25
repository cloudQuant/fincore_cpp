# Attribution

Brinson-Fachler, Brinson-Hood-Beebower, Fama-French and style attribution.

See the [usage guide](../guides/metrics.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::BrinsonAttribution
```

```{doxygenclass} fincore::FamaFrenchModel
```

```{doxygenclass} fincore::StyleResult
```

```{doxygenstruct} fincore::attribution_alignment
```

```{doxygenstruct} fincore::brinson_cumulative_result
```

```{doxygenstruct} fincore::brinson_period_result
```

```{doxygenstruct} fincore::factor_loading_table
```

```{doxygenstruct} fincore::factor_regression_result
```

```{doxygenstruct} fincore::fama_french_fit_result
```

```{doxygenstruct} fincore::idiosyncratic_risk_result
```

```{doxygenstruct} fincore::style_analysis_options
```

```{doxygenstruct} fincore::style_analysis_result
```

```{doxygenfunction} fincore::align_and_warn(const dataframe &returns, const dataframe &positions, const dataframe &factor_returns, const factor_loading_table &factor_loadings)
```

```{doxygenfunction} fincore::analyze_performance_by_style(const dataframe &asset_returns)
```

```{doxygenfunction} fincore::analyze_performance_by_style(const dataframe &asset_returns, const dataframe &style_exposures)
```

```{doxygenfunction} fincore::brinson_attribution(const dataframe &portfolio_returns, const dataframe &benchmark_returns, const dataframe &portfolio_weights, const dataframe &benchmark_weights, std::span< const std::string_view > sector_column_names)
```

```{doxygenfunction} fincore::brinson_cumulative(const dataframe &portfolio_returns, const dataframe &benchmark_returns, const dataframe &portfolio_weights, const dataframe &benchmark_weights, std::span< const std::string_view > sector_column_names)
```

```{doxygenfunction} fincore::brinson_results(const dataframe &portfolio_returns, const dataframe &benchmark_returns, const dataframe &portfolio_weights, const dataframe &benchmark_weights, std::span< const std::string_view > sector_column_names)
```

```{doxygenfunction} fincore::calculate_idiosyncratic_risk(const dataframe &returns, const dataframe &factor_data, std::optional< double > market_beta=std::nullopt, double risk_free=0.0)
```

```{doxygenfunction} fincore::calculate_regression_attribution(const dataframe &asset_returns)
```

```{doxygenfunction} fincore::calculate_regression_attribution(const dataframe &portfolio_returns, const dataframe &style_returns, std::span< const std::string_view > selected_styles)
```

```{doxygenfunction} fincore::calculate_style_tilts(const dataframe &asset_returns, std::size_t window=252U)
```

```{doxygenfunction} fincore::factor_attribution_decomposition(const dataframe &returns, const dataframe &factor_returns, std::span< const std::string_view > factor_column_names, double risk_free_rate=0.0, std::string_view returns_column_name=returns_column)
```

```{doxygenfunction} fincore::factor_regression_predictions(const factor_regression_result &regression, const dataframe &factor_returns, std::span< const std::string_view > factor_column_names)
```

```{doxygenfunction} fincore::fetch_ff_factors(std::string_view start, std::string_view end, std::string_view library, const fama_french_provider &provider, bool copy=true)
```

```{doxygenfunction} fincore::fetch_ff_factors(std::string_view start, std::string_view end, const fama_french_provider &provider, std::string_view library="french", bool copy=true)
```

```{doxygenfunction} fincore::fetch_style_factors(const std::vector< std::string > &tickers, const std::optional< std::vector< std::string > > &factors, std::string_view library, const style_factor_provider &provider)
```

```{doxygenfunction} fincore::fetch_style_factors(const std::vector< std::string > &tickers, const std::optional< std::vector< std::string > > &factors, const style_factor_provider &provider, std::string_view library="us")
```

```{doxygenfunction} fincore::fit_factor_regression(const dataframe &returns, const dataframe &factor_returns, std::span< const std::string_view > factor_column_names, std::string_view returns_column_name=returns_column)
```

```{doxygenfunction} fincore::rolling_factor_exposures(const dataframe &returns, const dataframe &factor_returns, std::span< const std::string_view > factor_column_names, std::optional< std::size_t > rolling_window=std::nullopt, std::string_view returns_column_name=returns_column)
```

```{doxygenfunction} fincore::style_analysis(const dataframe &asset_returns, const style_analysis_options &options={})
```

```{doxygentypedef} fincore::FamaFrenchModelPatch
```

```{doxygentypedef} fincore::fama_french_provider
```

```{doxygentypedef} fincore::style_factor_provider
```

```{doxygenvariable} fincore::FF3_FACTORS
```

```{doxygenvariable} fincore::FF4MOM_FACTORS
```

```{doxygenvariable} fincore::FF5_FACTORS
```

```{doxygenvariable} fincore::brinson_allocation_column
```

```{doxygenvariable} fincore::brinson_benchmark_return_column
```

```{doxygenvariable} fincore::brinson_interaction_column
```

```{doxygenvariable} fincore::brinson_portfolio_return_column
```

```{doxygenvariable} fincore::brinson_selection_column
```

```{doxygenvariable} fincore::brinson_total_column
```

```{doxygenvariable} fincore::factor_attribution_component_column
```

```{doxygenvariable} fincore::factor_attribution_contribution_column
```

```{doxygenvariable} fincore::factor_prediction_column
```

```{doxygenvariable} fincore::factor_regression_alpha_name
```

```{doxygenvariable} fincore::factor_regression_coefficient_column
```

```{doxygenvariable} fincore::factor_regression_factor_column
```

```{doxygenvariable} fincore::factor_regression_fitted_column
```

```{doxygenvariable} fincore::factor_regression_residual_column
```

```{doxygenvariable} fincore::style_asset_column
```

```{doxygenvariable} fincore::style_attribution_contribution_column
```

```{doxygenvariable} fincore::style_attribution_residual_name
```

```{doxygenvariable} fincore::style_equal_weight_column
```

```{doxygenvariable} fincore::style_growth_column
```

```{doxygenvariable} fincore::style_high_column
```

```{doxygenvariable} fincore::style_large_column
```

```{doxygenvariable} fincore::style_loser_column
```

```{doxygenvariable} fincore::style_low_column
```

```{doxygenvariable} fincore::style_mid_column
```

```{doxygenvariable} fincore::style_name_column
```

```{doxygenvariable} fincore::style_return_column
```

```{doxygenvariable} fincore::style_small_column
```

```{doxygenvariable} fincore::style_value_column
```

```{doxygenvariable} fincore::style_winner_column
```
