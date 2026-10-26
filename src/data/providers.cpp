#include <fincore/data/providers.hpp>

#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>

#include <algorithm>
#include <charconv>
#include <cmath>
#include <limits>
#include <stdexcept>
#include <string>
#include <utility>

namespace fincore {
namespace {

// Only the unadorned UTC form is accepted: a trailing Z or offset fails the
// separator test, a space may stand in for the T because feed payloads quote
// both ways, and a fraction longer than nine digits is rejected outright, which
// is what lets the accumulation loop below stay free of any overflow test.
timestamp parse_iso_utc(const std::string_view text) {
  const auto fail = []() -> timestamp {
    throw fincore_error{validation_error("Invalid date values")};
  };
  if (text.size() < 10U || text[4] != '-' || text[7] != '-') {
    return fail();
  }
  const auto number = [&text](const std::size_t offset,
                              const std::size_t length) -> int {
    int value = 0;
    const auto result =
        std::from_chars(text.data() + offset, text.data() + offset + length,
                        value);
    if (result.ec != std::errc{} ||
        result.ptr != text.data() + offset + length) {
      throw fincore_error{validation_error("Invalid date values")};
    }
    return value;
  };
  const int year = number(0U, 4U);
  const int month = number(5U, 2U);
  const int day = number(8U, 2U);
  int hour = 0;
  int minute = 0;
  int second = 0;
  unsigned nanosecond = 0U;
  if (text.size() > 10U) {
    if ((text[10] != 'T' && text[10] != ' ') || text.size() < 19U) {
      return fail();
    }
    hour = number(11U, 2U);
    if (text[13] != ':') {
      return fail();
    }
    minute = number(14U, 2U);
    if (text[16] != ':') {
      return fail();
    }
    second = number(17U, 2U);
    if (text.size() > 19U) {
      if (text[19] != '.' || text.size() > 29U) {
        return fail();
      }
      unsigned fraction = 0U;
      std::size_t digits = 0U;
      for (std::size_t index = 20U; index < text.size(); ++index) {
        if (text[index] < '0' || text[index] > '9') {
          return fail();
        }
        if (digits < 9U) {
          fraction = fraction * 10U +
                     static_cast<unsigned>(text[index] - '0');
          ++digits;
        }
      }
      while (digits < 9U) {
        fraction *= 10U;
        ++digits;
      }
      nanosecond = fraction;
    }
  }
  try {
    return utc_timestamp(year, static_cast<unsigned>(month),
                         static_cast<unsigned>(day),
                         static_cast<unsigned>(hour),
                         static_cast<unsigned>(minute),
                         static_cast<unsigned>(second), nanosecond);
  } catch (const std::exception&) {
    return fail();
  }
}

// Both helpers sort the failing symbols before joining them, so the order a map
// happens to iterate in cannot leak into an error string that callers compare or
// log; the two joins are kept identical on purpose so the structured fields and
// the sentence name the same symbols in the same sequence.
std::map<std::string, std::string> batch_error_fields(
    const std::string& provider,
    const std::map<std::string, std::string>& errors) {
  std::vector<std::string> symbols;
  symbols.reserve(errors.size());
  for (const auto& [symbol, message] : errors) {
    (void)message;
    symbols.push_back(symbol);
  }
  std::sort(symbols.begin(), symbols.end());
  std::string joined;
  for (std::size_t index = 0U; index < symbols.size(); ++index) {
    if (index != 0U) {
      joined += ", ";
    }
    joined += symbols[index];
  }
  return {{"provider", provider},
          {"symbols", joined},
          {"error_count", std::to_string(errors.size())}};
}

std::string batch_error_message(
    const std::string& provider,
    const std::map<std::string, std::string>& errors) {
  std::vector<std::string> symbols;
  symbols.reserve(errors.size());
  for (const auto& [symbol, message] : errors) {
    (void)message;
    symbols.push_back(symbol);
  }
  std::sort(symbols.begin(), symbols.end());
  std::string joined;
  for (std::size_t index = 0U; index < symbols.size(); ++index) {
    if (index != 0U) {
      joined += ", ";
    }
    joined += symbols[index];
  }
  return "Failed to fetch " + std::to_string(errors.size()) +
         " symbol(s) from " + provider + ": " + joined;
}

}  // namespace

batch_fetch_error::batch_fetch_error(
    std::string provider, std::map<std::string, std::string> errors,
    std::map<std::string, dataframe> partial_results)
    : fincore_error(make_error(
          "BatchFetchError", "fincore.data.batch_fetch",
          batch_error_message(provider, errors),
          batch_error_fields(provider, errors))),
      provider_(std::move(provider)),
      errors_(std::move(errors)),
      partial_results_(std::move(partial_results)) {}

std::map<std::string, dataframe> data_provider::fetch_multiple(
    const std::vector<std::string>& symbols, std::string_view start,
    std::string_view end, std::string_view interval,
    std::optional<bool> adjust, bool strict) const {
  const bool adjust_value = adjust.value_or(batch_adjust_default());
  std::map<std::string, dataframe> results;
  std::map<std::string, std::string> errors;
  // A lenient batch still keys the failed symbol, with an empty frame standing
  // in for the data, so a caller that indexes the result by symbol finds every
  // entry it asked for; strict mode keeps the same partial map but hands it to
  // the error instead of returning it.
  for (const auto& symbol : symbols) {
    try {
      results[symbol] = fetch(symbol, start, end, interval, adjust_value);
    } catch (const std::exception& error) {
      errors[symbol] = error.what();
      if (!strict) {
        results[symbol] = dataframe{};
      }
    }
  }
  if (strict && !errors.empty()) {
    throw batch_fetch_error(provider_name(), std::move(errors),
                            std::move(results));
  }
  return results;
}

std::vector<double> data_provider::to_returns(
    const dataframe& price_data, std::string_view column) const {
  std::string selected(column);
  if (!price_data.has_column(selected.c_str())) {
    selected = "Close";
  }
  const auto values = require_double_column(price_data, selected);
  std::vector<double> returns;
  if (values.size() < 2U) {
    return returns;
  }
  returns.reserve(values.size() - 1U);
  // Unusable steps are dropped rather than emitted as NaN, so the result can be
  // shorter than the price series minus one and is not positionally aligned with
  // the input. A zero price is kept as a signed infinity when the next print
  // moves, and as a dropped step when the price stays at zero, because no ratio
  // can say what 0 to 0 grew by.
  for (std::size_t index = 1U; index < values.size(); ++index) {
    const double previous = values[index - 1U];
    const double current = values[index];
    double change = std::numeric_limits<double>::quiet_NaN();
    if (!std::isnan(previous) && !std::isnan(current)) {
      if (previous == 0.0) {
        change = current == 0.0
                     ? std::numeric_limits<double>::quiet_NaN()
                     : std::copysign(std::numeric_limits<double>::infinity(),
                                     current);
      } else {
        change = (current - previous) / previous;
      }
    }
    if (!std::isnan(change)) {
      returns.push_back(change);
    }
  }
  return returns;
}

std::pair<timestamp, timestamp> data_provider::validate_dates(
    std::string_view start, std::string_view end) {
  const timestamp start_timestamp = parse_iso_utc(start);
  const timestamp end_timestamp = parse_iso_utc(end);
  if (start_timestamp >= end_timestamp) {
    throw fincore_error{
        validation_error("start date must be before end date")};
  }
  return {start_timestamp, end_timestamp};
}

}  // namespace fincore
