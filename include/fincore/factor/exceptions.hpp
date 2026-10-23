#pragma once

#include <fincore/factor/data.hpp>

#include <stdexcept>

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

}  // namespace fincore
