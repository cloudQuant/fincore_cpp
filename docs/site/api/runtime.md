# Runtime

Operation catalogs, sessions, snapshots, artifact bundles and serialization helpers.

See the [usage guide](../guides/reports-runtime.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::analysis_session
```

```{doxygenclass} fincore::analysis_snapshot
```

```{doxygenclass} fincore::native_backend
```

```{doxygenclass} fincore::operation_catalog
```

```{doxygenclass} fincore::operation_resolution_error
```

```{doxygenstruct} fincore::artifact_bundle
```

```{doxygenstruct} fincore::artifact_descriptor
```

```{doxygenstruct} fincore::dense_array
```

```{doxygenstruct} fincore::execution_plan
```

```{doxygenstruct} fincore::operation_request
```

```{doxygenstruct} fincore::operation_spec
```

```{doxygenstruct} fincore::runtime_result
```

```{doxygenstruct} fincore::runtime_result_payload
```

```{doxygenstruct} fincore::runtime_value
```

```{doxygenenum} fincore::alignment_policy
```

```{doxygenfunction} fincore::align_binary_metric_inputs(const dataframe &left, const dataframe &right, alignment_policy policy=alignment_policy::inner)
```

```{doxygenfunction} fincore::align_binary_metric_inputs(const dense_array &left, const dense_array &right, alignment_policy policy=alignment_policy::inner)
```

```{doxygenfunction} fincore::align_time_series(const std::vector< dataframe > &values, alignment_policy policy)
```

```{doxygenfunction} fincore::attribution_factor_operations()
```

```{doxygenfunction} fincore::batch(const execution_plan &plan, const operation_catalog &catalog)
```

```{doxygenfunction} fincore::builtin_catalog()
```

```{doxygenfunction} fincore::copy_for_consumer(const runtime_result &result)
```

```{doxygenfunction} fincore::from_json(std::string_view json)
```

```{doxygenfunction} fincore::from_payload(const runtime_result_payload &payload)
```

```{doxygenfunction} fincore::load_optional_module(std::string_view module_name, bool available, std::string_view dependency={}, std::string_view extra={}, std::string_view message={})
```

```{doxygenfunction} fincore::make_operations_provider(std::vector< operation_spec > declarations)
```

```{doxygenfunction} fincore::metrics_performance_operations()
```

```{doxygenfunction} fincore::normalize_time_series_timezone(const dataframe &value, std::string_view normalize_tz)
```

```{doxygenfunction} fincore::plan(const std::vector< operation_request > &requests, const operation_catalog &catalog)
```

```{doxygenfunction} fincore::portfolio_risk_simulation_operations()
```

```{doxygenfunction} fincore::run(const std::string &operation_id, const runtime_inputs &inputs, const runtime_config &config, const operation_catalog &catalog)
```

```{doxygenfunction} fincore::runtime_value_digest(const runtime_value &value)
```

```{doxygenfunction} fincore::semantic_digest(const runtime_result &result)
```

```{doxygenfunction} fincore::to_json(const runtime_result &result)
```

```{doxygenfunction} fincore::to_payload(const runtime_result &result)
```

```{doxygenfunction} fincore::validate_finite_array(const dense_array &value, std::string_view name="value", std::int64_t min_size=1, std::optional< std::int64_t > ndim=std::nullopt)
```

```{doxygenfunction} fincore::validate_mapping
```

```{doxygenfunction} fincore::with_metadata(const runtime_result &result, const std::map< std::string, std::string > &metadata)
```

```{doxygenfunction} fincore::write_artifact_bundle(const artifact_bundle &bundle, const std::filesystem::path &output_directory)
```

```{doxygentypedef} fincore::operations_provider
```

```{doxygentypedef} fincore::runtime_config
```

```{doxygentypedef} fincore::runtime_inputs
```

```{doxygenvariable} fincore::runtime_result_schema_version
```
