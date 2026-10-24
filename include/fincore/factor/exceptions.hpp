#pragma once

#include <fincore/factor/data.hpp>

#include <stdexcept>
#include <string>
#include <utility>

namespace fincore {

class factor_data_error : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class factor_loss_exceeded : public factor_data_error {
public:
    factor_loss_exceeded(const std::string& message,
                         factor_loss_report report)
        : factor_data_error(message), loss_report(report) {}

    factor_loss_report loss_report;
};

/// Pinned strict identity for a factor/prices timezone mismatch.
class non_matching_timezone_error : public std::runtime_error {
public:
    explicit non_matching_timezone_error(
        const std::string& message = "factor and prices have different timezones")
        : std::runtime_error(message) {}
};

/// Enhanced-kernel timezone validation error (Python subclasses ValueError).
class enhanced_non_matching_timezone_error : public factor_data_error {
public:
    explicit enhanced_non_matching_timezone_error(
        const std::string& message = "factor and prices have different timezones")
        : factor_data_error(message) {}
};

/// Pinned strict identity for a rejected cleaning-loss budget.
class max_loss_exceeded_error : public std::runtime_error {
public:
    explicit max_loss_exceeded_error(std::string message = "",
                                     factor_loss_report report = {})
        : std::runtime_error(std::move(message)),
          loss_report(std::move(report)) {}

    factor_loss_report loss_report;
};

/// Source-name aliases preserving the enhanced/strict split.
using factor_loss_exceeded_error = factor_loss_exceeded;
using factor_data_error_alias = factor_data_error;

}  // namespace fincore
