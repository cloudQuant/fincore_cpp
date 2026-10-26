#include <fincore/metrics/basic.hpp>

#include <fincore/detail/numeric.hpp>
#include <fincore/error.hpp>

#include <utility>
#include <vector>

namespace fincore {

// An explicit value is returned untouched, including a zero or a negative one:
// the callers validate their own annualization before using the result, so this
// function stays the mapping table only and cannot silently repair an argument.
double annualization_factor(frequency period,
                            std::optional<double> annualization) {
    if (annualization.has_value()) {
        return *annualization;
    }

    switch (period) {
    case frequency::daily:
        return 252.0;
    case frequency::weekly:
        return 52.0;
    case frequency::monthly:
        return 12.0;
    case frequency::quarterly:
        return 4.0;
    case frequency::yearly:
        return 1.0;
    }
    throw input_error{"unrecognized frequency"};
}

dataframe adjust_returns(const dataframe& returns,
                         double adjustment_factor,
                         std::string_view column) {
    const auto values = require_double_column(returns, column);
    std::vector<double> adjusted_values{values.begin(), values.end()};
    for (double& value : adjusted_values) {
        value -= adjustment_factor;
    }

    const auto& index = returns.get_index();
    return make_series({index.begin(), index.end()}, column,
                       std::move(adjusted_values));
}

}  // namespace fincore
