#include <fincore/performance/cashflows.hpp>

#include <cmath>
#include <map>
#include <string>
#include <vector>

namespace fincore {
namespace {

struct flow_components {
    std::vector<double> start;
    std::vector<double> end;
};

flow_components cashflow_sums(const std::optional<dataframe>& cashflows,
                              const dataframe& valuations,
                              std::string_view amount_column,
                              std::string_view timing_column) {
    const auto& index = valuations.get_index();
    flow_components result{std::vector<double>(index.size(), 0.0),
                           std::vector<double>(index.size(), 0.0)};
    if (!cashflows.has_value()) {
        return result;
    }
    const auto amounts = require_double_column(*cashflows, amount_column);
    const auto& ledger_index = cashflows->get_index();
    const auto timings = require_double_column(*cashflows, timing_column);
    std::map<timestamp, std::size_t> positions;
    for (std::size_t position = 0; position < index.size(); ++position) {
        positions[index[position]] = position;
    }
    for (std::size_t position = 0; position < amounts.size(); ++position) {
        if (!std::isfinite(amounts[position])) {
            throw input_error{"cashflow amounts must be finite"};
        }
        const auto found = positions.find(ledger_index[position]);
        if (found == positions.end()) {
            throw input_error{"cashflow dates must be present in the valuation index"};
        }
        if (timings[position] == 1.0) {
            result.start[found->second] += amounts[position];
        } else if (timings[position] == 0.0) {
            result.end[found->second] += amounts[position];
        } else {
            throw input_error{"cashflow timing must be 0 (end) or 1 (start)"};
        }
    }
    return result;
}

std::vector<double> aligned_fees(const std::optional<dataframe>& fees,
                                 const dataframe& valuations,
                                 std::string_view fees_column) {
    const auto& index = valuations.get_index();
    std::vector<double> result(index.size(), 0.0);
    if (!fees.has_value()) {
        return result;
    }
    const auto values = require_double_column(*fees, fees_column);
    const auto& fee_index = fees->get_index();
    std::map<timestamp, std::size_t> positions;
    for (std::size_t position = 0; position < index.size(); ++position) {
        positions[index[position]] = position;
    }
    for (std::size_t position = 0; position < values.size(); ++position) {
        if (!std::isfinite(values[position])) {
            throw input_error{"fees must be finite"};
        }
        const auto found = positions.find(fee_index[position]);
        if (found == positions.end()) {
            throw input_error{"fee dates must be present in the valuation index"};
        }
        result[found->second] += values[position];
    }
    return result;
}

std::vector<double> adjusted_returns(const dataframe& valuations,
                                     const std::optional<dataframe>& cashflows,
                                     const std::optional<dataframe>& fees,
                                     const fee_treatment treatment,
                                     const std::string_view value_column,
                                     const std::string_view amount_column,
                                     const std::string_view timing_column,
                                     const std::string_view fees_column) {
    const auto values = require_double_column(valuations, value_column);
    if (values.size() < 2) {
        throw input_error{"at least two valuations are required"};
    }
    const auto& index = valuations.get_index();
    for (std::size_t position = 1; position < index.size(); ++position) {
        if (!(index[position] > index[position - 1])) {
            throw input_error{
                "valuation index must be unique and monotonically increasing"};
        }
    }
    for (const double value : values) {
        if (!std::isfinite(value) || value < 0.0) {
            throw input_error{"valuations must be finite with a strictly positive opening capital"};
        }
    }
    for (std::size_t position = 0; position + 1 < values.size(); ++position) {
        if (values[position] <= 0.0) {
            throw input_error{"valuations must be finite with a strictly positive opening capital"};
        }
    }

    const auto flows =
        cashflow_sums(cashflows, valuations, amount_column, timing_column);
    const auto fee_values = aligned_fees(fees, valuations, fees_column);
    if (flows.start.front() != 0.0 || flows.end.front() != 0.0 ||
        fee_values.front() != 0.0) {
        throw input_error{"cashflows and fees on the first valuation date are not assignable to a period"};
    }

    std::vector<double> returns;
    returns.reserve(values.size() - 1);
    for (std::size_t period = 0; period + 1 < values.size(); ++period) {
        const double closing = values[period + 1];
        const double opening = values[period];
        const double closing_for_return =
            treatment == fee_treatment::gross ? closing + fee_values[period + 1]
                                              : closing;
        const double capital_base = opening + flows.start[period + 1];
        const double adjusted_closing =
            closing_for_return - flows.end[period + 1];
        if (!std::isfinite(closing_for_return)) {
            throw input_error{"fee adjustment produced non-finite closing capital"};
        }
        if (!std::isfinite(capital_base)) {
            throw input_error{"cashflow-adjusted capital base must be finite"};
        }
        if (!std::isfinite(adjusted_closing)) {
            throw input_error{"cashflow-adjusted closing capital must be finite"};
        }
        if (!(capital_base > 0.0)) {
            throw input_error{"cashflow-adjusted capital base must be strictly positive"};
        }
        if (adjusted_closing < 0.0) {
            throw input_error{"cashflow-adjusted closing capital must be non-negative"};
        }
        const double gross_factor = adjusted_closing / capital_base;
        const double period_return = gross_factor - 1.0;
        if (!std::isfinite(gross_factor) || !std::isfinite(period_return)) {
            throw input_error{"cashflow-adjusted returns must be finite"};
        }
        if (adjusted_closing > 0.0 && period_return == -1.0) {
            throw input_error{"cashflow-adjusted return factor is not representable"};
        }
        returns.push_back(period_return);
    }
    return returns;
}

}  // namespace

dataframe cashflow_adjusted_returns(
    const dataframe& valuations,
    const std::optional<dataframe>& cashflows,
    const std::optional<dataframe>& fees,
    const fee_treatment treatment,
    const std::string_view value_column,
    const std::string_view amount_column,
    const std::string_view timing_column,
    const std::string_view fees_column) {
    if (value_column.find('\0') != std::string_view::npos ||
        amount_column.find('\0') != std::string_view::npos ||
        timing_column.find('\0') != std::string_view::npos ||
        fees_column.find('\0') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    const auto returns = adjusted_returns(valuations, cashflows, fees, treatment,
                                          value_column, amount_column,
                                          timing_column, fees_column);
    return make_series(
        std::vector<timestamp>(valuations.get_index().begin() + 1,
                               valuations.get_index().end()),
        "returns", returns);
}

double cashflow_adjusted_twr(const dataframe& valuations,
                             const std::optional<dataframe>& cashflows,
                             const std::optional<dataframe>& fees,
                             const fee_treatment treatment,
                             const std::string_view value_column,
                             const std::string_view amount_column,
                             const std::string_view timing_column,
                             const std::string_view fees_column) {
    const auto returns = cashflow_adjusted_returns(
        valuations, cashflows, fees, treatment, value_column, amount_column,
        timing_column, fees_column);
    const auto values = require_double_column(returns, "returns");
    double compounded = 1.0;
    bool all_positive = true;
    for (const double value : values) {
        const double factor = 1.0 + value;
        compounded *= factor;
        if (!(factor > 0.0)) {
            all_positive = false;
        }
    }
    const double total = compounded - 1.0;
    if (!std::isfinite(compounded) || !std::isfinite(total)) {
        throw input_error{"cashflow-adjusted TWR must be finite"};
    }
    if ((compounded == 0.0 && all_positive) ||
        (compounded > 0.0 && total == -1.0)) {
        throw input_error{"cashflow-adjusted TWR factor is not representable"};
    }
    return total;
}

}  // namespace fincore
