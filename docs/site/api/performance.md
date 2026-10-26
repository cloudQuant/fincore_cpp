# Performance

Cashflow-adjusted returns, disclosure contexts, returns inference and performance statistics.

See the [usage guide](../guides/metrics.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenstruct} fincore::disclosure_context
```

```{doxygenenum} fincore::fee_treatment
```

```{doxygenfunction} fincore::cashflow_adjusted_returns(const dataframe &valuations, const std::optional< dataframe > &cashflows=std::nullopt, const std::optional< dataframe > &fees=std::nullopt, fee_treatment treatment=fee_treatment::net, std::string_view value_column="value", std::string_view amount_column="amount", std::string_view timing_column="timing", std::string_view fees_column="fees")
```

```{doxygenfunction} fincore::cashflow_adjusted_twr(const dataframe &valuations, const std::optional< dataframe > &cashflows=std::nullopt, const std::optional< dataframe > &fees=std::nullopt, fee_treatment treatment=fee_treatment::net, std::string_view value_column="value", std::string_view amount_column="amount", std::string_view timing_column="timing", std::string_view fees_column="fees")
```

```{doxygenfunction} fincore::mwr(const dataframe &cashflows, std::optional< double > period=std::nullopt, std::string_view amount_column="cashflow")
```

```{doxygenfunction} fincore::render_disclosure(const disclosure_context &context)
```

```{doxygenfunction} fincore::sharpe_confidence_interval(const dataframe &returns, double risk_free=0.0, double z=1.96, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::sharpe_standard_error(const dataframe &returns, double risk_free=0.0, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::standard_error_of_mean(const dataframe &values, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::twr(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::xirr(const dataframe &cashflows, std::string_view amount_column="cashflow")
```
