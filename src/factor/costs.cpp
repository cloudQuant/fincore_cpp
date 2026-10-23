#include <fincore/factor/costs.hpp>

#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <cstddef>
#include <map>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct weight_panel {
    std::vector<timestamp> dates;
    std::vector<std::string> assets;
    std::vector<double> positions;  // dates x assets
    std::vector<double> trades;     // dates x assets
};

weight_panel build_weight_panel(const std::vector<factor_weight_row>& weights) {
    weight_panel panel;
    std::map<std::pair<timestamp, std::string>, double> lookup;
    std::set<timestamp> date_set;
    std::set<std::string> asset_set;
    for (const auto& row : weights) {
        if (!std::isfinite(row.weight)) {
            throw factor_data_error{
                "weights must use valid dates and finite values"};
        }
        if (!lookup.emplace(std::pair{row.date, row.asset}, row.weight)
                 .second) {
            throw factor_data_error{"weights index must be unique"};
        }
        date_set.insert(row.date);
        asset_set.insert(row.asset);
    }
    panel.dates.assign(date_set.begin(), date_set.end());
    panel.assets.assign(asset_set.begin(), asset_set.end());
    const std::size_t assets = panel.assets.size();
    panel.positions.assign(panel.dates.size() * assets, 0.0);
    std::map<std::string, std::size_t> asset_index;
    for (std::size_t index = 0; index < assets; ++index) {
        asset_index[panel.assets[index]] = index;
    }
    std::map<timestamp, std::size_t> date_index;
    for (std::size_t index = 0; index < panel.dates.size(); ++index) {
        date_index[panel.dates[index]] = index;
    }
    for (const auto& entry : lookup) {
        panel.positions[date_index[entry.first.first] * assets +
                        asset_index[entry.first.second]] = entry.second;
    }
    for (std::size_t date = 0; date < panel.dates.size(); ++date) {
        double gross = 0.0;
        for (std::size_t asset = 0; asset < assets; ++asset) {
            gross += std::abs(panel.positions[date * assets + asset]);
        }
        if (std::abs(gross - 1.0) > 1e-12) {
            throw factor_data_error{
                "weights must be gross-normalized to one on every date"};
        }
    }
    panel.trades.assign(panel.positions.size(), 0.0);
    for (std::size_t asset = 0; asset < assets; ++asset) {
        panel.trades[asset] = std::abs(panel.positions[asset]);
    }
    for (std::size_t date = 1; date < panel.dates.size(); ++date) {
        for (std::size_t asset = 0; asset < assets; ++asset) {
            panel.trades[date * assets + asset] =
                std::abs(panel.positions[date * assets + asset] -
                         panel.positions[(date - 1U) * assets + asset]);
        }
    }
    return panel;
}

std::vector<double> aligned_panel(const dataframe& frame,
                                  const std::vector<timestamp>& dates,
                                  const std::vector<std::string>& assets,
                                  const std::string_view name,
                                  const bool strictly_positive) {
    if (!frame.get_index().empty()) {
        for (std::size_t index = 1; index < frame.get_index().size();
             ++index) {
            if (!(frame.get_index()[index] > frame.get_index()[index - 1U])) {
                throw factor_data_error{
                    std::string(name) + " index must be unique and sorted"};
            }
        }
    }
    std::map<timestamp, std::size_t> row_index;
    for (std::size_t index = 0; index < frame.get_index().size(); ++index) {
        row_index[frame.get_index()[index]] = index;
    }
    std::map<std::string, std::size_t> asset_index;
    for (std::size_t index = 0; index < assets.size(); ++index) {
        asset_index[assets[index]] = index;
    }
    std::vector<double> result(dates.size() * assets.size(), 0.0);
    for (std::size_t date = 0; date < dates.size(); ++date) {
        const auto row = row_index.find(dates[date]);
        if (row == row_index.end()) {
            throw factor_data_error{
                std::string(name) + " must cover every weight date and asset"};
        }
        for (std::size_t asset = 0; asset < assets.size(); ++asset) {
            if (!frame.has_column(assets[asset].c_str())) {
                throw factor_data_error{
                    std::string(name) +
                    " must cover every weight date and asset"};
            }
            const auto& column =
                frame.get_column<double>(assets[asset].c_str());
            if (column.size() != frame.get_index().size()) {
                throw factor_data_error{
                    std::string(name) + " must be finite"};
            }
            const double value = column[row->second];
            if (!std::isfinite(value)) {
                throw factor_data_error{
                    std::string(name) + " must be finite"};
            }
            if (strictly_positive && value <= 0.0) {
                throw factor_data_error{
                    std::string(name) + " must be strictly positive"};
            }
            if (!strictly_positive && value < 0.0) {
                throw factor_data_error{
                    std::string(name) + " must be non-negative"};
            }
            result[date * assets.size() + asset] = value;
        }
    }
    return result;
}

