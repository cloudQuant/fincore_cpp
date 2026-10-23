#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/report.hpp>
#include <fincore/report/html.hpp>

#include <cmath>
#include <cstdio>
#include <fstream>
#include <string>
#include <vector>

int main(int argc, char** argv) {
    if (argc != 2) {
        std::fprintf(stderr, "usage: %s OUTPUT.html\n", argv[0]);
        return 2;
    }
    fincore::prepared_factor_data data;
    data.forward_labels = {"1D", "5D"};
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    const auto day = 86'400'000'000'000LL;
    const char* assets[] = {"ALPHA", "BETA", "GAMMA", "DELTA", "EPSILON"};
    for (int date = 0; date < 40; ++date) {
        for (int asset = 0; asset < 5; ++asset) {
            fincore::factor_row row;
            row.date = start + date * day;
            row.asset = assets[asset];
            row.factor =
                0.4 * std::sin((date + 1) * 0.21 + asset * 0.8) +
                0.15 * std::cos((date + 1) * 0.07 + asset) +
                0.02 * asset;
            row.quantile = asset + 1;
            row.forward_returns = {
                0.002 * std::sin((date + 1) * 0.13 + asset * 0.7) +
                    0.0008 * asset,
                0.01 * std::cos((date + 1) * 0.05 + asset * 0.4) -
                    0.001 * asset};
            data.rows.push_back(row);
        }
    }
    fincore::factor_analysis_config config;
    config.periods = {"1D", "5D"};
    config.turnover_periods = {1, 5};
    config.include_portfolio_inputs = false;
    const auto model = fincore::analyze_factor(data, config);
    const auto document = fincore::build_factor_report(model, "Factor Report");
    const std::string html = fincore::render_html(document);
    std::ofstream stream(argv[1], std::ios::binary);
    if (!stream) {
        std::fprintf(stderr, "cannot open %s\n", argv[1]);
        return 1;
    }
    stream << html;
    std::printf("wrote %s (%zu bytes)\n", argv[1], html.size());
    return 0;
}
