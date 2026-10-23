#pragma once

#include <fincore/report/models.hpp>
#include <fincore/risk/report.hpp>

#include <string>

namespace fincore {

report_document build_risk_report(
    const risk_validation_report& report,
    std::string title = "Risk Validation Report");

}  // namespace fincore
