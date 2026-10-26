#pragma once

#include <fincore/report/models.hpp>
#include <fincore/risk/report.hpp>

#include <string>

namespace fincore {

/// Builds a risk validation report document from a walk-forward VaR report.
///
/// The document has domain "risk" and one "risk_validation" section with the
/// walk-forward status, forecast and refit counts, optional diagnostics and
/// backtest metrics, forecast-event and refit tables, and the report
/// disclosure as a note.
///
/// @param report Native walk-forward risk validation report to project.
/// @param title Document title.
/// @return Report document using report_document_schema_version.
report_document build_risk_report(
    const risk_validation_report& report,
    std::string title = "Risk Validation Report");

}  // namespace fincore
