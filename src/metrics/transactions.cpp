#include <fincore/metrics/transactions.hpp>

#include <fincore/calendar.hpp>
#include <fincore/error.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <limits>
#include <map>
#include <ranges>
#include <span>
#include <string>
#include <tuple>
#include <typeindex>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

struct transaction_columns {
    std::span<const double> amounts;
    std::span<const double> prices;
};

struct transaction_totals {
    double volume{0.0};
    double shares{0.0};
};

struct numeric_column {
    std::string name;
    std::span<const double> values;
};

struct output_column {
    std::string name;
    std::vector<double> values;
};

std::string checked_cash_column_name(const std::string_view cash_column_name) {
    if (cash_column_name.empty()) {
        throw input_error{"cash column name must not be empty"};
    }
    return std::string{cash_column_name};
}

transaction_columns required_transaction_columns(
    const dataframe& transactions,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    if (amount_column_name.empty() || price_column_name.empty()) {
        throw input_error{"transaction column names must not be empty"};
    }
    if (amount_column_name == price_column_name) {
        throw input_error{"transaction amount and price columns must be distinct"};
    }
    return {require_double_column(transactions, amount_column_name),
            require_double_column(transactions, price_column_name)};
}

timestamp utc_midnight(const timestamp value) {
    using namespace std::chrono;

    const sys_time<nanoseconds> point{nanoseconds{value}};
    const sys_days day{floor<days>(point)};
    const year_month_day calendar_day{day};
    return utc_timestamp(static_cast<int>(calendar_day.year()),
                         static_cast<unsigned>(calendar_day.month()),
                         static_cast<unsigned>(calendar_day.day()));
}

std::map<timestamp, transaction_totals> aggregate_transactions(
    const dataframe& transactions,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    const auto columns = required_transaction_columns(
        transactions, amount_column_name, price_column_name);
    const auto& index = transactions.get_index();

    std::map<timestamp, transaction_totals> daily_totals;
    for (std::size_t row = 0U; row < index.size(); ++row) {
        auto& totals = daily_totals[utc_midnight(index[row])];
        const double shares = std::abs(columns.amounts[row]);
        if (!std::isnan(shares)) {
            totals.shares += shares;
        }

        const double volume = shares * columns.prices[row];
        if (!std::isnan(volume)) {
            totals.volume += volume;
        }
    }
    return daily_totals;
}

