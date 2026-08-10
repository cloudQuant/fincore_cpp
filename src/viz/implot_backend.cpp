#include <fincore/viz/implot_backend.hpp>

#include <implot.h>

#include <string>
#include <vector>

namespace fincore {
namespace {

void plot_series(const dataframe& frame, std::string_view column,
                 std::string_view title, std::string_view label) {
    const auto values = require_double_column(frame, column);
    if (values.empty()) {
        return;
    }

    std::vector<double> x;
    x.reserve(frame.get_index().size());
    for (const timestamp value : frame.get_index()) {
        x.push_back(static_cast<double>(value));
    }

    const std::string plot_title{title};
    const std::string series_label{label};
    if (ImPlot::BeginPlot(plot_title.data())) {
        ImPlot::PlotLine(series_label.data(), x.data(), values.data(),
                         static_cast<int>(values.size()));
        ImPlot::EndPlot();
    }
}

}  // namespace

void plot_cumulative_returns(const dataframe& frame, std::string_view column) {
    plot_series(frame, column, "Cumulative Returns", "Cumulative Returns");
}

void plot_drawdown(const dataframe& frame, std::string_view column) {
    plot_series(frame, column, "Drawdown", "Drawdown");
}

void plot_rolling_metric(const dataframe& frame, std::string_view column,
                         std::string_view label) {
    plot_series(frame, column, label, label);
}

}  // namespace fincore
