#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/errors.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Error thrown when a strict batch fetch fails for one or more symbols.
///
/// The payload uses category `BatchFetchError` and code
/// `fincore.data.batch_fetch`. The message names the provider and the failed
/// symbols in sorted order, and the flat fields hold `provider`, `symbols` and
/// `error_count`.
class batch_fetch_error : public fincore_error {
 public:
  /// Creates a batch fetch error.
  ///
  /// @param provider Provider name.
  /// @param errors Per-symbol error messages.
  /// @param partial_results Frames fetched successfully before the failure.
  batch_fetch_error(std::string provider,
                    std::map<std::string, std::string> errors,
                    std::map<std::string, dataframe> partial_results);

  /// Returns the provider that produced the failure.
  const std::string& provider() const noexcept { return provider_; }
  /// Returns the per-symbol error messages.
  const std::map<std::string, std::string>& errors() const noexcept {
    return errors_;
  }
  /// Returns the frames that were fetched successfully.
  const std::map<std::string, dataframe>& partial_results() const noexcept {
    return partial_results_;
  }

 private:
  std::string provider_;
  std::map<std::string, std::string> errors_;
  std::map<std::string, dataframe> partial_results_;
};

/// Native provider interface for price data.
///
/// Live SDK/network subclasses are EXTERNAL_OPTIONAL; the interface, batch
/// orchestration, date validation and return conversion are native and
/// offline-testable with an injected provider.
class data_provider {
 public:
  /// Destroys the provider.
  virtual ~data_provider() = default;

  /// Returns the provider's stable name.
  ///
  /// @return Provider name used in messages and batch errors.
  virtual std::string provider_name() const = 0;

  /// Fetches price data for one symbol.
  ///
  /// Implementations define how `start` and `end` are interpreted; this
  /// interface does not validate them. Exceptions derived from `std::exception`
  /// are recorded per symbol by fetch_multiple().
  ///
  /// @param symbol Instrument identifier.
  /// @param start Requested start of the range.
  /// @param end Requested end of the range.
  /// @param interval Requested interval label; defaults to `"1d"`.
  /// @param adjust Price adjustment flag; defaults to true.
  /// @return Fetched price frame.
  virtual dataframe fetch(std::string_view symbol, std::string_view start,
                          std::string_view end,
                          std::string_view interval = "1d",
                          bool adjust = true) const = 0;

  /// Returns provider-specific metadata for one symbol.
  ///
  /// @param symbol Instrument identifier.
  /// @return Key/value metadata for the symbol.
  virtual std::map<std::string, std::string> get_info(
      std::string_view symbol) const = 0;

  /// Returns the adjustment flag used when fetch_multiple() is not given one.
  ///
  /// @return True by default.
  virtual bool batch_adjust_default() const { return true; }

  /// Fetches several symbols through fetch().
  ///
  /// The adjustment flag is `adjust` when supplied, otherwise
  /// batch_adjust_default(). Every symbol is attempted. In non-strict mode a
  /// failed symbol is stored as an empty frame and its message is discarded; in
  /// strict mode the loop still attempts all symbols and then throws if any
  /// failed. `start` and `end` are passed to fetch() unchanged.
  ///
  /// @param symbols Instrument identifiers to fetch.
  /// @param start Requested start of the range.
  /// @param end Requested end of the range.
  /// @param interval Requested interval label; defaults to `"1d"`.
  /// @param adjust Optional adjustment override.
  /// @param strict Whether to throw when any symbol fails.
  /// @return Map from symbol to fetched frame. In non-strict mode failed
  ///     symbols map to empty frames.
  /// @throws batch_fetch_error In strict mode when at least one fetch fails;
  ///     its partial results hold the successful frames.
  std::map<std::string, dataframe> fetch_multiple(
      const std::vector<std::string>& symbols, std::string_view start,
      std::string_view end, std::string_view interval = "1d",
      std::optional<bool> adjust = std::nullopt, bool strict = false) const;

  /// Converts a price column to period-over-period simple returns.
  ///
  /// `column` is used when present; otherwise the method falls back to
  /// `"Close"`. Consecutive pairs where either price is NaN are skipped. A zero
  /// previous price with a nonzero current price yields signed infinity; zero
  /// over zero yields NaN and is skipped. Fewer than two prices yield an empty
  /// result.
  ///
  /// @param price_data Frame containing price data.
  /// @param column Preferred price column; defaults to `"Adj Close"`.
  /// @return Simple returns in index order.
  /// @throws input_error If the selected column is absent, is not a double
  ///     column, or does not match the index length. The fallback `"Close"` is
  ///     selected only when `column` is absent.
  std::vector<double> to_returns(const dataframe& price_data,
                                 std::string_view column = "Adj Close") const;

  /// Parses and validates an ISO-8601 UTC date range.
  ///
  /// Accepted forms are `YYYY-MM-DD`, optionally followed by `T` or a space and
  /// `HH:MM:SS` with an optional `.` fraction of up to nine digits. Omitted
  /// time components default to midnight. Values are interpreted as UTC; a
  /// trailing timezone designator is not accepted.
  ///
  /// @param start Range start.
  /// @param end Range end; must be strictly later than `start`.
  /// @return Pair of UTC nanosecond timestamps `{start, end}`.
  /// @throws fincore_error With category `ValidationError` for an unparsable
  ///     date or when `start` is not before `end`.
  static std::pair<timestamp, timestamp> validate_dates(
      std::string_view start, std::string_view end);
};

}  // namespace fincore
