#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/errors.hpp>

#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

class batch_fetch_error : public fincore_error {
 public:
  batch_fetch_error(std::string provider,
                    std::map<std::string, std::string> errors,
                    std::map<std::string, dataframe> partial_results);

  const std::string& provider() const noexcept { return provider_; }
  const std::map<std::string, std::string>& errors() const noexcept {
    return errors_;
  }
  const std::map<std::string, dataframe>& partial_results() const noexcept {
    return partial_results_;
  }

 private:
  std::string provider_;
  std::map<std::string, std::string> errors_;
  std::map<std::string, dataframe> partial_results_;
};

// Native provider interface. Live SDK/network subclasses are EXTERNAL_OPTIONAL;
// the interface, batch orchestration, date validation and return conversion are
// native and offline-testable with an injected provider.
class data_provider {
 public:
  virtual ~data_provider() = default;

  virtual std::string provider_name() const = 0;
  virtual dataframe fetch(std::string_view symbol, std::string_view start,
                          std::string_view end,
                          std::string_view interval = "1d",
                          bool adjust = true) const = 0;
  virtual std::map<std::string, std::string> get_info(
      std::string_view symbol) const = 0;

  virtual bool batch_adjust_default() const { return true; }

  std::map<std::string, dataframe> fetch_multiple(
      const std::vector<std::string>& symbols, std::string_view start,
      std::string_view end, std::string_view interval = "1d",
      std::optional<bool> adjust = std::nullopt, bool strict = false) const;

  std::vector<double> to_returns(const dataframe& price_data,
                                 std::string_view column = "Adj Close") const;

  static std::pair<timestamp, timestamp> validate_dates(
      std::string_view start, std::string_view end);
};

}  // namespace fincore
