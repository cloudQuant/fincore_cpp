#include <fincore/metrics/round_trips.hpp>

#include <fincore/error.hpp>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <deque>
#include <limits>
#include <span>
#include <string>
#include <tuple>
#include <typeindex>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct transaction_fields {
    std::span<const std::string> symbols;
    std::span<const double> amounts;
    std::span<const double> prices;
};

struct raw_transaction {
    std::string symbol;
    double amount;
    double price;
    timestamp datetime;
    std::size_t position;
};

struct transaction_block {
    std::string symbol;
    double amount;
    double price;
    timestamp datetime;
};

struct queued_lot {
    double signed_price;
    double quantity;
    timestamp open_datetime;
};

struct round_trip {
    double pnl;
    timestamp open_datetime;
    timestamp close_datetime;
    bool is_long;
    double returns;
    std::string symbol;
    timestamp duration;
};

std::array<std::string, 3U> checked_field_names(
    const std::string_view symbol_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    std::array<std::string, 3U> names{std::string{symbol_column_name},
                                      std::string{amount_column_name},
                                      std::string{price_column_name}};
    for (const auto& name : names) {
        if (name.empty()) {
            throw input_error{"round-trip field names must not be empty"};
        }
    }
    if (names[0] == names[1] || names[0] == names[2] || names[1] == names[2]) {
        throw input_error{"round-trip field names must be distinct"};
    }
    return names;
}

std::span<const std::string> require_string_column(const dataframe& frame,
                                                    const std::string& name) {
    if (!frame.has_column(name.c_str())) {
        throw input_error{"required string column is absent"};
    }
    const auto columns = frame.get_columns_info<std::string>();
    const auto column = std::ranges::find_if(
        columns, [&name](const auto& info) {
            return std::get<0>(info) == name.c_str() &&
                   std::get<2>(info) == typeid(std::string);
        });
    if (column == columns.end()) {
        throw input_error{"required column is not a string column"};
    }
    const auto& values = frame.get_column<std::string>(name.c_str());
    if (values.size() != frame.get_index().size()) {
        throw input_error{"required string column must match the frame index length"};
    }
    return values;
}

transaction_fields required_transaction_fields(
    const dataframe& transactions,
    const std::string_view symbol_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    const auto names = checked_field_names(symbol_column_name, amount_column_name,
                                           price_column_name);
    return {require_string_column(transactions, names[0]),
            require_double_column(transactions, names[1]),
            require_double_column(transactions, names[2])};
}

bool same_direction(const double left, const double right) {
    return (left > 0.0) == (right > 0.0);
}

bool gap_exceeds(const timestamp earlier,
                 const timestamp later,
                 const std::chrono::nanoseconds max_delta) {
    if (max_delta.count() < 0) {
        return true;
    }
    const auto gap = static_cast<std::uint64_t>(later) -
                     static_cast<std::uint64_t>(earlier);
    return gap > static_cast<std::uint64_t>(max_delta.count());
}

std::vector<transaction_block> consolidated_blocks(
    const dataframe& transactions,
    const std::string_view symbol_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name,
    const std::chrono::nanoseconds max_delta) {
    const auto fields = required_transaction_fields(
        transactions, symbol_column_name, amount_column_name, price_column_name);
    const auto& index = transactions.get_index();

    std::vector<raw_transaction> rows;
    rows.reserve(index.size());
    for (std::size_t position = 0U; position < index.size(); ++position) {
        rows.push_back({fields.symbols[position], fields.amounts[position],
                        fields.prices[position], index[position], position});
    }
    std::stable_sort(rows.begin(), rows.end(), [](const raw_transaction& left,
                                                  const raw_transaction& right) {
        return std::tie(left.symbol, left.datetime, left.position) <
               std::tie(right.symbol, right.datetime, right.position);
    });

    std::vector<transaction_block> blocks;
    for (std::size_t position = 0U; position < rows.size();) {
        const raw_transaction& first = rows[position];
        double amount = 0.0;
        double notional = 0.0;
        std::size_t end = position;
        do {
            amount += rows[end].amount;
            notional += rows[end].amount * rows[end].price;
            ++end;
        } while (end < rows.size() && rows[end].symbol == first.symbol &&
                 same_direction(rows[end - 1U].amount, rows[end].amount) &&
                 !gap_exceeds(rows[end - 1U].datetime, rows[end].datetime, max_delta));
        blocks.push_back({first.symbol, amount, notional / amount, first.datetime});
        position = end;
    }
    return blocks;
}

