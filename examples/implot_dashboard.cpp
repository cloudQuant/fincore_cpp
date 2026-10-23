#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/viz/chart_data.hpp>
#include <fincore/viz/implot_backend.hpp>

#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <GLFW/glfw3.h>
#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <vector>

int main() {
    if (glfwInit() == GLFW_FALSE) {
        return 1;
    }

    GLFWwindow* window = glfwCreateWindow(1280, 720, "fincore ImPlot dashboard",
                                          nullptr, nullptr);
    if (window == nullptr) {
        glfwTerminate();
        return 1;
    }

    glfwMakeContextCurrent(window);
    glfwSwapInterval(1);

    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImPlot::CreateContext();
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init("#version 150");

    const auto returns = fincore::make_series(
        {1, 2, 3, 4, 5}, "cumulative_returns", {0.0, 0.01, 0.025, 0.02, 0.04});

    std::vector<fincore::timestamp> index;
    std::vector<double> daily;
    std::vector<double> cumulative;
    std::vector<double> sharpe;
    std::vector<double> drawdown;
    double running = 0.0;
    double peak = 0.0;
    for (int position = 0; position < 90; ++position) {
        index.push_back(fincore::utc_timestamp(2021, 1, 15) +
                        static_cast<fincore::timestamp>(position) *
                            86'400'000'000'000LL);
        const double value =
            0.01 * std::sin(position * 0.3) +
            0.002 * std::cos(position * 0.7);
        daily.push_back(value);
        running = (1.0 + running) * (1.0 + value) - 1.0;
        cumulative.push_back(running);
        sharpe.push_back(std::sin(position * 0.1) * 1.5);
        peak = std::max(peak, running);
        drawdown.push_back((running - peak) / (1.0 + peak));
    }
    const auto daily_frame =
        fincore::make_series(index, "returns", daily);
    const auto cumulative_frame =
        fincore::make_series(index, "cumulative_returns", cumulative);
    const auto sharpe_frame = fincore::make_series(index, "sharpe", sharpe);
    const auto drawdown_frame =
        fincore::make_series(index, "drawdown", drawdown);
    const auto monthly = fincore::build_monthly_heatmap(daily_frame);
    const auto cumulative_chart =
        fincore::build_returns_chart(cumulative_frame);
    const auto sharpe_chart = fincore::build_rolling_sharpe_chart(
        sharpe_frame, "sharpe", 20);
    const auto drawdown_chart =
        fincore::build_drawdown_chart(drawdown_frame);
    const auto bars = fincore::build_bar_chart(
        {"Q1", "Q2"}, {"Strategy", "Benchmark"},
        {0.01, 0.005, -0.02, 0.001});
    const auto histogram = fincore::build_histogram(daily, 8);
    const auto correlation =
        fincore::build_correlation_matrix(daily_frame);
    fincore::efficient_frontier_result frontier;
    frontier.frontier_volatilities = {0.05, 0.06, 0.07, 0.08};
    frontier.frontier_returns = {0.08, 0.12, 0.15, 0.17};
    frontier.frontier_sharpe = {1.2, 1.6, 1.8, 1.7};
    frontier.min_variance.volatility = 0.05;
    frontier.min_variance.expected_return = 0.08;
    frontier.max_sharpe.volatility = 0.07;
    frontier.max_sharpe.expected_return = 0.15;
    const auto frontier_chart = fincore::build_frontier_chart(frontier);

    while (glfwWindowShouldClose(window) == GLFW_FALSE) {
        glfwPollEvents();
        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        ImGui::Begin("fincore dashboard");
        fincore::plot_cumulative_returns(returns, "cumulative_returns");
        ImGui::SeparatorText("Cumulative returns");
        fincore::plot_series_chart(cumulative_chart);
        ImGui::SeparatorText("Monthly heatmap");
        fincore::plot_monthly_heatmap(monthly);
        ImGui::SeparatorText("Rolling Sharpe");
        fincore::plot_series_chart(sharpe_chart);
        ImGui::SeparatorText("Drawdown fill");
        fincore::plot_drawdown_chart(drawdown_chart);
        ImGui::SeparatorText("Grouped bars");
        fincore::plot_bar_chart(bars);
        ImGui::SeparatorText("Distribution");
        fincore::plot_histogram(histogram);
        ImGui::SeparatorText("Correlation");
        fincore::plot_correlation_matrix(correlation);
        ImGui::SeparatorText("Efficient frontier");
        fincore::plot_frontier_chart(frontier_chart);
        ImGui::End();

        ImGui::Render();
        int display_width = 0;
        int display_height = 0;
        glfwGetFramebufferSize(window, &display_width, &display_height);
        glViewport(0, 0, display_width, display_height);
        glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
        glfwSwapBuffers(window);
    }

    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImPlot::DestroyContext();
    ImGui::DestroyContext();
    glfwDestroyWindow(window);
    glfwTerminate();
    return 0;
}
