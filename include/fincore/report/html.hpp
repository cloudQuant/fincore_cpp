#pragma once

#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

enum class report_chart_backend {
  inline_svg,
  echarts,
};

struct report_html_options {
  std::string language = "en";
  report_chart_backend chart_backend = report_chart_backend::inline_svg;
};

std::string render_html(const report_document &document,
                        const report_html_options &options = {});

} // namespace fincore
