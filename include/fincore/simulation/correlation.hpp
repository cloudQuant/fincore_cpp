#pragma once

#include <fincore/dataframe.hpp>

#include <span>
#include <string_view>

namespace fincore {

/// Builds a square correlation breakdown matrix.
///
/// The matrix has one row and one double column per asset, in the supplied
/// order, with an ordinal index. Diagonal entries are 1.0 and every
/// off-diagonal entry is ``target_correlation``, which is retained even when
/// it lies outside [-1, 1]. Empty ``asset_names`` yields an empty frame.
///
/// @param asset_names Ordered asset names; entries must be non-empty,
///     distinct and free of embedded NULs.
/// @param target_correlation Finite off-diagonal correlation value.
/// @return A square frame with one double column per asset.
/// @throws input_error If a name is empty, duplicated or contains an embedded
///     NUL, the target is not finite, or the matrix is unrepresentable.
dataframe generate_correlation_breakdown(
    std::span<const std::string_view> asset_names,
    double target_correlation = 1.0);

}  // namespace fincore
