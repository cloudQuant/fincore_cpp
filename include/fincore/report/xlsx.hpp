#pragma once

#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

struct xlsx_render_options {
    std::string sheet_prefix;
};

std::string render_xlsx(
    const report_document& document,
    const xlsx_render_options& options = {});

}  // namespace fincore
