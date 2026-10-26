# Reports

Report models, HTML/Plotly/Bokeh/XLSX renderers, portfolio and risk report builders.

See the [usage guide](../guides/reports-runtime.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenstruct} fincore::portfolio_transaction
```

```{doxygenstruct} fincore::report_artifact
```

```{doxygenstruct} fincore::report_artifact_bundle
```

```{doxygenstruct} fincore::report_cell
```

```{doxygenstruct} fincore::report_document
```

```{doxygenstruct} fincore::report_html_options
```

```{doxygenstruct} fincore::report_metric
```

```{doxygenstruct} fincore::report_section
```

```{doxygenstruct} fincore::report_series
```

```{doxygenstruct} fincore::report_static_axes
```

```{doxygenstruct} fincore::report_table
```

```{doxygenstruct} fincore::xlsx_render_options
```

```{doxygenenum} fincore::report_chart_backend
```

```{doxygenfunction} fincore::build_portfolio_report(const dataframe &returns, const dataframe *benchmark=nullptr, std::string title="Portfolio Report", const dataframe *positions=nullptr, std::size_t rolling_window=63U, std::span< const portfolio_transaction > transactions={})
```

```{doxygenfunction} fincore::build_risk_report(const risk_validation_report &report, std::string title="Risk Validation Report")
```

```{doxygenfunction} fincore::render_bokeh(const report_document &document)
```

```{doxygenfunction} fincore::render_html(const report_document &document, const report_html_options &options={})
```

```{doxygenfunction} fincore::render_matplotlib(const report_document &document, const std::vector< std::string > &supplied_axes={})
```

```{doxygenfunction} fincore::render_pdf_bytes(const report_document &document)
```

```{doxygenfunction} fincore::render_plotly(const report_document &document)
```

```{doxygenfunction} fincore::render_xlsx(const report_document &document, const xlsx_render_options &options={})
```

```{doxygenfunction} fincore::write_pdf(const report_document &document, const std::string &target)
```

```{doxygenvariable} fincore::report_document_schema_version
```
