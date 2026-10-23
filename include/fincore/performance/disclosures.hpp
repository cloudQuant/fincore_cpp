#pragma once

#include <string>
#include <vector>

namespace fincore {

struct disclosure_context {
    std::string convention = "TWR";
    std::string sample_period;
    std::string data_quality;
    std::string fees = "gross-of-fees";
    std::string cashflows = "none";
    std::string benchmark;
    std::string risk_free;
    bool annualized = true;
    std::vector<std::string> notes;
    std::string return_type;
    std::string units;
    std::string frequency;
};

std::string render_disclosure(const disclosure_context& context);

}  // namespace fincore
