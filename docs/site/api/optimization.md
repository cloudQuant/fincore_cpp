# Optimization

Mean-variance objectives, risk parity and efficient-frontier construction with diagnostics.

See the [usage guide](../guides/portfolio-risk.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenstruct} fincore::efficient_frontier_result
```

```{doxygenstruct} fincore::frontier_config
```

```{doxygenstruct} fincore::frontier_point_diagnostics
```

```{doxygenstruct} fincore::portfolio_optimization_config
```

```{doxygenstruct} fincore::portfolio_optimization_result
```

```{doxygenstruct} fincore::portfolio_point
```

```{doxygenstruct} fincore::risk_parity_result
```

```{doxygenstruct} fincore::sector_allocation_constraint
```

```{doxygenenum} fincore::frontier_solver
```

```{doxygenenum} fincore::portfolio_objective
```

```{doxygenfunction} fincore::efficient_frontier(const dataframe &returns, const frontier_config &config={})
```

```{doxygenfunction} fincore::optimize_portfolio(const dataframe &returns, const portfolio_optimization_config &config={})
```

```{doxygenfunction} fincore::risk_parity(const dataframe &returns, std::optional< std::vector< double > > risk_budget=std::nullopt, std::size_t max_iter=1000U)
```
