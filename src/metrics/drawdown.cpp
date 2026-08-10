#include <fincore/metrics/drawdown.hpp>

#include <cmath>
#include <limits>
#include <vector>

namespace fincore {

dataframe drawdown_series(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> drawdowns;
    drawdowns.reserve(values.size());

    double wealth = 100.0;
    double peak = wealth;
    for (const double value : values) {
        wealth *= 1.0 + (std::isnan(value) ? 0.0 : value);
        peak = std::fmax(peak, wealth);
        drawdowns.push_back((wealth - peak) / peak);
    }
    return make_series(returns.get_index(), returns_column, std::move(drawdowns));
}

double max_drawdown(const dataframe& returns, std::string_view column) {
    const auto values = require_double_column(returns, column);
    if (values.empty()) {
        return std::numeric_limits<double>::quiet_NaN();
    }

    const auto drawdowns = drawdown_series(returns, column);
    const auto drawdown_values =
        require_double_column(drawdowns, returns_column);
    double result = std::numeric_limits<double>::quiet_NaN();
    for (const double value : drawdown_values) {
        if (!std::isnan(value) && (std::isnan(result) || value < result)) {
            result = value;
        }
    }
    return result;
}

}  // namespace fincore
