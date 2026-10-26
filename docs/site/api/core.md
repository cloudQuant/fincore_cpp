# Core types and errors

Frames, timestamps, calendars, operation descriptors, versions, rolling windows, data-provider contracts and structured errors.

See the [usage guide](../quickstart.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::analysis_context
```

```{doxygenclass} fincore::batch_fetch_error
```

```{doxygenclass} fincore::data_provider
```

```{doxygenclass} fincore::data_snapshot
```

```{doxygenclass} fincore::fincore_error
```

```{doxygenclass} fincore::input_error
```

```{doxygenclass} fincore::optimization_error
```

```{doxygenclass} fincore::rolling_engine
```

```{doxygenstruct} fincore::dependency_status
```

```{doxygenstruct} fincore::error_info
```

```{doxygenstruct} fincore::operation_descriptor
```

```{doxygenstruct} fincore::request_policy
```

```{doxygenenum} fincore::rolling_metric
```

```{doxygenenum} fincore::transport_error_kind
```

```{doxygenfunction} fincore::align_on_common_index(const dataframe &left, std::string_view left_column, const dataframe &right, std::string_view right_column)
```

```{doxygenfunction} fincore::alignment_error(std::string message, std::string operation_id={}, std::string parameter={}, std::string path={}, std::string profile={})
```

```{doxygenfunction} fincore::all_operations()
```

```{doxygenfunction} fincore::attribution_operations()
```

```{doxygenfunction} fincore::canonical_dataframe_csv(const dataframe &frame)
```

```{doxygenfunction} fincore::check_dependencies(std::span< const dependency_status > dependencies)
```

```{doxygenfunction} fincore::contextual_error(std::string category, std::string message, std::string operation_id={}, std::string parameter={}, std::string path={}, std::string profile={})
```

```{doxygenfunction} fincore::data_alignment_error(std::string message, std::optional< std::int64_t > returns_length=std::nullopt, std::optional< std::int64_t > factor_length=std::nullopt)
```

```{doxygenfunction} fincore::data_operations()
```

```{doxygenfunction} fincore::dataframe_content_sha256(const dataframe &frame)
```

```{doxygenfunction} fincore::dependency_error(std::string message, std::string dependency={}, std::string extra={})
```

```{doxygenfunction} fincore::ensure_not_nan(double value, std::string_view param_name, std::optional< double > replace_with=std::nullopt)
```

```{doxygenfunction} fincore::factor_analysis_operations()
```

```{doxygenfunction} fincore::handle_numerical_error
```

```{doxygenfunction} fincore::input_contract_error(std::string message, std::string operation_id={}, std::string parameter={}, std::string path={}, std::string profile={})
```

```{doxygenfunction} fincore::insufficient_data_error(std::string message, std::optional< std::int64_t > required_length=std::nullopt, std::optional< std::int64_t > actual_length=std::nullopt)
```

```{doxygenfunction} fincore::invalid_period_error(std::string period)
```

```{doxygenfunction} fincore::make_error(std::string category, std::string code, std::string message, std::map< std::string, std::string > fields={}, std::map< std::string, std::string > report={})
```

```{doxygenfunction} fincore::make_operation_resolution_error(std::string operation_id)
```

```{doxygenfunction} fincore::make_series(std::vector< timestamp > index, std::string_view column_name, std::vector< double > values)
```

```{doxygenfunction} fincore::metrics_operations()
```

```{doxygenfunction} fincore::missing_data_error(std::string message, std::string missing_field={})
```

```{doxygenfunction} fincore::numerical_convergence_error(std::string message, std::string operation_id={}, std::string parameter={}, std::string path={}, std::string profile={})
```

```{doxygenfunction} fincore::numerical_error(std::string message, std::string operation={})
```

```{doxygenfunction} fincore::optimization_operations()
```

```{doxygenfunction} fincore::performance_operations()
```

```{doxygenfunction} fincore::portfolio_operations()
```

```{doxygenfunction} fincore::report_operations()
```

```{doxygenfunction} fincore::require_double_column(const dataframe &frame, std::string_view column_name)
```

```{doxygenfunction} fincore::resource_lifecycle_error(std::string message, std::string operation_id={}, std::string parameter={}, std::string path={}, std::string profile={})
```

```{doxygenfunction} fincore::result_contract_error(std::string message, std::string operation_id={}, std::string parameter={}, std::string path={}, std::string profile={})
```

```{doxygenfunction} fincore::risk_operations()
```

```{doxygenfunction} fincore::runtime_diagnostic_error(std::string message, std::string runtime_category, std::string operation_id={}, std::string capability_id={}, std::map< std::string, std::string > details={}, std::string remediation={})
```

```{doxygenfunction} fincore::safe_divide(double numerator, double denominator, double default_value=std::numeric_limits< double >::quiet_NaN())
```

```{doxygenfunction} fincore::safe_sqrt(double value, double default_value=std::numeric_limits< double >::quiet_NaN())
```

```{doxygenfunction} fincore::simulation_operations()
```

```{doxygenfunction} fincore::unsupported_format_error(std::string message, std::string format={})
```

```{doxygenfunction} fincore::utc_timestamp(int year, unsigned month=1, unsigned day=1, unsigned hour=0, unsigned minute=0, unsigned second=0, unsigned nanosecond=0)
```

```{doxygenfunction} fincore::utc_year(timestamp value) noexcept
```

```{doxygenfunction} fincore::validation_error(std::string message, std::string param_name={}, std::string value={})
```

```{doxygenfunction} fincore::version() noexcept
```

```{doxygentypedef} fincore::dataframe
```

```{doxygentypedef} fincore::timestamp
```

```{doxygenvariable} fincore::data_snapshot_schema_version
```

```{doxygenvariable} fincore::pinned_version_major
```

```{doxygenvariable} fincore::pinned_version_minor
```

```{doxygenvariable} fincore::pinned_version_patch
```

```{doxygenvariable} fincore::pinned_version_string
```

```{doxygenvariable} fincore::version_major
```

```{doxygenvariable} fincore::version_minor
```

```{doxygenvariable} fincore::version_patch
```

```{doxygenvariable} fincore::version_string
```
