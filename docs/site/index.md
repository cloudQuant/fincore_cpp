# fincore_cpp

`fincore_cpp` is a native C++23 library for portfolio, factor, performance
and risk analytics. It is a native implementation with its own typed API; it
does not promise Python object or pandas compatibility.

```{toctree}
:maxdepth: 2
:caption: Start here

installation
quickstart
conventions
```

```{toctree}
:maxdepth: 2
:caption: Guides

guides/metrics
guides/portfolio-risk
guides/factor-research
guides/reports-runtime
```

```{toctree}
:maxdepth: 2
:caption: Reference

api/index
migration
performance
troubleshooting
contributing
publication
maintainer/README
```

## Scope

The library consumes a local `DataFrame` type and returns owning typed
results. The documentation distinguishes the stable public surface from
internal `fincore::detail` helpers. Optional visualization and plotting
adapters are built only when their CMake options and dependencies are enabled.

## Status

This site is generated from the same source commit as the C++ library and its
English/Chinese guides. It is validated locally with strict Sphinx and
Doxygen checks; external hosting status is recorded separately in the
publication notes.
