#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/artifacts.hpp>
#include <fincore/factor/tears.hpp>
#include <fincore/viz/chart_data.hpp>
#include <fincore/viz/implot_backend.hpp>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <imgui.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <utility>
#include <vector>

namespace {

bool write_ppm(const char *path, const int width, const int height,
               const std::vector<unsigned char> &pixels) {
  // glReadPixels fills the buffer from the lower-left corner while a PPM image
  // starts at the top row, so the rows are emitted in reverse.
  FILE *stream = std::fopen(path, "wb");
  if (stream == nullptr) {
    return false;
  }
  std::fprintf(stream, "P6\n%d %d\n255\n", width, height);
  for (int row = height - 1; row >= 0; --row) {
    std::fwrite(pixels.data() + static_cast<std::size_t>(row * width * 3), 1,
                static_cast<std::size_t>(width * 3), stream);
  }
  std::fclose(stream);
  return true;
}

enum class dashboard_view { general, factor, groups, events, specialized };

dashboard_view selected_view() {
  // An unset variable and an unrecognized value both land on `general`, so the
  // caller cannot tell them apart from the enum alone; main re-reads the raw
  // value to decide whether to reject the request.
  const char *value = std::getenv("FINCORE_IMPLOT_VIEW");
  if (value == nullptr || std::string(value) == "general") {
    return dashboard_view::general;
  }
  if (std::string(value) == "factor") {
    return dashboard_view::factor;
  }
  if (std::string(value) == "groups") {
    return dashboard_view::groups;
  }
  if (std::string(value) == "events") {
    return dashboard_view::events;
  }
  if (std::string(value) == "specialized") {
    return dashboard_view::specialized;
  }
  std::fprintf(stderr,
               "FINCORE_IMPLOT_VIEW must be general, factor, groups, events, "
               "or specialized\n");
  return dashboard_view::general;
}

fincore::factor_analysis_model make_factor_demo_model() {
  fincore::prepared_factor_data data;
  data.forward_labels = {"1D", "5D"};
  const auto start = fincore::utc_timestamp(2021, 1, 15);
  constexpr fincore::timestamp day = 86'400'000'000'000LL;
  const char *assets[] = {"ALPHA", "BETA", "GAMMA", "DELTA", "EPSILON", "ZETA"};
  const char *groups[] = {"Growth", "Value"};
  std::vector<fincore::timestamp> return_dates;
  return_dates.reserve(60U);
  for (int date = 0; date < 60; ++date) {
    const auto timestamp = start + static_cast<fincore::timestamp>(date) * day;
    return_dates.push_back(timestamp);
    // Only every tenth date carries factor rows, and the event panel below uses
    // the same `date % 10` rule for its offsets, so the ledger events and the
    // factor observations land on one shared set of six dates.
    if (date % 10 != 5) {
      continue;
    }
    for (int asset = 0; asset < 6; ++asset) {
      const double factor = 0.55 * std::sin((date + 1) * 0.17 + asset * 0.6) +
                            0.18 * std::cos((date + 1) * 0.07 + asset * 0.3) +
                            0.12 * static_cast<double>(asset % 3);
      fincore::factor_row row;
      row.date = timestamp;
      row.asset = assets[asset];
      row.factor = factor;
      // Six assets fold into three quantiles (two each) and two groups (three
      // each), so the group and quantile panels show different partitions of the
      // same rows rather than different data.
      row.quantile = asset % 3 + 1;
      row.has_group = true;
      row.group = groups[asset / 3];
      row.forward_returns = {
          0.001 * std::sin((date + 1) * 0.21 + asset * 0.5) + 0.0006 * factor +
              0.0002 * (asset - 2),
          0.003 * std::cos((date + 1) * 0.09 + asset * 0.35) + 0.0011 * factor -
              0.0003 * (asset - 2)};
      data.rows.push_back(std::move(row));
    }
  }

  fincore::dataframe event_returns;
  event_returns.load_index(std::vector<fincore::timestamp>(return_dates));
  constexpr double event_profile[] = {-0.010, -0.004, 0.003, 0.022,
                                      0.012,  -0.006, -0.012};
  for (int asset = 0; asset < 6; ++asset) {
    std::vector<double> values;
    values.reserve(return_dates.size());
    for (int date = 0; date < 60; ++date) {
      const int offset = date % 10 - 5;
      const double event_return =
          offset >= -3 && offset <= 3
              ? event_profile[static_cast<std::size_t>(offset + 3)]
              : 0.0;
      const double quantile_scale = 0.6 + 0.3 * static_cast<double>(asset % 3);
      values.push_back(quantile_scale * event_return +
                       0.0005 * static_cast<double>(asset / 3 == 0 ? 1 : -1) +
                       0.0002 * std::sin(asset * 0.7));
    }
    event_returns.load_column<double>(assets[asset], std::move(values),
                                      hmdf::nan_policy::dont_pad_with_nans);
  }

  fincore::factor_analysis_config config;
  config.by_group = true;
  config.periods = {"1D", "5D"};
  config.turnover_periods = {1, 5};
  config.include_portfolio_inputs = false;
  // The demo event panel is a raw mean-return path. With long_short enabled,
  // event analysis demeans that same date mean and its fixture appears flat.
  config.long_short = false;
  // The two trailing arguments are the event window in ledger rows on each side
  // of an event date; three on each side plus the event day is the seven-value
  // profile built above.
  return fincore::analyze_factor(data, config, event_returns, 3, 3);
}

std::string event_offset_label(const fincore::timestamp offset) {
  const auto bars = static_cast<int>(offset);
  return bars < 0 ? "T" + std::to_string(bars) : "T+" + std::to_string(bars);
}

void plot_event_chart(const fincore::series_chart &chart) {
  if (chart.series.empty() || !ImPlot::BeginPlot(chart.title.c_str())) {
    return;
  }
  ImPlot::SetupAxes("Event offset (bars)", chart.y_label.c_str());
  const auto &tick_source = chart.series.front().index;
  std::vector<double> ticks;
  std::vector<std::string> labels;
  std::vector<const char *> label_pointers;
  ticks.reserve(tick_source.size());
  labels.reserve(tick_source.size());
  for (const auto offset : tick_source) {
    ticks.push_back(static_cast<double>(offset));
    labels.push_back(event_offset_label(offset));
  }
  for (const auto &label : labels) {
    // SetupAxisTicks takes a C array of pointers and copies each string into
    // its own buffer, so the strings above only have to survive until here.
    label_pointers.push_back(label.c_str());
  }
  // The range overload asserts n_ticks >= 2 whenever labels are supplied, which
  // is why a single-offset panel skips the custom ticks instead of passing one.
  if (ticks.size() > 1U) {
    ImPlot::SetupAxisTicks(ImAxis_X1, ticks.front(), ticks.back(),
                           static_cast<int>(ticks.size()),
                           label_pointers.data());
  }
  for (const auto &series : chart.series) {
    const std::size_t count =
        std::min(series.index.size(), series.values.size());
    std::vector<double> x;
    x.reserve(count);
    for (std::size_t index = 0; index < count; ++index) {
      x.push_back(static_cast<double>(series.index[index]));
    }
    ImPlot::PlotLine(series.label.c_str(), x.data(), series.values.data(),
                     static_cast<int>(count));
  }
  ImPlot::EndPlot();
}

void draw_factor_view(const fincore::factor_tear_charts &charts) {
  ImGui::SeparatorText("Information coefficient");
  fincore::plot_series_chart(charts.information_coefficient);
  ImGui::SeparatorText("Mean return by quantile and period");
  fincore::plot_bar_chart(charts.quantile_returns);
  ImGui::SeparatorText("Top minus bottom by period");
  fincore::plot_bar_chart(charts.spread);
  ImGui::SeparatorText("Quantile turnover");
  fincore::plot_series_chart(charts.turnover);
  ImGui::SeparatorText("Rank autocorrelation");
  fincore::plot_series_chart(charts.rank_autocorrelation);
}

void draw_group_view(const fincore::factor_tear_charts &charts) {
  if (charts.group_snapshots.empty()) {
    ImGui::TextUnformatted("No named group snapshots are available.");
    return;
  }
  for (const auto &group : charts.group_snapshots) {
    ImGui::SeparatorText(group.group.c_str());
    fincore::plot_series_chart(group.information_coefficient);
    fincore::plot_bar_chart(group.quantile_returns);
    fincore::plot_bar_chart(group.spread);
  }
}

void draw_event_view(const fincore::factor_tear_charts &charts) {
  if (!charts.event_analysis.has_value()) {
    ImGui::TextUnformatted("No event analysis is available.");
    return;
  }
  const auto &event = *charts.event_analysis;
  ImGui::SeparatorText("Mean return around events");
  plot_event_chart(event.mean_returns);
  ImGui::SeparatorText("Event return distribution");
  fincore::plot_histogram(event.return_distribution);
  ImGui::SeparatorText("Mean return by quantile and event offset");
  plot_event_chart(event.quantile_average_returns);
}

} // namespace