dataframe make_frame(std::vector<timestamp> index,
                     std::vector<output_column> columns) {
    dataframe result;
    result.load_index(std::move(index));
    for (auto& column : columns) {
        result.load_column<double>(column.name.c_str(), std::move(column.values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

dataframe daily_transaction_frame(
    const std::map<timestamp, transaction_totals>& daily_totals) {
    std::vector<timestamp> index;
    std::vector<double> volumes;
    std::vector<double> shares;
    index.reserve(daily_totals.size());
    volumes.reserve(daily_totals.size());
    shares.reserve(daily_totals.size());
    for (const auto& [day, totals] : daily_totals) {
        index.push_back(day);
        volumes.push_back(totals.volume);
        shares.push_back(totals.shares);
    }
    return make_frame(std::move(index),
                      {{"txn_volume", std::move(volumes)},
                       {"txn_shares", std::move(shares)}});
}

std::vector<numeric_column> numeric_position_columns(const dataframe& positions) {
    const auto columns = positions.get_columns_info<double>();
    const auto& index = positions.get_index();

    std::vector<numeric_column> result;
    result.reserve(columns.size());
    for (const auto& info : columns) {
        if (std::get<2>(info) != typeid(double)) {
            continue;
        }

        const std::string name{std::get<0>(info).c_str()};
        const auto& values = positions.get_column<double>(name.c_str());
        if (values.size() != index.size()) {
            throw input_error{
                "numeric position columns must match the frame index length"};
        }
        result.push_back({name, values});
    }
    if (result.empty()) {
        throw input_error{"positions must contain at least one double column"};
    }
    return result;
}

std::vector<timestamp> normalized_position_days(const dataframe& positions) {
    const auto& index = positions.get_index();
    std::vector<timestamp> result;
    result.reserve(index.size());
    for (const timestamp value : index) {
        const timestamp day = utc_midnight(value);
        if (!result.empty() && day <= result.back()) {
            throw input_error{
                "position dates must be strictly increasing unique UTC days"};
        }
        result.push_back(day);
    }
    return result;
}

std::map<timestamp, double> normalized_return_values(
    const dataframe& returns, const std::string_view returns_column_name) {
    const auto values = require_double_column(returns, returns_column_name);
    const auto& index = returns.get_index();

    std::map<timestamp, double> result;
    timestamp previous_day{};
    bool has_previous_day = false;
    for (std::size_t row = 0U; row < index.size(); ++row) {
        const timestamp day = utc_midnight(index[row]);
        if (has_previous_day && day <= previous_day) {
            throw input_error{
                "return dates must be strictly increasing unique UTC days"};
        }
        result.emplace(day, values[row]);
        previous_day = day;
        has_previous_day = true;
    }
    return result;
}

double sum_skipping_nan(const std::vector<numeric_column>& columns,
                        std::size_t row);

std::map<timestamp, double> daily_portfolio_values(
    const dataframe& positions) {
    const auto columns = numeric_position_columns(positions);
    const auto days = normalized_position_days(positions);

    std::map<timestamp, double> result;
    for (std::size_t row = 0U; row < days.size(); ++row) {
        result.emplace(days[row], sum_skipping_nan(columns, row));
    }
    return result;
}

const numeric_column& required_cash_column(
    const std::vector<numeric_column>& columns,
    const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto cash = std::ranges::find_if(
        columns, [&cash_name](const numeric_column& column) {
            return column.name == cash_name;
        });
    if (cash == columns.end()) {
        throw input_error{"cash column must be a double position column"};
    }
    return *cash;
}

double sum_skipping_nan(const std::vector<numeric_column>& columns,
                        const std::size_t row) {
    double total = 0.0;
    for (const auto& column : columns) {
        const double value = column.values[row];
        if (!std::isnan(value)) {
            total += value;
        }
    }
    return total;
}

double gross_book(const std::vector<numeric_column>& columns,
                  const std::string_view cash_column_name,
                  const std::size_t row) {
    double total = 0.0;
    for (const auto& column : columns) {
        if (column.name == cash_column_name) {
            continue;
        }
        const double value = column.values[row];
        if (!std::isnan(value)) {
            total += std::abs(value);
        }
    }
    return total;
}

std::map<timestamp, double> actual_gross_book_denominators(
    const std::vector<timestamp>& days,
    const std::vector<numeric_column>& columns,
    const std::string_view cash_column_name) {
    static_cast<void>(required_cash_column(columns, cash_column_name));

    std::map<timestamp, double> result;
    double previous_gross = 0.0;
    for (std::size_t row = 0U; row < days.size(); ++row) {
        const double current_gross = gross_book(columns, cash_column_name, row);
        const double denominator =
            row == 0U ? current_gross / 2.0
                      : (previous_gross + current_gross) / 2.0;
        result.emplace(days[row], denominator);
        previous_gross = current_gross;
    }
    return result;
}

std::map<timestamp, double> portfolio_value_denominators(
    const std::vector<timestamp>& days,
    const std::vector<numeric_column>& columns) {
    std::map<timestamp, double> result;
    for (std::size_t row = 0U; row < days.size(); ++row) {
        result.emplace(days[row], sum_skipping_nan(columns, row));
    }
    return result;
}

std::map<timestamp, double> turnover_denominators(
    const std::vector<timestamp>& days,
    const std::vector<numeric_column>& columns,
    const turnover_denominator denominator,
    const std::string_view cash_column_name) {
    switch (denominator) {
    case turnover_denominator::actual_gross_book:
        return actual_gross_book_denominators(days, columns, cash_column_name);
    case turnover_denominator::portfolio_value:
        return portfolio_value_denominators(days, columns);
    }
    throw input_error{"unknown turnover denominator"};
}

}  // namespace

dataframe get_txn_vol(const dataframe& transactions,
                      const std::string_view amount_column_name,
                      const std::string_view price_column_name) {
    return daily_transaction_frame(aggregate_transactions(
        transactions, amount_column_name, price_column_name));
}

dataframe adjust_returns_for_slippage(
    const dataframe& returns,
    const dataframe& positions,
    const dataframe& transactions,
    const double slippage_bps,
    const std::string_view returns_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    const auto return_values =
        normalized_return_values(returns, returns_column_name);
    const auto portfolio_values = daily_portfolio_values(positions);
    const auto daily_transactions =
        get_txn_vol(transactions, amount_column_name, price_column_name);
    const auto transaction_volumes =
        require_double_column(daily_transactions, "txn_volume");

    std::map<timestamp, double> transaction_values;
    const auto& transaction_days = daily_transactions.get_index();
    for (std::size_t row = 0U; row < transaction_days.size(); ++row) {
        transaction_values.emplace(transaction_days[row], transaction_volumes[row]);
    }

    std::map<timestamp, double> union_days;
    for (const auto& [day, ignored] : return_values) {
        static_cast<void>(ignored);
        union_days.emplace(day, 0.0);
    }
    for (const auto& [day, ignored] : portfolio_values) {
        static_cast<void>(ignored);
        union_days.emplace(day, 0.0);
    }
    for (const auto& [day, ignored] : transaction_values) {
        static_cast<void>(ignored);
        union_days.emplace(day, 0.0);
    }

    std::vector<timestamp> index;
    std::vector<double> values;
    index.reserve(union_days.size());
    values.reserve(union_days.size());
    for (const auto& [day, ignored] : union_days) {
        static_cast<void>(ignored);
        const auto return_value = return_values.find(day);
        const auto portfolio_value = portfolio_values.find(day);
        const auto transaction_value = transaction_values.find(day);
        const double pnl =
            return_value != return_values.end() && portfolio_value != portfolio_values.end()
                ? return_value->second * portfolio_value->second
                : nan;
        const double transaction_cost =
            transaction_value == transaction_values.end()
                ? nan
                : transaction_value->second * (slippage_bps / 10000.0);
        const double adjusted_pnl = std::isnan(pnl) && std::isnan(transaction_cost)
                                        ? nan
                                        : (std::isnan(pnl) ? 0.0 : pnl) -
                                              (std::isnan(transaction_cost)
                                                   ? 0.0
                                                   : transaction_cost);
        const double pnl_safe = pnl == 0.0 ? nan : pnl;
        double adjusted_return = return_value == return_values.end()
                                     ? nan
                                     : return_value->second * adjusted_pnl / pnl_safe;
        if (std::isnan(adjusted_return)) {
            adjusted_return = 0.0;
        }
        index.push_back(day);
        values.push_back(adjusted_return);
    }
    return make_series(std::move(index), returns_column_name, std::move(values));
}

dataframe get_turnover(const dataframe& positions,
                       const dataframe& transactions,
                       const turnover_denominator denominator,
                       const std::string_view cash_column_name,
                       const std::string_view amount_column_name,
                       const std::string_view price_column_name) {
    if (denominator != turnover_denominator::actual_gross_book &&
        denominator != turnover_denominator::portfolio_value) {
        throw input_error{"unknown turnover denominator"};
    }

    const auto daily_transactions = aggregate_transactions(
        transactions, amount_column_name, price_column_name);
    const auto columns = numeric_position_columns(positions);
    const auto days = normalized_position_days(positions);
    const auto denominators =
        turnover_denominators(days, columns, denominator, cash_column_name);

    std::map<timestamp, double> union_days;
    for (const auto& [day, ignored] : denominators) {
        static_cast<void>(ignored);
        union_days.emplace(day, 0.0);
    }
    for (const auto& [day, ignored] : daily_transactions) {
        static_cast<void>(ignored);
        union_days.emplace(day, 0.0);
    }

    std::vector<timestamp> index;
    std::vector<double> values;
    index.reserve(union_days.size());
    values.reserve(union_days.size());
    for (const auto& [day, ignored] : union_days) {
        static_cast<void>(ignored);
        const auto transaction = daily_transactions.find(day);
        const double volume = transaction == daily_transactions.end()
                                  ? 0.0
                                  : transaction->second.volume;
        const auto denominator_value = denominators.find(day);
        const double divisor = denominator_value == denominators.end()
                                   ? nan
                                   : denominator_value->second;
        double value = volume / divisor;
        if (!std::isfinite(value)) {
            value = 0.0;
        }
        index.push_back(day);
        values.push_back(value);
    }
    return make_frame(std::move(index), {{"turnover", std::move(values)}});
}

}  // namespace fincore
