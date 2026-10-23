#include <fincore/performance/returns.hpp>

#include <cmath>

namespace fincore {

double twr(const dataframe& returns, std::string_view column) {
    if (column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto values = require_double_column(returns, column);
    double product = 1.0;
    for (const double value : values) {
        if (!std::isnan(value)) {
            product *= 1.0 + value;
        }
    }
    return product - 1.0;
}

}  // namespace fincore
