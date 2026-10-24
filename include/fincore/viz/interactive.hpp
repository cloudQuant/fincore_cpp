#pragma once

#include <fincore/viz/bokeh_document.hpp>
#include <fincore/viz/chart_data.hpp>
#include <fincore/viz/html_builder.hpp>
#include <fincore/viz/plotly_document.hpp>

/**
 * Umbrella header for the Iteration 8 native interactive visualization
 * artifacts: Plotly-compatible and Bokeh-compatible chart documents plus the
 * self-contained HTML report builder.
 *
 * Native builds do not depend on Plotly, Bokeh, Python or a browser. Every
 * `show` entry point is headless and reports `NOT_RUN_INTERACTIVE`.
 */
namespace fincore::viz {

inline constexpr std::string_view interactive_backends[] = {
    "plotly", "bokeh", "html"};

}  // namespace fincore::viz
