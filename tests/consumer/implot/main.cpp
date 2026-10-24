#include <fincore/viz/implot_backend.hpp>

#include <imgui.h>
#include <implot.h>

using plot_series_function = void (*)(const fincore::series_chart&);

int main() {
    plot_series_function volatile plot_series = &fincore::plot_series_chart;
    auto* imgui_context = ImGui::CreateContext();
    auto* implot_context = ImPlot::CreateContext();
    const bool ready = imgui_context != nullptr && implot_context != nullptr &&
                       ImGui::GetCurrentContext() == imgui_context &&
                       ImPlot::GetCurrentContext() == implot_context &&
                       plot_series != nullptr;
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    return ready ? 0 : 1;
}
