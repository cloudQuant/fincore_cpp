#pragma once

#include <fincore/factor/research_detailed.hpp>

#include <cstddef>
#include <map>
#include <memory>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore::research_detailed_detail {

struct horizon_input {
    std::string period;
    std::vector<double> label_values;
};

struct label_input {
    std::vector<std::string> periods;
    std::vector<horizon_input> horizons;
};

struct detailed_signal_columns {
    std::span<const timestamp> dates;
    std::span<const std::string> assets;
    std::span<const std::size_t> source_rows;
    std::span<const double> source_values;
    std::span<const double> processed_values;
    std::string_view factor;
    std::string_view preprocess;
    int direction = 1;
    const std::map<std::string, std::string>* groups = nullptr;
};

class detailed_export_sink {
public:
    virtual ~detailed_export_sink() = default;

    virtual void write_signals(const detailed_signal_columns& columns) = 0;
    virtual void begin_labels() = 0;
    virtual void write_label_horizon(
        std::span<const timestamp> dates,
        std::span<const std::string> assets,
        const horizon_input& horizon,
        std::span<unsigned char> has_any_label) = 0;
    virtual void finish_labels() = 0;
    virtual research_export_result finish(
        const research_detailed_result& result) = 0;
};

struct detailed_export_evaluation {
    research_detailed_result result;
    std::size_t signal_rows = 0U;
    std::size_t label_rows = 0U;
    research_export_result exported;
};

std::unique_ptr<detailed_export_sink> make_detailed_export_sink(
    const std::string& directory);

detailed_export_evaluation evaluate_factor_research_detailed_for_export(
    std::string_view factor_name, const factor_series& factors,
    const dataframe& prices, const research_spec& spec,
    detailed_export_sink& sink);

// Validate the shared window/target selection and return original source-row
// indices in source order without materializing an intermediate panel.
std::vector<std::size_t> select_panel_source_rows(
    const factor_series& factors, const research_spec& spec);

// Label vectors follow the selected panel row order, including NaN holes.
label_input build_detailed_label_input(
    const factor_series& panel, const dataframe& prices,
    const research_spec& spec);

// Internal-only stability path for the detailed evaluator. The panel must be
// sorted by (date, asset); ranks are parallel to its rows and retain NaNs for
// missing original values. date_offsets contains one contiguous row range per
// panel date, including dates whose ranks are all NaN.
std::vector<factor_series_result> factor_rank_autocorrelation_ranked_panel(
    const factor_series& panel, std::span<const double> ranks,
    std::span<const std::size_t> date_offsets, const std::vector<int>& lags);

}  // namespace fincore::research_detailed_detail
