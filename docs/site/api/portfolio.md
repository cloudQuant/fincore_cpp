# Portfolio

Positions, capacity, exposure and portfolio model inputs.

See the [usage guide](../guides/portfolio-risk.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::portfolio_inputs
```

```{doxygenstruct} fincore::capacity_assessment
```

```{doxygenstruct} fincore::capacity_config
```

```{doxygenstruct} fincore::daily_txn_bar
```

```{doxygenstruct} fincore::exposure_bundle
```

```{doxygenstruct} fincore::long_short_aggregate
```

```{doxygenstruct} fincore::position_record
```

```{doxygenstruct} fincore::ranked_position
```

```{doxygenstruct} fincore::round_trip_stat_table
```

```{doxygenstruct} fincore::round_trip_statistic
```

```{doxygenstruct} fincore::round_trip_stats
```

```{doxygenstruct} fincore::round_trip_transaction
```

```{doxygenstruct} fincore::sector_assignment
```

```{doxygenstruct} fincore::sector_definition
```

```{doxygenstruct} fincore::ticker_liquidity
```

```{doxygenstruct} fincore::top_positions
```

```{doxygenstruct} fincore::trade_record
```

```{doxygenstruct} fincore::transaction_liquidity
```

```{doxygenenum} fincore::turnover_denominator
```

```{doxygenfunction} fincore::add_closing_transactions(const dataframe &positions, std::span< const round_trip_transaction > transactions)
```

```{doxygenfunction} fincore::adjust_returns_for_slippage(const dataframe &returns, const dataframe &positions, const dataframe &transactions, double slippage_bps, std::string_view returns_column_name=returns_column, std::string_view amount_column_name=transaction_amount_column, std::string_view price_column_name=transaction_price_column)
```

```{doxygenfunction} fincore::agg_all_long_short(const dataframe &round_trips, std::string_view column, std::span< const round_trip_statistic > statistics)
```

```{doxygenfunction} fincore::apply_sector_mappings_to_round_trips(const dataframe &round_trips, const std::map< std::string, std::string > &sector_mappings)
```

```{doxygenfunction} fincore::apply_slippage_penalty(const dataframe &returns, const dataframe &transactions_with_bar_data, double simulate_starting_capital, double backtest_starting_capital, double impact=0.1, std::string_view returns_column_name=returns_column, std::string_view amount_column_name=transaction_amount_column, std::string_view price_column_name=transaction_price_column, std::string_view volume_column_name=transaction_bar_volume_column)
```

```{doxygenfunction} fincore::assess_liquidity(const dataframe &positions, std::span< const trade_record > transactions, const dataframe &prices, const dataframe &volumes, const capacity_config &config={})
```

```{doxygenfunction} fincore::compute_cap_exposures(const dataframe &positions, const dataframe &caps, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::compute_sector_exposures(const dataframe &positions, const dataframe &sectors, std::span< const sector_definition > definitions, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::compute_style_factor_exposures(const dataframe &positions, const dataframe &risk_factors, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::compute_volume_exposures(const dataframe &shares_held, const dataframe &volumes, double percentile=0.5)
```

```{doxygenfunction} fincore::daily_txns_with_bar_data(std::span< const trade_record > transactions, const dataframe &prices, const dataframe &volumes)
```

```{doxygenfunction} fincore::days_to_liquidate_positions(const dataframe &positions, const dataframe &prices, const dataframe &volumes, double max_bar_consumption=0.2, double capital_base=1000000.0, std::size_t mean_volume_window=5U, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::extract_round_trips(const dataframe &transactions, std::chrono::nanoseconds max_delta=default_round_trip_max_delta, std::string_view symbol_column_name=transaction_symbol_column, std::string_view amount_column_name=transaction_amount_column, std::string_view price_column_name=transaction_price_column)
```

```{doxygenfunction} fincore::gen_round_trip_stats(const dataframe &round_trips, std::string_view pnl_column_name=round_trip_pnl_column, std::string_view returns_column_name=round_trip_return_column, std::string_view long_column_name=round_trip_long_column, std::string_view symbol_column_name=transaction_symbol_column, std::string_view open_column_name=round_trip_open_datetime_column, std::string_view close_column_name=round_trip_close_datetime_column)
```

```{doxygenfunction} fincore::get_long_short_notional(const dataframe &positions, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::get_long_short_pos(const dataframe &positions, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::get_low_liquidity_transactions(std::span< const trade_record > transactions, const dataframe &prices, const dataframe &volumes, std::optional< std::size_t > last_n_days=std::nullopt)
```

```{doxygenfunction} fincore::get_max_days_to_liquidate_by_ticker(const dataframe &positions, const dataframe &prices, const dataframe &volumes, double max_bar_consumption=0.2, double capital_base=1000000.0, std::size_t mean_volume_window=5U, std::optional< std::size_t > last_n_days=std::nullopt, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::get_max_median_position_concentration(const dataframe &positions, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::get_percent_alloc(const dataframe &positions)
```

```{doxygenfunction} fincore::get_sector_exposures(const dataframe &positions, std::span< const sector_assignment > assignments, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::get_top_long_short_abs(const dataframe &positions, std::size_t top=10U, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::get_turnover(const dataframe &positions, const dataframe &transactions, turnover_denominator denominator=turnover_denominator::actual_gross_book, std::string_view cash_column_name=cash_column, std::string_view amount_column_name=transaction_amount_column, std::string_view price_column_name=transaction_price_column)
```

```{doxygenfunction} fincore::get_txn_vol(const dataframe &transactions, std::string_view amount_column_name=transaction_amount_column, std::string_view price_column_name=transaction_price_column)
```

```{doxygenfunction} fincore::gross_lev(const dataframe &positions, std::string_view cash_column_name=cash_column)
```

```{doxygenfunction} fincore::groupby_consecutive_transactions(const dataframe &transactions, std::chrono::nanoseconds max_delta=default_round_trip_max_delta, std::string_view symbol_column_name=transaction_symbol_column, std::string_view amount_column_name=transaction_amount_column, std::string_view price_column_name=transaction_price_column)
```

```{doxygenfunction} fincore::make_transaction_frame(std::span< const portfolio_transaction > transactions)
```

```{doxygenfunction} fincore::map_transaction(const portfolio_transaction &transaction)
```

```{doxygenfunction} fincore::stack_positions(const dataframe &positions, bool pos_in_dollars=true, std::string_view cash_column_name=cash_column)
```

```{doxygenvariable} fincore::cash_column
```

```{doxygenvariable} fincore::default_round_trip_max_delta
```

```{doxygenvariable} fincore::round_trip_close_datetime_column
```

```{doxygenvariable} fincore::round_trip_duration_column
```

```{doxygenvariable} fincore::round_trip_long_column
```

```{doxygenvariable} fincore::round_trip_open_datetime_column
```

```{doxygenvariable} fincore::round_trip_pnl_column
```

```{doxygenvariable} fincore::round_trip_return_column
```

```{doxygenvariable} fincore::transaction_amount_column
```

```{doxygenvariable} fincore::transaction_bar_volume_column
```

```{doxygenvariable} fincore::transaction_price_column
```

```{doxygenvariable} fincore::transaction_symbol_column
```
