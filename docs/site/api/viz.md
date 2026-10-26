# Visualization

Chart data models, native HTML/SVG builders, Plotly/Bokeh documents and the optional ImPlot backend.

See the [usage guide](../guides/reports-runtime.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::bokeh_backend
```

```{doxygenclass} fincore::html_report_builder
```

```{doxygenclass} fincore::plotly_backend
```

```{doxygenclass} fincore::printf_tick_formatter
```

```{doxygenclass} fincore::renderer_snapshot
```

```{doxygenstruct} fincore::bar_chart
```

```{doxygenstruct} fincore::bokeh_document
```

```{doxygenstruct} fincore::bokeh_glyph
```

```{doxygenstruct} fincore::bokeh_hover
```

```{doxygenstruct} fincore::bokeh_span
```

```{doxygenstruct} fincore::chart_series
```

```{doxygenstruct} fincore::correlation_matrix
```

```{doxygenstruct} fincore::frontier_chart_data
```

```{doxygenstruct} fincore::heat_grid
```

```{doxygenstruct} fincore::histogram_bins
```

```{doxygenstruct} fincore::html_metric
```

```{doxygenstruct} fincore::plotly_annotation
```

```{doxygenstruct} fincore::plotly_axis
```

```{doxygenstruct} fincore::plotly_color_theme
```

```{doxygenstruct} fincore::plotly_document
```

```{doxygenstruct} fincore::plotly_shape
```

```{doxygenstruct} fincore::plotly_trace
```

```{doxygenstruct} fincore::render_model
```

```{doxygenstruct} fincore::series_chart
```

```{doxygenstruct} fincore::series_chart_spec
```

```{doxygenfunction} fincore::build_bar_chart(std::vector< std::string > group_labels, std::vector< std::string > series_labels, std::vector< double > values)
```

```{doxygenfunction} fincore::build_correlation_matrix(const dataframe &frame)
```

```{doxygenfunction} fincore::build_drawdown_chart(const dataframe &frame, std::string_view column="drawdown", std::string_view label="Drawdown")
```

```{doxygenfunction} fincore::build_frontier_chart(const efficient_frontier_result &frontier)
```

```{doxygenfunction} fincore::build_histogram(std::span< const double > values, std::size_t bins=10U)
```

```{doxygenfunction} fincore::build_monthly_heatmap(const dataframe &returns, std::string_view column=returns_column)
```

```{doxygenfunction} fincore::build_returns_chart(const dataframe &frame, std::string_view column="cumulative_returns", std::string_view label="Cumulative Returns")
```

```{doxygenfunction} fincore::build_rolling_sharpe_chart(const dataframe &portfolio, std::string_view column, std::size_t window, const dataframe *benchmark=nullptr, std::string_view benchmark_column="benchmark_sharpe", std::string_view label="Portfolio Sharpe", std::string_view benchmark_label="Benchmark Sharpe")
```

```{doxygenfunction} fincore::build_series_chart(const dataframe &frame, const std::vector< series_chart_spec > &specs, std::string title, std::string y_label, bool zero_line=false, bool fill_to_zero=false)
```

```{doxygenfunction} fincore::plot_bar_chart(const bar_chart &chart)
```

```{doxygenfunction} fincore::plot_correlation_matrix(const correlation_matrix &matrix)
```

```{doxygenfunction} fincore::plot_cumulative_returns(const dataframe &frame, std::string_view column)
```

```{doxygenfunction} fincore::plot_drawdown(const dataframe &frame, std::string_view column)
```

```{doxygenfunction} fincore::plot_drawdown_chart(const series_chart &chart)
```

```{doxygenfunction} fincore::plot_events_distribution_chart(const events_distribution_chart &chart)
```

```{doxygenfunction} fincore::plot_frontier_chart(const frontier_chart_data &chart)
```

```{doxygenfunction} fincore::plot_histogram(const histogram_bins &bins)
```

```{doxygenfunction} fincore::plot_monthly_heatmap(const heat_grid &grid)
```

```{doxygenfunction} fincore::plot_monthly_ic_heatmap(const monthly_ic_heatmap_chart &chart)
```

```{doxygenfunction} fincore::plot_qq_chart(const qq_chart &chart)
```

```{doxygenfunction} fincore::plot_quantile_cumulative_chart(const quantile_cumulative_chart &chart)
```

```{doxygenfunction} fincore::plot_rolling_metric(const dataframe &frame, std::string_view column, std::string_view label)
```

```{doxygenfunction} fincore::plot_series_chart(const series_chart &chart)
```

```{doxygenfunction} fincore::plot_spread_time_series(const spread_time_series_chart &chart)
```

```{doxygenfunction} fincore::plot_violin_chart(const violin_chart &chart)
```

```{doxygentypedef} fincore::renderer_callback
```

```{doxygentypedef} fincore::renderer_map
```

```{doxygenvariable} fincore::NOT_RUN_INTERACTIVE
```

```{doxygenvariable} fincore::month_labels
```
