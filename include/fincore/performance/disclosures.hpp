#pragma once

#include <string>
#include <vector>

namespace fincore {

/// Display metadata rendered by render_disclosure().
///
/// Fields are free-form text and are not interpreted or validated. Empty
/// string fields render as ``unspecified``, except an empty ``benchmark``,
/// which renders as ``none``.
struct disclosure_context {
    std::string convention = "TWR";    ///< Return convention label.
    std::string sample_period;         ///< Sample-period description.
    std::string data_quality;          ///< Data-quality description.
    std::string fees = "gross-of-fees"; ///< Fee-basis label.
    std::string cashflows = "none";    ///< Cashflow-treatment label.
    std::string benchmark;             ///< Benchmark label; empty renders "none".
    std::string risk_free;             ///< Risk-free-rate description.
    bool annualized = true;            ///< Whether values are annualized.
    std::vector<std::string> notes;    ///< Extra notes, joined with "; ".
    std::string return_type;           ///< Return-type label.
    std::string units;                 ///< Unit label.
    std::string frequency;             ///< Observation-frequency label.
};

/// Returns a deterministic, newline-separated disclosure block.
///
/// Lines appear in this order: convention, return type, units, frequency,
/// sample period, data quality, fees, cashflows, benchmark, risk-free and
/// annualized. A non-empty ``notes`` list adds a final ``Notes: `` line whose
/// entries are joined by ``; ``; that final line has no trailing newline.
///
/// @param context Disclosure values to render.
/// @return Plain-text disclosure block.
std::string render_disclosure(const disclosure_context& context);

}  // namespace fincore
