# Risk

VaR/ES diagnostics, GARCH/EGARCH estimates, extreme-value models, backtesting, walk-forward validation and risk reports.

See the [usage guide](../guides/portfolio-risk.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenstruct} fincore::egarch_parameters
```

```{doxygenstruct} fincore::egarch_result
```

```{doxygenstruct} fincore::extreme_risk_result
```

```{doxygenstruct} fincore::garch11_parameters
```

```{doxygenstruct} fincore::garch11_result
```

```{doxygenstruct} fincore::garch_conditional_estimate
```

```{doxygenstruct} fincore::garch_forecast_result
```

```{doxygenstruct} fincore::gev_parameters
```

```{doxygenstruct} fincore::gjr_garch_parameters
```

```{doxygenstruct} fincore::gjr_garch_result
```

```{doxygenstruct} fincore::gpd_parameters
```

```{doxygenstruct} fincore::hill_estimate
```

```{doxygenstruct} fincore::refit_parameters
```

```{doxygenstruct} fincore::risk_backtest_result
```

```{doxygenstruct} fincore::risk_estimate
```

```{doxygenstruct} fincore::risk_forecast_config
```

```{doxygenstruct} fincore::risk_forecast_event
```

```{doxygenstruct} fincore::risk_model_spec
```

```{doxygenstruct} fincore::risk_refit_event
```

```{doxygenstruct} fincore::risk_report_diagnostics
```

```{doxygenstruct} fincore::risk_validation_report
```

```{doxygenstruct} fincore::risk_validation_report_dict
```

```{doxygenstruct} fincore::walk_forward_var_result
```

```{doxygenenum} fincore::evt_model
```

```{doxygenenum} fincore::evt_tail
```

```{doxygenenum} fincore::garch_model
```

```{doxygenenum} fincore::gpd_estimation_method
```

```{doxygenenum} fincore::risk_backtest_method
```

```{doxygenenum} fincore::risk_backtest_status
```

```{doxygenenum} fincore::risk_distribution
```

```{doxygenenum} fincore::risk_estimate_status
```

```{doxygenenum} fincore::risk_forecast_method
```

```{doxygenenum} fincore::risk_forecast_target
```

```{doxygenenum} fincore::risk_tail
```

```{doxygenenum} fincore::walk_forward_status
```

```{doxygenfunction} fincore::backtest_es(const dataframe &forecast, const dataframe &realized, double confidence_level=0.975, std::size_t n_bootstrap=1000U, std::uint64_t seed=20260817U, std::string_view forecast_column="forecast", std::string_view realized_column="realized")
```

```{doxygenfunction} fincore::backtest_var(const dataframe &forecast, const dataframe &realized, double confidence_level=0.99, double significance=0.05, std::string_view forecast_column="forecast", std::string_view realized_column="realized")
```

```{doxygenfunction} fincore::basel_traffic_light(std::size_t exceptions, std::size_t observations, double confidence_level=0.99)
```

```{doxygenfunction} fincore::build_risk_validation_report(const walk_forward_var_result &result, std::optional< std::string > timestamp_index_name=std::nullopt)
```

```{doxygenfunction} fincore::christoffersen_lr(std::span< const int > exceptions)
```

```{doxygenfunction} fincore::conditional_es(const dataframe &returns, garch_model model, double alpha=0.05, std::size_t horizon=1U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::conditional_var(const dataframe &returns, garch_model model, double alpha=0.05, std::size_t horizon=1U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::es_calibration_score(double forecast_es, std::span< const double > realized, double confidence_level)
```

```{doxygenfunction} fincore::evt_cvar(const dataframe &data, double alpha=0.05, evt_model model=evt_model::gpd, evt_tail tail=evt_tail::lower, std::optional< double > threshold=std::nullopt, std::optional< std::size_t > block_size=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::evt_var(const dataframe &data, double alpha=0.05, evt_model model=evt_model::gpd, evt_tail tail=evt_tail::lower, std::optional< double > threshold=std::nullopt, std::optional< std::size_t > block_size=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::expected_exception_count(std::size_t observations, double confidence_level)
```

```{doxygenfunction} fincore::extreme_risk(const dataframe &returns, double alpha=0.05, evt_tail tail=evt_tail::lower, evt_model model=evt_model::gpd, std::optional< double > threshold=std::nullopt, std::optional< std::size_t > block_size=std::nullopt, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::fit_egarch(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::fit_garch11(const dataframe &returns, bool constant_mean=false, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::fit_gjr_garch(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::forecast_egarch(const egarch_result &result, std::size_t horizon=1U)
```

```{doxygenfunction} fincore::forecast_es(const dataframe &returns, const risk_forecast_config &config={}, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::forecast_garch11(const garch11_result &result, std::size_t horizon=1U)
```

```{doxygenfunction} fincore::forecast_gjr_garch(const gjr_garch_result &result, std::size_t horizon=1U)
```

```{doxygenfunction} fincore::forecast_var(const dataframe &returns, const risk_forecast_config &config={}, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::forecast_volatility(const dataframe &returns, garch_model model, std::size_t horizon=1U, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::garch11_log_likelihood(std::span< const double > eps, double omega, double alpha, double beta)
```

```{doxygenfunction} fincore::gev_fit(const dataframe &data, std::optional< std::size_t > block_size=std::nullopt, evt_tail tail=evt_tail::lower, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::gpd_fit(const dataframe &data, std::optional< double > threshold=std::nullopt, gpd_estimation_method method=gpd_estimation_method::mle, evt_tail tail=evt_tail::lower, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::hill_estimator(const dataframe &data, std::optional< double > threshold=std::nullopt, evt_tail tail=evt_tail::upper, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::kupiec_lr(std::size_t observations, std::size_t exceptions, double confidence_level)
```

```{doxygenfunction} fincore::risk_backtest_status_name(risk_backtest_status status)
```

```{doxygenfunction} fincore::risk_forecast_target_name(risk_forecast_target target)
```

```{doxygenfunction} fincore::risk_tail_name(risk_tail tail)
```

```{doxygenfunction} fincore::validate_risk_model_spec(const risk_model_spec &spec)
```

```{doxygenfunction} fincore::walk_forward_status_name(walk_forward_status status)
```

```{doxygenfunction} fincore::walk_forward_var(const dataframe &returns, const risk_model_spec &spec, std::string_view column=returns_column)
```

```{doxygenvariable} fincore::losses_negative_sign_convention
```

```{doxygenvariable} fincore::risk_validation_report_schema_version
```
