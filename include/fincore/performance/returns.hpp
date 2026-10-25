#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

/// Returns the total time-weighted return of ``column``.
///
/// Multiplies ``1 + r`` over all non-NaN observations and subtracts one. NaN
/// observations are skipped; infinities and returns at or below -1 are used as
/// supplied. An empty series or a series containing only NaN yields 0.0.
///
/// @param returns Frame containing the return series.
/// @param column Double column of periodic simple returns.
/// @return Decimal cumulative simple return.
/// @throws input_error If the column name is empty, the column is absent, not
///     a double column, or does not match the frame index length.
double twr(const dataframe& returns,
           std::string_view column = returns_column);

/// Returns the annualized internal rate of return of dated cashflows.
///
/// Cashflows are summed by UTC calendar day and sorted by date; zero daily
/// totals are dropped. Period fractions use actual elapsed days divided by
/// 365. A rate is solved only for flows with exactly one sign change, and the
/// result is rejected when no representable root meets the solver's residual
/// budget.
///
/// @param cashflows Frame whose index holds cashflow dates.
/// @param amount_column Double column of signed cashflow amounts.
/// @return Decimal annualized rate, or NaN when the flows are not conventional
///     (not exactly one sign change) or no representable root is found.
/// @throws input_error If the column is invalid, an amount is non-finite, or
///     fewer than two distinct UTC cashflow dates are present.
double xirr(const dataframe& cashflows,
            std::string_view amount_column = "cashflow");

/// Returns the money-weighted return of periodic cashflows.
///
/// Uses row order, not timestamps, as the period axis. The first row is period
/// zero and each subsequent row is ``period`` intervals later. Zero amounts
/// are dropped after their positions are fixed. A rate is solved only for
/// flows with exactly one sign change.
///
/// @param cashflows Frame whose rows are cashflow periods.
/// @param period Optional positive finite interval between rows; defaults to
///     1.0.
/// @param amount_column Double column of signed cashflow amounts.
/// @return Decimal per-period internal rate of return, or NaN when the flows
///     are not conventional or no representable root is found.
/// @throws input_error If the column is invalid, fewer than two amounts are
///     present, an amount is non-finite, or ``period`` is not positive and
///     finite.
double mwr(const dataframe& cashflows,
           std::optional<double> period = std::nullopt,
           std::string_view amount_column = "cashflow");

}  // namespace fincore
