# Troubleshooting

## Configure or build fails

Confirm that the C++ toolchain supports C++23, CMake is at least 3.25, and the
pinned submodules are present (`git submodule update --init --recursive`).
If a consumer-provided DataFrame is used, set `FINCORE_DATAFRAME_SOURCE_DIR` or
provide a discoverable `DataFrameConfig.cmake`.

## Tests fail on a platform

Record the compiler, standard flags, CMake generator and the exact failing
test. Do not weaken assertions or change numerical tolerances merely to make a
local platform pass. The acceptance record distinguishes qualified passes and
external blockers.

## Data or numerical errors

Check units, index timezone/order/uniqueness, missing values, duplicate keys,
ties and window warm-up against the specific API contract. A `NaN` result may
mean an incomplete window or a genuinely undefined statistic, not zero return.

## Documentation build fails

Run Doxygen first, then Sphinx with `-W -n --keep-going`; confirm Doxygen XML
was regenerated from the same commit. Missing Breathe XML, stale gettext
catalogs and external link outages are separate failure classes; do not hide
them with global warning suppression.
