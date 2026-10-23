#pragma once

#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

struct report_html_options {
    std::string language = "en";
};

std::string render_html(
    const report_document& document,
    const report_html_options& options = {});

}  // namespace fincore