factor_capacity_result capacity_from_trades(const weight_panel& panel,
                                            const std::vector<double>& volume,
                                            const double max_participation) {
    const std::size_t assets = panel.assets.size();
    factor_capacity_result result;
    result.dates = panel.dates;
    result.assets = panel.assets;
    result.trade_weights = panel.trades;
    result.max_participation = max_participation;
    std::vector<double> by_date(panel.dates.size(), 0.0);
    double maximum = std::numeric_limits<double>::infinity();
    bool any = false;
    for (std::size_t date = 0; date < panel.dates.size(); ++date) {
        double best = std::numeric_limits<double>::infinity();
        for (std::size_t asset = 0; asset < assets; ++asset) {
            const double trade = panel.trades[date * assets + asset];
            if (trade > 0.0) {
                best = std::min(best,
                                max_participation *
                                    volume[date * assets + asset] / trade);
            }
        }
        by_date[date] = best;
        if (std::isfinite(best)) {
            any = true;
            if (best < maximum) {
                maximum = best;
            }
        }
    }
    if (!any) {
        throw factor_data_error{
            "weights must contain at least one nonzero trade"};
    }
    result.maximum_portfolio_value = maximum;
    result.maximum_portfolio_value_by_date = by_date;
    bool found = false;
    for (std::size_t date = 0; date < panel.dates.size(); ++date) {
        if (!found && by_date[date] == maximum) {
            result.binding_date = panel.dates[date];
            found = true;
            double best = std::numeric_limits<double>::infinity();
            for (std::size_t asset = 0; asset < assets; ++asset) {
                const double trade = panel.trades[date * assets + asset];
                if (trade > 0.0 && volume[date * assets + asset] > 0.0) {
                    const double limit =
                        max_participation * volume[date * assets + asset] /
                        trade;
                    if (limit < best) {
                        best = limit;
                        result.binding_asset = panel.assets[asset];
                    }
                }
            }
            break;
        }
    }
    return result;
}

void require_model(const factor_cost_model& model) {
    if (!std::isfinite(model.half_spread_bps) ||
        model.half_spread_bps < 0.0) {
        throw factor_data_error{
            "half_spread_bps must be a finite non-negative number"};
    }
    if (!std::isfinite(model.impact_coefficient) ||
        model.impact_coefficient < 0.0) {
        throw factor_data_error{
            "impact_coefficient must be a finite non-negative number"};
    }
    if (!std::isfinite(model.impact_exponent) ||
        model.impact_exponent <= 0.0) {
        throw factor_data_error{"impact_exponent must be positive"};
    }
    if (!std::isfinite(model.max_participation) ||
        !(model.max_participation > 0.0 &&
          model.max_participation <= 1.0)) {
        throw factor_data_error{
            "max_participation must be in (0, 1]"};
    }
}

}  // namespace

factor_capacity_result estimate_factor_capacity(
    const std::vector<factor_weight_row>& weights,
    const dataframe& dollar_volume, const double max_participation) {
    if (!std::isfinite(max_participation) ||
        !(max_participation > 0.0 && max_participation <= 1.0)) {
        throw factor_data_error{
            "max_participation must be a finite number in (0, 1]"};
    }
    const auto panel = build_weight_panel(weights);
    const auto volume = aligned_panel(
        dollar_volume, panel.dates, panel.assets, "dollar_volume", true);
    return capacity_from_trades(panel, volume, max_participation);
}

