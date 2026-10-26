#include <fincore/simulation/base.hpp>

#include <fincore/dataframe.hpp>
#include <fincore/errors.hpp>

#include <cmath>
#include <cstddef>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {
namespace {

[[noreturn]] void throw_returns_error(const char* const message) {
    throw fincore_error{validation_error(message, "returns")};
}

// NaN and infinity get separate messages because they are separate mistakes, and
// both stop the simulation here: unlike the parameter estimator, which skips a
// missing observation, a path generator refuses to invent one.
void require_finite_returns(const std::span<const double> returns) {
    for (const double value : returns) {
        if (std::isnan(value)) {
            throw_returns_error("Returns cannot contain NaN values");
        }
        if (!std::isfinite(value)) {
            throw_returns_error("Returns must be finite");
        }
    }
}

}  // namespace

std::vector<double> validate_returns(const std::span<const double> returns) {
    if (returns.empty()) {
        throw_returns_error("Returns cannot be empty");
    }
    require_finite_returns(returns);
    return std::vector<double>{returns.begin(), returns.end()};
}

std::vector<double> validate_returns(const dataframe& returns,
                                     const std::string_view column) {
    if (column.empty()) {
        throw_returns_error("Returns column must not be empty");
    }
    const std::string name{column};
    bool found = false;
    for (const auto& info : returns.get_columns_info<double>()) {
        if (std::string{std::get<0>(info).c_str()} == name) {
            found = true;
            break;
        }
    }
    // The search runs over double columns only, so a column of another type
    // carrying the right name is reported as missing rather than as wrongly
    // typed.
    if (!found) {
        throw fincore_error{validation_error(
            "Returns must contain a double column named '" + name + "'",
            "returns")};
    }
    const auto values = returns.get_column<double>(name.c_str());
    return validate_returns(std::span<const double>{values.data(), values.size()});
}

}  // namespace fincore
