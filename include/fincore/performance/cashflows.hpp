#pragma once

#include <fincore/metrics/returns.hpp>

#include <optional>
#include <string_view>

namespace fincore {

/// Treatment of fees when computing cashflow-adjusted performance.
enum class fee_treatment {
    net,   ///< Uses valuations as supplied; fees do not adjust returns.
    gross  ///< Adds fees back to the closing valuation of their period.
};

/// Returns one cashflow-adjusted simple return per adjacent valuation pair.
///
/// The output is indexed by the later timestamp of each valuation pair and
/// stores decimal returns under ``returns``. A period return is
/// ``(closing - end_flows) / (opening + start_flows) - 1``; under
/// fee_treatment::gross the closing capital also includes fees recorded on
/// the closing valuation date. Cashflows and fees must fall on valuation
/// dates; same-day entries are summed, and nonzero entries on the first
/// valuation date are rejected.
///
/// @param valuations Valuation frame with a required double ``value_column``,
///     at least two rows and a strictly increasing, unique index. Values must
///     be finite and non-negative, and every value except the last must be
///     positive.
/// @param cashflows Optional ledger with double ``amount_column`` and
///     ``timing_column`` columns. Timing 1 marks a start-of-period flow and
///     timing 0 an end-of-period flow.
/// @param fees Optional fee ledger with a double ``fees_column``.
/// @param treatment Fee treatment used for closing valuations.
/// @param value_column Column holding valuations in ``valuations``.
/// @param amount_column Column holding signed cashflow amounts in
///     ``cashflows``.
/// @param timing_column Column holding the start/end timing flag in
///     ``cashflows``.
/// @param fees_column Column holding fee amounts in ``fees``.
/// @return Cashflow-adjusted simple returns indexed by the later timestamp of
///     each valuation pair.
/// @throws input_error If a column name is empty, the column is absent, not
///     double or misaligned;
///     the valuation index is not strictly increasing; a valuation is
///     non-finite or negative; an opening valuation is not positive; a
///     cashflow or fee date is absent from the valuation index; a timing flag
///     is not 0 or 1; a nonzero cashflow or fee falls on the first valuation
///     date; or a period capital base or return is not representable.
dataframe cashflow_adjusted_returns(
    const dataframe& valuations,
    const std::optional<dataframe>& cashflows = std::nullopt,
    const std::optional<dataframe>& fees = std::nullopt,
    fee_treatment treatment = fee_treatment::net,
    std::string_view value_column = "value",
    std::string_view amount_column = "amount",
    std::string_view timing_column = "timing",
    std::string_view fees_column = "fees");

/// Returns the compounded cashflow-adjusted time-weighted return.
///
/// Compounding multiplies ``1 + r`` over the per-period returns from
/// cashflow_adjusted_returns() and subtracts one.
///
/// @param valuations Valuation frame accepted by
///     cashflow_adjusted_returns().
/// @param cashflows Optional cashflow ledger accepted by
///     cashflow_adjusted_returns().
/// @param fees Optional fee ledger accepted by cashflow_adjusted_returns().
/// @param treatment Fee treatment used for closing valuations.
/// @param value_column Column holding valuations in ``valuations``.
/// @param amount_column Column holding signed cashflow amounts in
///     ``cashflows``.
/// @param timing_column Column holding the start/end timing flag in
///     ``cashflows``.
/// @param fees_column Column holding fee amounts in ``fees``.
/// @return Decimal cumulative time-weighted return over all periods.
/// @throws input_error If cashflow_adjusted_returns() rejects the inputs or
///     the compounded value is not representable.
double cashflow_adjusted_twr(
    const dataframe& valuations,
    const std::optional<dataframe>& cashflows = std::nullopt,
    const std::optional<dataframe>& fees = std::nullopt,
    fee_treatment treatment = fee_treatment::net,
    std::string_view value_column = "value",
    std::string_view amount_column = "amount",
    std::string_view timing_column = "timing",
    std::string_view fees_column = "fees");

}  // namespace fincore
