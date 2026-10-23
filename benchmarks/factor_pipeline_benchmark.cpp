#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/report.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/xlsx.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <string>
#include <utility>
#include <vector>

#if defined(__APPLE__)
#include <sys/resource.h>
#else
#include <sys/resource.h>
#endif

namespace {

constexpr std::int64_t day_ns = 86'400'000'000'000LL;

struct panel {
    fincore::prepared_factor_data data;
    std::size_t dates = 0U;
    std::size_t assets = 0U;
};

panel make_panel(const int date_count, const int assets, const int stride) {
    panel result;
    result.data.forward_labels = {"1D", "5D"};
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    for (int date = 0; date < date_count; ++date) {
        std::vector<std::pair<double, int>> ordered;
        for (int asset = 0; asset < assets; ++asset) {
            if (stride > 1 && (date * 7 + asset) % stride != 0) {
                continue;
            }
            const double factor =
                std::sin((date + 1) * 0.17 + asset * 0.9) +
                0.03 * static_cast<double>(asset % 11) +
                0.002 * std::cos((date + 1) * 0.05);
            ordered.emplace_back(factor, asset);
        }
        std::sort(ordered.begin(), ordered.end());
        const auto count = static_cast<int>(ordered.size());
        std::vector<int> quantiles(ordered.size(), 1);
        for (int rank = 0; rank < count; ++rank) {
            quantiles[static_cast<std::size_t>(rank)] =
                1 + rank * 5 / std::max(count, 1);
        }
        for (int rank = 0; rank < count; ++rank) {
            fincore::factor_row row;
            row.date = start + static_cast<std::int64_t>(date) * day_ns;
            row.asset = "A" + std::to_string(ordered[rank].second);
            row.factor = ordered[rank].first;
            row.quantile = quantiles[static_cast<std::size_t>(rank)];
            const int asset = ordered[rank].second;
            row.forward_returns = {
                0.002 * std::sin((date + 1) * 0.11 + asset * 0.7) +
                    0.0004 * static_cast<double>(asset % 13),
                0.01 * std::sin((date + 1) * 0.07 + asset * 0.3) -
                    0.0002 * static_cast<double>(asset % 7)};
            result.data.rows.push_back(std::move(row));
        }
    }
    result.dates = static_cast<std::size_t>(date_count);
    result.assets = static_cast<std::size_t>(assets);
    return result;
}

template <typename Function>
double milliseconds(Function&& function) {
    const auto start = std::chrono::steady_clock::now();
    function();
    return std::chrono::duration<double, std::milli>(
               std::chrono::steady_clock::now() - start)
        .count();
}

double rss_megabytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        return 0.0;
    }
#if defined(__APPLE__)
    return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
    return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
}

void run_case(const char* name, const int dates, const int assets,
              const int stride, const int repeats) {
    const auto panel = make_panel(dates, assets, stride);
    fincore::factor_analysis_config config;
    config.turnover_periods = {1, 5};
    config.time_aggregation = {"M"};
    double analyze_ms = 1.0e18;
    std::string fingerprint;
    for (int repeat = 0; repeat < repeats; ++repeat) {
        analyze_ms = std::min(
            analyze_ms, milliseconds([&panel, &config, &fingerprint] {
                const auto model =
                    fincore::analyze_factor(panel.data, config);
                fingerprint = model.result_fingerprint;
            }));
    }
    auto model = fincore::analyze_factor(panel.data, config);
    const bool stable = model.result_fingerprint == fingerprint;
    const double copy_ms = milliseconds([&model] {
        auto copy = model;
        copy.forward_periods.push_back("99D");
        static_cast<void>(copy.factor_data.rows.size());
    });
    auto copy = model;
    copy.forward_periods.push_back("99D");
    const bool isolated = model.forward_periods.size() == 2U;

    fincore::report_document document;
    const double report_ms = milliseconds(
        [&model, &document] { document = fincore::build_factor_report(model); });
    const double html_ms = milliseconds(
        [&document] { static_cast<void>(fincore::render_html(document)); });
    const auto html = fincore::render_html(document);
    const double xlsx_ms = milliseconds(
        [&document] { static_cast<void>(fincore::render_xlsx(document)); });
    const auto xlsx = fincore::render_xlsx(document);

    std::printf(
        "CASE %s dates=%zu assets=%zu rows=%zu analyze_ms=%.3f "
        "fingerprint_stable=%d copy_ms=%.3f isolated=%d report_ms=%.3f "
        "html_ms=%.3f html_bytes=%zu xlsx_ms=%.3f xlsx_bytes=%zu "
        "rss_mb=%.1f\n",
        name, panel.dates, panel.assets, panel.data.rows.size(), analyze_ms,
        stable ? 1 : 0, copy_ms, isolated ? 1 : 0, report_ms, html_ms,
        html.size(), xlsx_ms, xlsx.size(), rss_megabytes());
    std::fflush(stdout);
}

}  // namespace

int main() {
    std::printf("FINCORE_FACTOR_BENCHMARK schema=1\n");
    run_case("dense", 250, 100, 1, 3);
    run_case("sparse", 250, 1000, 20, 3);
    return 0;
}
