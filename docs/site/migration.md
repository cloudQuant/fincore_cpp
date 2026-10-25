# Migrating from Python fincore

`fincore_cpp` is a native library, not a Python binding. It does not provide
`import fincore` compatibility, Python exception inheritance, pandas views or
NumPy dtypes. Port workflows by matching the native typed inputs and outputs,
not by mechanically renaming Python objects.

The repository's parity records describe the implemented native surface and
its declared variances. The current iteration-8 record is the authoritative
scope snapshot; later performance work does not retroactively expand it. The
current dependency and platform status should be checked in the dated
acceptance records rather than inferred from a phase count.

For a migration review, map each Python call to a public native symbol, verify
its unit/alignment contract and retain the same fixture or oracle budget. Do
not treat `EXTERNAL_OPTIONAL` provider or PyMC routes as native parity.
