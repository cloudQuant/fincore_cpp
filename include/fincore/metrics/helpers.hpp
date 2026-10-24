#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/metrics/basic.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {

// Row-major dense matrix used where the pinned Python source consumes a
// NumPy 2-D array. `values.size()` is always `row_count * column_count`.
class matrix {
public:
    matrix() = default;
    matrix(std::size_t row_count,
           std::size_t column_count,
           std::vector<double> values);

    [[nodiscard]] std::size_t rows() const noexcept;
    [[nodiscard]] std::size_t columns() const noexcept;
    [[nodiscard]] std::span<const double> values() const noexcept;
    [[nodiscard]] std::span<const double> row(std::size_t row_index) const;
    [[nodiscard]] double at(std::size_t row_index,
                            std::size_t column_index) const;
    double& at(std::size_t row_index, std::size_t column_index);

private:
    std::size_t row_count_{0};
    std::size_t column_count_{0};
    std::vector<double> values_;
};

// Align two single-column series on their common timestamps.
//
// Native contract: the common index is emitted in left-frame order (the
// `align_on_common_index` semantics), timestamps absent from either frame are
// dropped, and present NaN values are retained. Both indexes must contain
// unique timestamps and must not contain the NaT sentinel
// (`numeric_limits<timestamp>::min()`); duplicates are ambiguous for keyed
// alignment and raise `input_error`.
//
// Declared source variances: the pinned Python two-Series path uses
// `pd.concat(..., sort=False)` (an outer union that keeps right-only rows as
// NaN), while its DataFrame and mixed DataFrame/Series paths intersect. This
// native helper follows the P101 common-index intersection for every input,
// and emits left order where the source DataFrame.align path sorts ascending.
[[nodiscard]] std::pair<dataframe, dataframe> aligned_series(
    const dataframe& left,
    std::string_view left_column,
    const dataframe& right,
    std::string_view right_column);

// Align two frames on their common timestamps, keeping every double column of
// each frame in source column order. Same uniqueness, NaT, missingness and
// ordering rules as the single-column overload.
[[nodiscard]] std::pair<dataframe, dataframe> aligned_series(
    const dataframe& left,
    const dataframe& right);

// Validate and normalize a frame's UTC-nanosecond index.
//
// A present index must be strictly increasing, duplicate-free and free of the
// NaT sentinel; the returned frame is an owning copy. A frame without an index
// but with exactly one double column receives the synthetic UTC index the
// source builds from `pd.date_range("1970-01-01", periods=n, freq=...)`:
//   daily     -> 1970-01-01, 1970-01-02, ...
//   weekly    -> Sundays from 1970-01-04
//   monthly   -> month ends from 1970-01-31
//   quarterly -> quarter ends from 1970-03-31
//   yearly    -> 1970-12-31, 1971-12-31, ...
// An empty frame is returned unchanged. Frames with several columns and no
// index, or with a length mismatch between columns and index, raise
// `input_error`; the source silently regenerates an index for an unindexed
// Series only.
[[nodiscard]] dataframe ensure_datetime_index_series(
    const dataframe& frame,
    frequency period = frequency::daily);

// Vector form of the synthetic-index path. An empty span returns an empty
// frame. Values are copied verbatim (NaN retained).
[[nodiscard]] dataframe ensure_datetime_index_series(
    std::span<const double> values,
    frequency period = frequency::daily,
    std::string_view column = returns_column);

// Flatten a frame to a row-major vector.
//
// Columns are visited in source insertion order and rows in index order, so
// the result is `row0[col0..colN], row1[col0..colN], ...`. Only double columns
// are supported; every column must have the frame index length. Present NaN
// cells are preserved (the native frame has no absent-cell state); a frame
// whose columns are longer than its index (unindexed data) raises
// `input_error`. For raw matrices use the `matrix` overload, which returns the
// stored row-major buffer.
[[nodiscard]] std::vector<double> flatten(const dataframe& frame);

[[nodiscard]] std::vector<double> flatten(const matrix& values);

// Native replacement for the source `to_pandas` route (route kept under the
// name `fincore.metrics.basic.to_pandas` in the ledger, implementation named
// `to_dataframe`). pandas/NumPy identity is not claimed.
//
// 1-D input becomes a single-column frame with a positional 0..n-1 index
// (the native analogue of a RangeIndex); 2-D input becomes one double column
// per matrix column named "<prefix>_0", "<prefix>_1", ... with the same
// positional index. An existing frame is returned as an owning copy. Inputs
// with more than two dimensions cannot be represented by `matrix`, so the
// source ValueError branch has no native equivalent.
[[nodiscard]] dataframe to_dataframe(
    std::span<const double> values,
    std::string_view column = returns_column);

[[nodiscard]] dataframe to_dataframe(
    const matrix& values,
    std::string_view column_prefix = "column");

[[nodiscard]] dataframe to_dataframe(const dataframe& frame);

}  // namespace fincore