factor_cost_result apply_factor_costs(
    const std::span<const double> gross_returns,
    const std::vector<factor_weight_row>& weights,
    const dataframe& dollar_volume, const double portfolio_value,
    const factor_cost_model& model, const dataframe* borrow_rates,
    const dataframe* borrow_available) {
    require_model(model);
    if (!std::isfinite(portfolio_value) || portfolio_value <= 0.0) {
        throw factor_data_error{
            "portfolio_value must be a positive finite number"};
    }
    const auto panel = build_weight_panel(weights);
    const std::size_t dates = panel.dates.size();
    const std::size_t assets = panel.assets.size();
    if (gross_returns.size() != dates) {
        throw factor_data_error{
            "gross_returns dates must exactly match weights dates"};
    }
    for (const double value : gross_returns) {
        if (!std::isfinite(value)) {
            throw factor_data_error{"gross_returns must be finite"};
        }
    }
    const auto volume = aligned_panel(
        dollar_volume, panel.dates, panel.assets, "dollar_volume", true);
    bool has_short = false;
    for (const double position : panel.positions) {
        if (position < 0.0) {
            has_short = true;
        }
    }
    std::vector<double> borrow(dates, 0.0);
    if (has_short) {
        if (borrow_rates == nullptr || borrow_available == nullptr) {
            throw factor_data_error{
                "short positions require both borrow_rates and "
                "borrow_available"};
        }
        const auto rates = aligned_panel(*borrow_rates, panel.dates,
                                         panel.assets, "borrow_rates",
                                         false);
        const auto availability = aligned_panel(
            *borrow_available, panel.dates, panel.assets, "borrow_available",
            false);
        for (std::size_t date = 0; date < dates; ++date) {
            for (std::size_t asset = 0; asset < assets; ++asset) {
                const double position =
                    panel.positions[date * assets + asset];
                if (position < 0.0) {
                    if (!(availability[date * assets + asset] == 1.0)) {
                        throw factor_data_error{
                            "borrow is unavailable for one or more short "
                            "positions"};
                    }
                    borrow[date] += -position * rates[date * assets + asset];
                }
            }
        }
    } else if (borrow_rates != nullptr || borrow_available != nullptr) {
        throw factor_data_error{
            "borrow inputs are only valid when weights contain short "
            "exposure"};
    }
    const auto capacity = capacity_from_trades(
        panel, volume, model.max_participation);
    if (portfolio_value > capacity.maximum_portfolio_value) {
        throw factor_data_error{
            "portfolio_value exceeds maximum capacity"};
    }
    factor_cost_result result;
    result.model = model;
    result.portfolio_value = portfolio_value;
    result.dates = panel.dates;
    result.assets = panel.assets;
    result.gross_returns.assign(gross_returns.begin(), gross_returns.end());
    result.net_returns.assign(dates, 0.0);
    result.turnover.assign(dates, 0.0);
    result.spread_cost.assign(dates, 0.0);
    result.impact_cost.assign(dates, 0.0);
    result.borrow_cost = borrow;
    result.total_cost.assign(dates, 0.0);
    result.trade_weights = panel.trades;
    result.participation.assign(dates * assets, 0.0);
    for (std::size_t date = 0; date < dates; ++date) {
        double trade_sum = 0.0;
        double impact = 0.0;
        for (std::size_t asset = 0; asset < assets; ++asset) {
            const std::size_t cell = date * assets + asset;
            const double trade = panel.trades[cell];
            trade_sum += trade;
            const double participation =
                trade * portfolio_value / volume[cell];
            result.participation[cell] = participation;
            if (!std::isfinite(participation)) {
                throw factor_data_error{
                    "cost ledger participation must remain finite"};
            }
            impact += trade * model.impact_coefficient *
                      std::pow(participation, model.impact_exponent);
        }
        result.turnover[date] = 0.5 * trade_sum;
        result.spread_cost[date] =
            trade_sum * model.half_spread_bps * 1.0e-4;
        result.impact_cost[date] = impact;
        result.total_cost[date] =
            result.spread_cost[date] + result.impact_cost[date] +
            result.borrow_cost[date];
        result.net_returns[date] =
            result.gross_returns[date] - result.total_cost[date];
        if (!std::isfinite(result.total_cost[date]) ||
            !std::isfinite(result.net_returns[date])) {
            throw factor_data_error{
                "cost ledger values must remain finite"};
        }
    }
    result.capacity = capacity;
    return result;
}

}  // namespace fincore
