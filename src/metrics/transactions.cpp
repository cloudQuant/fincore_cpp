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

struct transaction_bar_columns {
    std::span<const double> amounts;
    std::span<const double> prices;
    std::span<const double> volumes;
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

transaction_bar_columns required_transaction_bar_columns(
    const dataframe& transactions,
    const std::string_view amount_column_name,
    const std::string_view price_column_name,
    const std::string_view volume_column_name) {
    if (amount_column_name.empty() || price_column_name.empty() ||
        volume_column_name.empty()) {
        throw input_error{"transaction column names must not be empty"};
    }
    if (amount_column_name == price_column_name ||
        amount_column_name == volume_column_name ||
        price_column_name == volume_column_name) {
        throw input_error{"transaction columns must be distinct"};
    }
    return {require_double_column(transactions, amount_column_name),
            require_double_column(transactions, price_column_name),
            require_double_column(transactions, volume_column_name)};
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

dataframe apply_slippage_penalty(
    const dataframe& returns,
    const dataframe& transactions_with_bar_data,
    const double simulate_starting_capital,
    const double backtest_starting_capital,
    const double impact,
    const std::string_view returns_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name,
    const std::string_view volume_column_name) {
    if (backtest_starting_capital == 0.0) {
        throw input_error{"backtest starting capital must not be zero"};
    }

    const auto columns = required_transaction_bar_columns(
        transactions_with_bar_data, amount_column_name, price_column_name,
        volume_column_name);
    const double mult = simulate_starting_capital / backtest_starting_capital;
    std::map<timestamp, double> daily_penalties;
    const auto& transaction_index = transactions_with_bar_data.get_index();
    for (std::size_t row = 0U; row < transaction_index.size(); ++row) {
        const double shares = std::abs(mult * columns.amounts[row]);
        const double dollars = columns.prices[row] * shares;
        const double volume_share = shares / columns.volumes[row];
        const double penalty = volume_share * volume_share * impact * dollars;
        if (!std::isnan(penalty)) {
            daily_penalties[utc_midnight(transaction_index[row])] += penalty;
        }
    }

    const auto return_values =
        normalized_return_values(returns, returns_column_name);
    std::vector<timestamp> index;
    std::vector<double> values;
    index.reserve(return_values.size());
    values.reserve(return_values.size());
    double wealth = 1.0;
    for (const auto& [day, return_value] : return_values) {
        wealth *= 1.0 + (std::isnan(return_value) ? 0.0 : return_value);
        double portfolio_value =
            (wealth * backtest_starting_capital) * mult;
        if (portfolio_value == 0.0) {
            portfolio_value = nan;
        }

        const auto penalty_value = daily_penalties.find(day);
        double daily_penalty = penalty_value == daily_penalties.end()
                                   ? 0.0
                                   : penalty_value->second;
        if (std::isnan(daily_penalty)) {
            daily_penalty = 0.0;
        }
        double adjusted = return_value - daily_penalty / portfolio_value;
        if (std::isnan(adjusted)) {
            adjusted = return_value;
        }
        index.push_back(day);
        values.push_back(adjusted);
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

namespace {

constexpr long long nanoseconds_per_day = 86'400'000'000'000LL;

void require_same_index(const dataframe& left, const dataframe& right) {
    if (left.get_index() != right.get_index()) {
        throw input_error{"liquidity inputs must share the same index"};
    }
}

void validate_liquidity_parameters(const double max_bar_consumption,
                                   const double capital_base,
                                   const std::size_t mean_volume_window) {
    if (!(max_bar_consumption > 0.0 && max_bar_consumption <= 1.0)) {
        throw input_error{"max_bar_consumption must be in (0, 1]"};
    }
    if (!(capital_base > 0.0)) {
        throw input_error{"capital_base must be positive"};
    }
    if (mean_volume_window < 1U) {
        throw input_error{"mean_volume_window must be positive"};
    }
}

std::vector<double> rolling_mean_dollar_volume(
    const dataframe& prices, const dataframe& volumes,
    const std::string& symbol, const std::size_t window) {
    const auto& price_values = prices.get_column<double>(symbol.c_str());
    const auto& volume_values = volumes.get_column<double>(symbol.c_str());
    std::vector<double> result(price_values.size(), nan);
    for (std::size_t row = window; row < price_values.size(); ++row) {
        double total = 0.0;
        bool valid = true;
        for (std::size_t offset = row - window; offset < row; ++offset) {
            const double dollar_volume =
                price_values[offset] * volume_values[offset];
            if (std::isnan(dollar_volume)) {
                valid = false;
                break;
            }
            total += dollar_volume;
        }
        if (valid) {
            const double average = total / static_cast<double>(window);
            if (average != 0.0) {
                result[row] = average;
            }
        }
    }
    return result;
}

std::vector<output_column> liquidity_allocations(const dataframe& positions,
                                                 const std::string& cash_name) {
    const auto columns = numeric_position_columns(positions);
    const std::size_t row_count = positions.get_index().size();
    std::vector<output_column> result;
    for (const auto& column : columns) {
        if (column.name != cash_name) {
            result.push_back({column.name, std::vector<double>(row_count, nan)});
        }
    }
    for (std::size_t row = 0; row < row_count; ++row) {
        double total = 0.0;
        for (const auto& column : columns) {
            if (!std::isnan(column.values[row])) {
                total += column.values[row];
            }
        }
        std::size_t output = 0;
        for (const auto& column : columns) {
            if (column.name == cash_name) {
                continue;
            }
            result[output].values[row] =
                total == 0.0 ? nan : column.values[row] / total;
            ++output;
        }
    }
    return result;
}

}  // namespace

dataframe days_to_liquidate_positions(
    const dataframe& positions, const dataframe& prices,
    const dataframe& volumes, const double max_bar_consumption,
    const double capital_base, const std::size_t mean_volume_window,
    const std::string_view cash_column_name) {
    if (cash_column_name.find(' ') != std::string_view::npos) {
        throw input_error{"column name must not contain an embedded NUL"};
    }
    validate_liquidity_parameters(max_bar_consumption, capital_base,
                                  mean_volume_window);
    require_same_index(positions, prices);
    require_same_index(positions, volumes);
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto allocations = liquidity_allocations(positions, cash_name);
    const auto& index = positions.get_index();
    if (index.size() <= mean_volume_window) {
        return make_frame({}, {});
    }

    std::vector<timestamp> output_index(index.begin() +
                                            static_cast<std::ptrdiff_t>(
                                                mean_volume_window),
                                        index.end());
    std::vector<output_column> output;
    output.reserve(allocations.size());
    for (const auto& allocation : allocations) {
        const auto dollar_volumes = rolling_mean_dollar_volume(
            prices, volumes, allocation.name, mean_volume_window);
        std::vector<double> values(output_index.size(), nan);
        for (std::size_t row = mean_volume_window; row < index.size(); ++row) {
            const double allocation_value = allocation.values[row];
            const double dollar_volume = dollar_volumes[row];
            if (!std::isnan(allocation_value) && !std::isnan(dollar_volume)) {
                values[row - mean_volume_window] =
                    allocation_value * capital_base /
                    (max_bar_consumption * dollar_volume);
            }
        }
        output.push_back({allocation.name, std::move(values)});
    }
    return make_frame(std::move(output_index), std::move(output));
}

std::vector<ticker_liquidity> get_max_days_to_liquidate_by_ticker(
    const dataframe& positions, const dataframe& prices,
    const dataframe& volumes, const double max_bar_consumption,
    const double capital_base, const std::size_t mean_volume_window,
    const std::optional<std::size_t> last_n_days,
    const std::string_view cash_column_name) {
    const std::string cash_name = checked_cash_column_name(cash_column_name);
    const auto days = days_to_liquidate_positions(
        positions, prices, volumes, max_bar_consumption, capital_base,
        mean_volume_window, cash_column_name);
    const auto allocations = liquidity_allocations(positions, cash_name);
    const auto& day_index = days.get_index();
    std::optional<timestamp> newest;
    for (const timestamp point : day_index) {
        newest = newest.has_value() ? std::max(*newest, point) : point;
    }
    std::vector<bool> keep(day_index.size(), true);
    if (last_n_days.has_value() && newest.has_value()) {
        const long long cutoff =
            *newest - static_cast<long long>(*last_n_days) * nanoseconds_per_day;
        for (std::size_t row = 0; row < day_index.size(); ++row) {
            keep[row] = day_index[row] >= cutoff;
        }
    }

    std::vector<ticker_liquidity> result;
    for (const auto& allocation : allocations) {
        const auto& values = days.get_column<double>(allocation.name.c_str());
        std::size_t best_row = values.size();
        double best_days = nan;
        for (std::size_t row = 0; row < values.size(); ++row) {
            if (!keep[row] || std::isnan(values[row])) {
                continue;
            }
            if (best_row == values.size() || values[row] > best_days) {
                best_row = row;
                best_days = values[row];
            }
        }
        double allocation_pct = nan;
        if (best_row < values.size()) {
            for (std::size_t position = 0;
                 position < positions.get_index().size(); ++position) {
                if (positions.get_index()[position] == day_index[best_row]) {
                    allocation_pct = allocation.values[position] * 100.0;
                    break;
                }
            }
        }
        result.push_back(
            {allocation.name, best_days, allocation_pct});
    }
    std::sort(result.begin(), result.end(),
              [](const ticker_liquidity& left, const ticker_liquidity& right) {
                  return left.symbol < right.symbol;
              });
    return result;
}

std::vector<transaction_liquidity> get_low_liquidity_transactions(
    const std::span<const trade_record> transactions, const dataframe& prices,
    const dataframe& volumes, const std::optional<std::size_t> last_n_days) {
    require_same_index(prices, volumes);
    std::map<std::pair<std::string, timestamp>, double> daily_amounts;
    for (const auto& trade : transactions) {
        if (!std::isfinite(trade.amount)) {
            throw input_error{"transaction amounts must be finite"};
        }
        daily_amounts[{trade.symbol, trade.date}] += std::fabs(trade.amount);
    }
    if (daily_amounts.empty()) {
        return {};
    }
    std::optional<timestamp> newest;
    for (const auto& [key, amount] : daily_amounts) {
        static_cast<void>(amount);
        newest = newest.has_value() ? std::max(*newest, key.second) : key.second;
    }
    const long long cutoff =
        last_n_days.has_value()
            ? *newest - static_cast<long long>(*last_n_days) * nanoseconds_per_day
            : std::numeric_limits<long long>::min();

    std::map<timestamp, std::size_t> row_positions;
    for (std::size_t row = 0; row < prices.get_index().size(); ++row) {
        row_positions[prices.get_index()[row]] = row;
    }

    struct best_entry {
        bool seen = false;
        timestamp date{};
        double percent = nan;
    };
    std::map<std::string, best_entry> best;
    for (const auto& [key, amount] : daily_amounts) {
        if (key.second <= cutoff) {
            continue;
        }
        const auto row = row_positions.find(key.second);
        if (row == row_positions.end()) {
            throw input_error{"transaction dates must be present in the price index"};
        }
        double volume = nan;
        try {
            volume = volumes.get_column<double>(key.first.c_str())[row->second];
        } catch (const input_error&) {
            throw input_error{"every traded symbol must have a volume column"};
        }
        const double percent = amount / volume * 100.0;
        auto& entry = best[key.first];
        if (!entry.seen || percent > entry.percent) {
            entry = {true, key.second, percent};
        }
    }

    std::vector<transaction_liquidity> result;
    result.reserve(best.size());
    for (const auto& [symbol, entry] : best) {
        result.push_back({symbol, entry.date, entry.percent});
    }
    return result;
}

}  // namespace fincore
