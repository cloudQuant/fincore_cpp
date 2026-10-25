# Extensions

Extension discovery and snapshot contracts for optional plugins.

See the [usage guide](../conventions.md) for task-oriented examples. Signatures below are generated from the public headers at the same commit as the rest of this site.

```{doxygenclass} fincore::extensions::extension_hook
```

```{doxygenclass} fincore::extensions::extension_snapshot
```

```{doxygenclass} fincore::extensions::renderer_registration
```

```{doxygenstruct} fincore::extensions::discovered_extension
```

```{doxygenstruct} fincore::extensions::extension_hook_invocation
```

```{doxygenstruct} fincore::extensions::extension_operation
```

```{doxygenstruct} fincore::extensions::loaded_extension
```

```{doxygenfunction} fincore::extensions::discover_extensions(std::span< const discovered_extension > manifests, std::span< const std::string_view > groups=entry_point_groups())
```

```{doxygenfunction} fincore::extensions::discover_extensions(const std::vector< discovered_extension > &manifests, std::span< const std::string_view > groups=entry_point_groups())
```

```{doxygenfunction} fincore::extensions::entry_point_groups() noexcept
```

```{doxygenfunction} fincore::extensions::load_extension(const discovered_extension &extension)
```

```{doxygenfunction} fincore::extensions::normalize_extension_name(std::string_view value)
```

```{doxygenfunction} fincore::extensions::operations()
```

```{doxygentypedef} fincore::extensions::extension_hook_callback
```

```{doxygentypedef} fincore::extensions::extension_keywords
```

```{doxygentypedef} fincore::extensions::extension_manifest
```

```{doxygentypedef} fincore::extensions::extension_renderer_callback
```

```{doxygentypedef} fincore::extensions::extension_value
```

```{doxygenvariable} fincore::extensions::ENTRY_POINT_GROUPS
```

```{doxygenvariable} fincore::extensions::default_hook_priority
```
