#pragma once

#include <stdexcept>

namespace fincore {

class input_error : public std::invalid_argument {
public:
    using std::invalid_argument::invalid_argument;
};

}  // namespace fincore
