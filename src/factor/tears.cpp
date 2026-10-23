#include <fincore/factor/tears.hpp>

#include <cmath>
#include <string>
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

}  // namespace

factor_tear_charts build_factor_tear_charts(
    const factor_analysis_model& model) {
    factor_tear_charts charts;
    charts.information_coefficient.title = "Information coefficient";
    charts.information_coefficient.y_label = "IC";
    charts.information_coefficient.zero_line = true;
    const std::size_t ic_dates =
        model.information_coefficient.dates.size();
    const std::size_t ic_labels =
        model.information_coefficient.labels.size();
    for (std::size_t label = 0; label < ic_labels; ++label) {
        std::vector<double> values(ic_dates, 0.0);
        for (std::size_t date = 0; date < ic_dates; ++date) {
            values[date] =
                model.information_coefficient.values[date * ic_labels +
                                                     label];
        }
        charts.information_coefficient.series.push_back(series_for(
            model.information_coefficient.labels[label],
            model.information_coefficient.dates, values));
    }

    const auto& quantiles = model.mean_returns_by_quantile.quantiles;
    const auto& labels = model.mean_returns_by_quantile.labels;
    charts.quantile_returns.group_labels = labels;
    for (const int quantile : quantiles) {
        charts.quantile_returns.series_labels.push_back(
            "Q" + std::to_string(quantile));
    }
    for (std::size_t label = 0; label < labels.size(); ++label) {
        for (std::size_t quantile = 0; quantile < quantiles.size();
             ++quantile) {
            charts.quantile_returns.values.push_back(
                model.mean_returns_by_quantile.means
                    [quantile * labels.size() + label]);
        }
    }

    charts.turnover.title = "Quantile turnover";
    charts.turnover.y_label = "Turnover";
    const auto& turnover_dates =
        model.quantile_turnover.empty()
            ? std::vector<timestamp>{}
            : model.quantile_turnover.front().dates;
    for (std::size_t index = 0; index < model.quantile_turnover.size();
         ++index) {
        charts.turnover.series.push_back(series_for(
            "Q" + std::to_string(index + 1U), turnover_dates,
            model.quantile_turnover[index].values));
    }

    charts.spread.title = "Quantile spread";
    charts.spread.y_label = "Top minus bottom";
    charts.spread.zero_line = true;
    chart_series spread;
    spread.label = "Spread";
    if (!quantiles.empty()) {
        for (std::size_t label = 0; label < labels.size(); ++label) {
            const double top =
                model.mean_returns_by_quantile.means
                    [(quantiles.size() - 1U) * labels.size() + label];
            const double bottom =
                model.mean_returns_by_quantile.means[label];
            if (std::isfinite(top) && std::isfinite(bottom)) {
                spread.index.push_back(
                    model.factor_returns.dates.empty()
                        ? label
                        : model.factor_returns.dates[label]);
                spread.values.push_back(top - bottom);
            }
        }
    }
    charts.spread.series.push_back(std::move(spread));

    charts.rank_autocorrelation.title = "Rank autocorrelation";
    charts.rank_autocorrelation.y_label = "Autocorrelation";
    charts.rank_autocorrelation.zero_line = true;
    charts.rank_autocorrelation.series.push_back(series_for(
        "Rank autocorrelation", model.rank_autocorrelation.dates,
        model.rank_autocorrelation.values));
    return charts;
}

}  // namespace fincore
