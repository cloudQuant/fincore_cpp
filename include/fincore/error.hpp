#pragma once

#include <stdexcept>

namespace fincore {

/// Invalid caller input, derived from `std::invalid_argument`.
///
/// The type inherits the `std::invalid_argument` constructors. Calendar and
/// dataframe helpers such as utc_timestamp() and make_series() throw it for
/// rejected arguments.
class input_error : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

/// Optimization failure, derived from `std::runtime_error`.
///
/// The type inherits the `std::runtime_error` constructors. Optimization
/// routines throw it for solver-level failures.
class optimization_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace fincore
