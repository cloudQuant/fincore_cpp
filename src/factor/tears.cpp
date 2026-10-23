#include <fincore/factor/tears.hpp>

#include <cmath>
#include <string>
#include <utility>
#include <vector>

namespace fincore {
namespace {

chart_series series_for(const std::string& label,
                        const std::vector<timestamp>& dates,
                        const std::vector<double>& values) {
    chart_series series;
    series.label = label;
    for (std::size_t index = 0; index < dates.size(); ++index) {
        if (!std::isfinite(values[index])) {
            continue;
        }
        series.index.push_back(dates[index]);
        series.values.push_back(values[index]);
    }
    return series;
}

series_chart ic_chart(const factor_ic_table& information) {
    series_chart chart;
    chart.title = "Information coefficient";
    chart.y_label = "IC";
    chart.zero_line = true;
    const std::size_t dates = information.dates.size();
    const std::size_t labels = information.labels.size();
    for (std::size_t label = 0; label < labels; ++label) {
        std::vector<double> values(dates, 0.0);
        for (std::size_t date = 0; date < dates; ++date) {
            values[date] =
                information.values[date * labels + label];
        }
        chart.series.push_back(series_for(information.labels[label],
                                          information.dates, values));
    }
    return chart;
}

bar_chart quantile_chart(const quantile_return_table& quantile_returns) {
    bar_chart chart;
    const auto& quantiles = quantile_returns.quantiles;
    const auto& labels = quantile_returns.labels;
    chart.group_labels = labels;
    for (const int quantile : quantiles) {
        chart.series_labels.push_back("Q" + std::to_string(quantile));
    }
    for (std::size_t label = 0; label < labels.size(); ++label) {
        for (std::size_t quantile = 0; quantile < quantiles.size();
             ++quantile) {
            chart.values.push_back(
                quantile_returns.means[quantile * labels.size() + label]);
        }
    }
    return chart;
}

series_chart turnover_chart(
    const std::vector<factor_turnover_series>& turnover) {
    series_chart chart;
    chart.title = "Quantile turnover";
    chart.y_label = "Turnover";
    const int primary_lag =
        turnover.empty() ? 1 : turnover.front().lag;
    const auto& dates = turnover.empty()
                            ? std::vector<timestamp>{}
                            : turnover.front().values.dates;
    for (const auto& series : turnover) {
        if (series.lag != primary_lag) {
            continue;
        }
        chart.series.push_back(
            series_for("Q" + std::to_string(series.quantile), dates,
                       series.values.values));
    }
    return chart;
}

series_chart spread_chart(const quantile_return_table& quantile_returns,
                          const factor_return_table& factor_returns) {
    series_chart chart;
    chart.title = "Quantile spread";
    chart.y_label = "Top minus bottom";
    chart.zero_line = true;
    chart_series spread;
    spread.label = "Spread";
    const auto& quantiles = quantile_returns.quantiles;
    const auto& labels = quantile_returns.labels;
    if (!quantiles.empty()) {
        for (std::size_t label = 0; label < labels.size(); ++label) {
            const double top =
                quantile_returns.means[(quantiles.size() - 1U) *
                                           labels.size() +
                                       label];
            const double bottom = quantile_returns.means[label];
            if (std::isfinite(top) && std::isfinite(bottom)) {
                spread.index.push_back(
                    factor_returns.dates.empty()
                        ? label
                        : factor_returns.dates[label]);
                spread.values.push_back(top - bottom);
            }
        }
    }
    chart.series.push_back(std::move(spread));
    return chart;
}

series_chart rank_chart(
    const std::vector<factor_rank_autocorrelation_series>&
        autocorrelation) {
    series_chart chart;
    chart.title = "Rank autocorrelation";
    chart.y_label = "Autocorrelation";
    chart.zero_line = true;
    for (const auto& series : autocorrelation) {
        chart.series.push_back(series_for(
            "Lag " + std::to_string(series.lag), series.values.dates,
            series.values.values));
    }
    return chart;
}

}  // namespace

factor_tear_charts build_factor_tear_charts(
    const factor_analysis_model& model) {
    factor_tear_charts charts;
    charts.information_coefficient =
        ic_chart(model.information_coefficient);
    charts.quantile_returns =
        quantile_chart(model.mean_returns_by_quantile);
    charts.turnover = turnover_chart(model.quantile_turnover);
    charts.spread = spread_chart(model.mean_returns_by_quantile,
                                 model.factor_returns);
    charts.rank_autocorrelation = rank_chart(model.rank_autocorrelation);

    for (const auto& group : model.grouped_results) {
        factor_group_tear_charts grouped;
        grouped.group = group.group;
        grouped.information_coefficient =
            ic_chart(group.information_coefficient);
        grouped.quantile_returns =
            quantile_chart(group.mean_returns_by_quantile);
        grouped.turnover = turnover_chart(group.quantile_turnover);
        grouped.spread = spread_chart(group.mean_returns_by_quantile,
                                      group.factor_returns);
        grouped.rank_autocorrelation =
            rank_chart(group.rank_autocorrelation);
        charts.grouped.push_back(std::move(grouped));
    }
    return charts;
}

}  // namespace fincore
