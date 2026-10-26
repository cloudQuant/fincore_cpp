#include <fincore/viz/implot_backend.hpp>

#include <imgui.h>
#include <implot.h>

using plot_series_function = void (*)(const fincore::series_chart&);

int main() {
    // The function is never called here: taking its address is what obliges the
// installed implot component to export the symbol, and the volatile qualifier
// keeps the read of that address observable rather than a folded constant.
plot_series_function volatile plot_series = &fincore::plot_series_chart;
    auto* imgui_context = ImGui::CreateContext();
    auto* implot_context = ImPlot::CreateContext();
    // Identity with the created handles is required, not mere non-nullness: the
    // backend header documents its plot entry points as using whatever contexts
    // the caller has made current.
    const bool ready = imgui_context != nullptr && implot_context != nullptr &&
                       ImGui::GetCurrentContext() == imgui_context &&
                       ImPlot::GetCurrentContext() == implot_context &&
                       plot_series != nullptr;
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    return ready ? 0 : 1;
}
