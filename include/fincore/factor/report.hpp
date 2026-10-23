#pragma once

#include <fincore/factor/analysis.hpp>
#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

report_document build_factor_report(
    const factor_analysis_model& model,
    std::string title = "Factor Analysis Report");

}  // namespace fincore