dataframe make_block_frame(const std::vector<transaction_block>& blocks,
                           const std::string_view amount_column_name,
                           const std::string_view symbol_column_name,
                           const std::string_view price_column_name) {
    std::vector<timestamp> index;
    std::vector<std::string> symbols;
    std::vector<double> amounts;
    std::vector<double> prices;
    index.reserve(blocks.size());
    symbols.reserve(blocks.size());
    amounts.reserve(blocks.size());
    prices.reserve(blocks.size());
    for (const auto& block : blocks) {
        index.push_back(block.datetime);
        symbols.push_back(block.symbol);
        amounts.push_back(block.amount);
        prices.push_back(block.price);
    }

    dataframe result;
    result.load_index(std::move(index));
    const std::string amount_name{amount_column_name};
    const std::string symbol_name{symbol_column_name};
    const std::string price_name{price_column_name};
    result.load_column<double>(amount_name.c_str(), std::move(amounts),
                               hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<std::string>(symbol_name.c_str(), std::move(symbols),
                                    hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>(price_name.c_str(), std::move(prices),
                               hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

double amount_sign(const double amount) {
    if (amount > 0.0) {
        return 1.0;
    }
    if (amount < 0.0) {
        return -1.0;
    }
    return amount;
}

bool same_lot_direction(const double signed_price, const double amount) {
    return std::signbit(signed_price) == std::signbit(amount);
}

timestamp checked_duration(const timestamp earlier, const timestamp later) {
    const auto difference = static_cast<std::uint64_t>(later) -
                            static_cast<std::uint64_t>(earlier);
    if (difference >
        static_cast<std::uint64_t>(std::numeric_limits<timestamp>::max())) {
        throw input_error{"round-trip duration cannot be represented by timestamp"};
    }
    return static_cast<timestamp>(difference);
}

std::vector<round_trip> fifo_round_trips(
    const std::vector<transaction_block>& blocks) {
    std::vector<round_trip> result;
    std::deque<queued_lot> lots;
    std::string current_symbol;
    bool has_symbol = false;

    for (const auto& block : blocks) {
        if (!has_symbol || block.symbol != current_symbol) {
            lots.clear();
            current_symbol = block.symbol;
            has_symbol = true;
        }
        if (block.price < 0.0) {
            continue;
        }

        const double signed_price = block.price * amount_sign(block.amount);
        double remaining = std::abs(block.amount);
        if (lots.empty() || same_lot_direction(lots.back().signed_price, block.amount)) {
            lots.push_back({signed_price, remaining, block.datetime});
            continue;
        }

        double pnl = 0.0;
        double invested = 0.0;
        timestamp first_open_datetime{};
        bool matched_any = false;
        while (remaining > 0.0 && !lots.empty() &&
               !same_lot_direction(lots.front().signed_price, signed_price)) {
            const queued_lot& previous = lots.front();
            if (!matched_any) {
                first_open_datetime = previous.open_datetime;
                matched_any = true;
            }
            const double matched = std::min(remaining, previous.quantity);
            pnl += -(signed_price + previous.signed_price) * matched;
            invested += std::abs(previous.signed_price) * matched;

            if (matched == previous.quantity) {
                lots.pop_front();
            } else {
                lots.front().quantity -= matched;
            }
            remaining -= matched;
        }
        if (matched_any) {
            result.push_back({pnl,
                              first_open_datetime,
                              block.datetime,
                              signed_price < 0.0,
                              invested != 0.0 ? pnl / invested : 0.0,
                              block.symbol,
                              checked_duration(first_open_datetime, block.datetime)});
        }
        if (remaining > 0.0) {
            lots.push_back({signed_price, remaining, block.datetime});
        }
    }
    return result;
}

dataframe make_round_trip_frame(const std::vector<round_trip>& round_trips) {
    std::vector<timestamp> index;
    std::vector<double> pnls;
    std::vector<timestamp> open_datetimes;
    std::vector<timestamp> close_datetimes;
    std::vector<bool> longs;
    std::vector<double> returns;
    std::vector<std::string> symbols;
    std::vector<timestamp> durations;
    index.reserve(round_trips.size());
    pnls.reserve(round_trips.size());
    open_datetimes.reserve(round_trips.size());
    close_datetimes.reserve(round_trips.size());
    longs.reserve(round_trips.size());
    returns.reserve(round_trips.size());
    symbols.reserve(round_trips.size());
    durations.reserve(round_trips.size());
    for (std::size_t position = 0U; position < round_trips.size(); ++position) {
        const auto& value = round_trips[position];
        index.push_back(static_cast<timestamp>(position));
        pnls.push_back(value.pnl);
        open_datetimes.push_back(value.open_datetime);
        close_datetimes.push_back(value.close_datetime);
        longs.push_back(value.is_long);
        returns.push_back(value.returns);
        symbols.push_back(value.symbol);
        durations.push_back(value.duration);
    }

    dataframe result;
    result.load_index(std::move(index));
    result.load_column<double>(round_trip_pnl_column.data(), std::move(pnls),
                               hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<timestamp>(round_trip_open_datetime_column.data(),
                                  std::move(open_datetimes),
                                  hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<timestamp>(round_trip_close_datetime_column.data(),
                                  std::move(close_datetimes),
                                  hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<bool>(round_trip_long_column.data(), std::move(longs),
                             hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<double>(round_trip_return_column.data(), std::move(returns),
                               hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<std::string>(transaction_symbol_column.data(), std::move(symbols),
                                    hmdf::nan_policy::dont_pad_with_nans);
    result.load_column<timestamp>(round_trip_duration_column.data(), std::move(durations),
                                  hmdf::nan_policy::dont_pad_with_nans);
    return result;
}

}  // namespace

dataframe groupby_consecutive_transactions(
    const dataframe& transactions,
    const std::chrono::nanoseconds max_delta,
    const std::string_view symbol_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    return make_block_frame(consolidated_blocks(transactions, symbol_column_name,
                                                amount_column_name, price_column_name,
                                                max_delta),
                            amount_column_name, symbol_column_name, price_column_name);
}

dataframe extract_round_trips(
    const dataframe& transactions,
    const std::chrono::nanoseconds max_delta,
    const std::string_view symbol_column_name,
    const std::string_view amount_column_name,
    const std::string_view price_column_name) {
    const auto blocks = consolidated_blocks(transactions, symbol_column_name,
                                            amount_column_name, price_column_name,
                                            max_delta);
    return make_round_trip_frame(fifo_round_trips(blocks));
}

}  // namespace fincore
