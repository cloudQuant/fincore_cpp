# API reference

The API reference is generated from the public headers under
`include/fincore/` with Doxygen and Breathe. Internal `fincore::detail`
helpers are excluded from the stable API reference. Signatures and
descriptions come from the same source commit as the rest of this site.

```{note}
A symbol appearing here means it is part of the maintained public surface. It
does not imply Python object compatibility, a supported platform beyond the
recorded acceptance evidence, or a performance guarantee.
```

## Modules

```{toctree}
---
maxdepth: 1
---
core
metrics
performance
risk
optimization
portfolio
attribution
factor
simulation
report
runtime
viz
extensions
```

## Conventions used by every module

- Timestamps are UTC nanoseconds; calendar helpers return UTC midnights.
- Prices and returns are decimal fractions unless a field name says
  otherwise. Annualization is explicit in each signature.
- `dataframe` inputs and outputs follow the DataFrame submodule's column
  rules; column names are part of each contract.
- Errors carry a stable category, code and message through
  {cpp:class}`fincore::fincore_error`; research routes add typed subclasses.
- Optional dependencies are named at the call site; no API downloads data or
  renders a chart implicitly.

## Coverage

The public symbol ledger records every extracted symbol, its module, source
line and documentation status. The maintainers regenerate it with
`tools/build_api_ledger.py` and require either documentation or a reviewed
exemption for each symbol before a documentation release. The ledger itself
is a source artifact in the repository, so its counts can be compared against
the same commit.

The rendered signatures are the normative C++ surface. Python-compatible
behaviour, including exception inheritance and DataFrame object identity, is
described in the [migration guide](../migration.md), not inferred from the
generated declarations.
