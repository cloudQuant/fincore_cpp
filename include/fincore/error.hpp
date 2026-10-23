#pragma once

#include <stdexcept>

namespace fincore {

class input_error : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

class optimization_error : public std::runtime_error {
public:
    using std::runtime_error::runtime_error;
};

}  // namespace fincore
