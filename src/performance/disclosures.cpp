#include <fincore/performance/disclosures.hpp>

namespace fincore {
namespace {

const std::string& or_default(const std::string& value,
                              const char* fallback) {
    static const std::string unspecified{fallback};
    return value.empty() ? unspecified : value;
}

}  // namespace

// Every line is emitted whether or not the field was supplied, with text fields
// falling back to "unspecified" and the benchmark to "none": the block is meant to
// answer each question rather than to omit the ones it cannot, so a reader never
// has to tell an absent line apart from a deliberately blank one.
std::string render_disclosure(const disclosure_context& context) {
    static const std::string none{"none"};
    std::string result;
    const auto append = [&result](const char* label, const std::string& value) {
        result += label;
        result += value;
        result += '\n';
    };
    append("Convention: ", or_default(context.convention, "unspecified"));
    append("Return type: ", or_default(context.return_type, "unspecified"));
    append("Units: ", or_default(context.units, "unspecified"));
    append("Frequency: ", or_default(context.frequency, "unspecified"));
    append("Sample period: ", or_default(context.sample_period, "unspecified"));
    append("Data quality: ", or_default(context.data_quality, "unspecified"));
    append("Fees: ", or_default(context.fees, "unspecified"));
    append("Cashflows: ", or_default(context.cashflows, "unspecified"));
    append("Benchmark: ", context.benchmark.empty() ? none : context.benchmark);
    append("Risk-free: ", or_default(context.risk_free, "unspecified"));
    append("Annualized: ", context.annualized ? "yes" : "no");
    if (!context.notes.empty()) {
        result += "Notes: ";
        for (std::size_t position = 0; position < context.notes.size();
             ++position) {
            if (position != 0) {
                result += "; ";
            }
            result += context.notes[position];
        }
    }
    return result;
}

}  // namespace fincore
