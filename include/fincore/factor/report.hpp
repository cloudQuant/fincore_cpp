#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

/// Builds a renderer-independent factor analysis report from a computed model.
///
/// The document carries summary, information-coefficient, returns and turnover
/// sections in that order.
///
/// @param model Computed factor analysis model.
/// @param title Document title.
/// @return Report document with domain `"factor"` and derived metadata.
report_document build_factor_report(
    const factor_analysis_model& model,
    std::string title = "Factor Analysis Report");

}  // namespace fincore