int main() {
  const auto view = selected_view();
  // A typo in the variable would otherwise render the general view silently, so
  // the raw value is checked once more here and rejected with its own exit code.
  const char *raw_view = std::getenv("FINCORE_IMPLOT_VIEW");
  if (raw_view != nullptr && view == dashboard_view::general &&
      std::string(raw_view) != "general") {
    return 2;
  }
  if (glfwInit() == GLFW_FALSE) {
    return 1;
  }

  glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
  glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
  glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
  glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GLFW_TRUE);
  const char *capture = std::getenv("FINCORE_IMPLOT_CAPTURE");
  if (capture != nullptr) {
    glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
  }

  // Capture mode renders into a hidden window and reads back a single frame, so
  // the per-view height is what decides whether every panel fits in the image;
  // the interactive path keeps the ordinary 720 pixels and scrolls instead.
  const int window_height_for_capture = view == dashboard_view::general  ? 3200
                                        : view == dashboard_view::factor ? 1700
                                        : view == dashboard_view::groups ? 2100
                                        : view == dashboard_view::events ? 1200
                                                                         : 2600;
  const int window_height =
      capture != nullptr ? window_height_for_capture : 720;
  GLFWwindow *window = glfwCreateWindow(
      1280, window_height, "fincore ImPlot dashboard", nullptr, nullptr);
  if (window == nullptr) {
    glfwTerminate();
    return 1;
  }

  glfwMakeContextCurrent(window);
  glfwSwapInterval(1);

  IMGUI_CHECKVERSION();
  ImGui::CreateContext();
  ImGui::GetIO().IniFilename = nullptr;
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
        0.01 * std::sin(position * 0.3) + 0.002 * std::cos(position * 0.7);
    daily.push_back(value);
    running = (1.0 + running) * (1.0 + value) - 1.0;
    cumulative.push_back(running);
    sharpe.push_back(std::sin(position * 0.1) * 1.5);
    peak = std::max(peak, running);
    drawdown.push_back((running - peak) / (1.0 + peak));
  }
  const auto daily_frame = fincore::make_series(index, "returns", daily);
  const auto cumulative_frame =
      fincore::make_series(index, "cumulative_returns", cumulative);
  const auto sharpe_frame = fincore::make_series(index, "sharpe", sharpe);
  const auto drawdown_frame = fincore::make_series(index, "drawdown", drawdown);
  const auto monthly = fincore::build_monthly_heatmap(daily_frame);
  const auto cumulative_chart = fincore::build_returns_chart(cumulative_frame);
  const auto sharpe_chart =
      fincore::build_rolling_sharpe_chart(sharpe_frame, "sharpe", 20);
  const auto drawdown_chart = fincore::build_drawdown_chart(drawdown_frame);
  const auto bars = fincore::build_bar_chart(
      {"Q1", "Q2"}, {"Strategy", "Benchmark"}, {0.01, 0.005, -0.02, 0.001});
  const auto histogram = fincore::build_histogram(daily, 8);
  const auto correlation = fincore::build_correlation_matrix(daily_frame);
  fincore::efficient_frontier_result frontier;
  frontier.frontier_volatilities = {0.05, 0.06, 0.07, 0.08};
  frontier.frontier_returns = {0.08, 0.12, 0.15, 0.17};
  frontier.frontier_sharpe = {1.2, 1.6, 1.8, 1.7};
  frontier.min_variance.volatility = 0.05;
  frontier.min_variance.expected_return = 0.08;
  frontier.max_sharpe.volatility = 0.07;
  frontier.max_sharpe.expected_return = 0.15;
  const auto frontier_chart = fincore::build_frontier_chart(frontier);
  const auto factor_model = make_factor_demo_model();
  const auto factor_charts = fincore::build_factor_tear_charts(factor_model);
  const auto quantile_cumulative =
      fincore::build_cumulative_returns_by_quantile(
          factor_model.mean_returns_by_date, "1D");
  const auto events_distribution = fincore::build_events_distribution(
      factor_model.mean_returns_by_date.dates);
  const auto qq_charts =
      fincore::build_ic_qq(factor_model.information_coefficient);
  const auto violin = fincore::build_quantile_returns_violin(
      factor_model.mean_returns_by_date, "1D");
  std::vector<double> spread_values;
  const auto& quantile_dates = factor_model.mean_returns_by_date;
  if (!quantile_dates.quantiles.empty() && !quantile_dates.labels.empty()) {
    const std::size_t quantile_count = quantile_dates.quantiles.size();
    for (std::size_t date = 0U; date < quantile_dates.dates.size(); ++date) {
      // `means` is flattened date-major, then quantile, then label, so one date's
      // block is quantile_count * labels wide and its top quantile starts a
      // label-count before the next block.
      const std::size_t base = date * quantile_count * quantile_dates.labels.size();
      const double top = quantile_dates.means.at(
          base + (quantile_count - 1U) * quantile_dates.labels.size());
      const double bottom = quantile_dates.means.at(base);
      spread_values.push_back(top - bottom);
    }
  }
  const auto spread = fincore::build_mean_quantile_returns_spread_time_series(
      quantile_dates.dates, spread_values, "1D");
  std::vector<fincore::monthly_ic_heatmap_chart> monthly_ic;
  if (!factor_model.time_aggregated_information.empty()) {
    monthly_ic = fincore::build_monthly_ic_heatmap(
        factor_model.time_aggregated_information.front());
  }

  const auto draw_specialized_view = [&] {
    ImGui::SeparatorText("Quantile cumulative returns");
    fincore::plot_quantile_cumulative_chart(quantile_cumulative);
    ImGui::SeparatorText("Events distribution");
    fincore::plot_events_distribution_chart(events_distribution);
    ImGui::SeparatorText("IC QQ");
    for (const auto& chart : qq_charts) {
      fincore::plot_qq_chart(chart);
    }
    ImGui::SeparatorText("Mean quantile spread");
    fincore::plot_spread_time_series(spread);
    ImGui::SeparatorText("Monthly IC heatmap");
    for (const auto& chart : monthly_ic) {
      fincore::plot_monthly_ic_heatmap(chart);
    }
    ImGui::SeparatorText("Quantile return violin");
    fincore::plot_violin_chart(violin);
  };

  const auto draw_dashboard = [&] {
    ImGui::SetNextWindowPos(ImVec2(0.0F, 0.0F));
    ImGui::SetNextWindowSize(
        ImVec2(1280.0F, static_cast<float>(window_height)));
    ImGui::Begin("fincore dashboard");
    if (view == dashboard_view::general) {
      fincore::plot_cumulative_returns(cumulative_frame, "cumulative_returns");
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
    } else if (view == dashboard_view::factor) {
      draw_factor_view(factor_charts);
    } else if (view == dashboard_view::groups) {
      draw_group_view(factor_charts);
    } else if (view == dashboard_view::specialized) {
      draw_specialized_view();
    } else {
      draw_event_view(factor_charts);
    }
    ImGui::End();
  };

  std::vector<unsigned char> pixels;
  int capture_width = 0;
  int capture_height = 0;
  for (int frame = 0;
       capture != nullptr ? frame < 3
                          : glfwWindowShouldClose(window) == GLFW_FALSE;
       ++frame) {
    glfwPollEvents();
    ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    draw_dashboard();
    ImGui::Render();

    int display_width = 0;
    int display_height = 0;
    glfwGetFramebufferSize(window, &display_width, &display_height);
    glViewport(0, 0, display_width, display_height);
    glClearColor(0.1F, 0.1F, 0.1F, 1.0F);
    glClear(GL_COLOR_BUFFER_BIT);
    ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
    if (capture != nullptr) {
      if (frame == 2) {
        glFinish();
        capture_width = display_width;
        capture_height = display_height;
        pixels.resize(static_cast<std::size_t>(display_width) *
                      static_cast<std::size_t>(display_height) * 3U);
        glReadPixels(0, 0, display_width, display_height, GL_RGB,
                     GL_UNSIGNED_BYTE, pixels.data());
      }
    } else {
      glfwSwapBuffers(window);
    }
  }

  int exit_code = 0;
  if (capture != nullptr) {
    exit_code =
        write_ppm(capture, capture_width, capture_height, pixels) ? 0 : 1;
  }

  ImGui_ImplOpenGL3_Shutdown();
  ImGui_ImplGlfw_Shutdown();
  ImPlot::DestroyContext();
  ImGui::DestroyContext();
  glfwDestroyWindow(window);
  glfwTerminate();
  return exit_code;
}
