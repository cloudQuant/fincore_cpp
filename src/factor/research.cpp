#include <fincore/factor/research.hpp>

#include <fincore/detail/json.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <cmath>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

std::vector<double> ranks(std::vector<double> values) {
    std::vector<std::size_t> order(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        order[index] = index;
    }
    std::stable_sort(order.begin(), order.end(),
                     [&values](const std::size_t left,
                               const std::size_t right) {
                         return values[left] < values[right];
                     });
    std::vector<double> result(values.size(), nan);
    std::size_t position = 0U;
    while (position < values.size()) {
        std::size_t end = position + 1U;
        while (end < values.size() &&
               values[order[end]] == values[order[position]]) {
            ++end;
        }
        const double rank =
            0.5 * (static_cast<double>(position + 1U) +
                   static_cast<double>(end));
        for (std::size_t index = position; index < end; ++index) {
            result[order[index]] = rank;
        }
        position = end;
    }
    return result;
}

double mean_of(const std::vector<double>& values) {
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isfinite(value)) {
            total += value;
            ++count;
        }
    }
    return count == 0U ? nan : total / static_cast<double>(count);
}

double std_of(const std::vector<double>& values) {
    const double mean = mean_of(values);
    if (!std::isfinite(mean)) {
        return nan;
    }
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isfinite(value)) {
            total += (value - mean) * (value - mean);
            ++count;
        }
    }
    return count < 2U ? nan
                      : std::sqrt(total / static_cast<double>(count - 1U));
}

void preprocess(factor_series& factors, const std::string& method) {
    if (method == "none") {
        return;
    }
    if (method != "cross_section_rank" &&
        method != "cross_section_zscore") {
        throw factor_data_error{
            "preprocess must be cross_section_rank, cross_section_zscore or "
            "none"};
    }
    std::map<timestamp, std::vector<std::size_t>> by_date;
    for (std::size_t row = 0; row < factors.dates.size(); ++row) {
        if (std::isfinite(factors.values[row])) {
            by_date[factors.dates[row]].push_back(row);
        }
    }
    for (const auto& entry : by_date) {
        std::vector<double> values;
        values.reserve(entry.second.size());
        for (const std::size_t row : entry.second) {
            values.push_back(factors.values[row]);
        }
        if (method == "cross_section_rank") {
            const auto ranked = ranks(values);
            for (std::size_t index = 0; index < entry.second.size(); ++index) {
                factors.values[entry.second[index]] =
                    values.size() > 1U
                        ? (ranked[index] - 1.0) /
                              static_cast<double>(values.size() - 1U)
                        : 0.0;
            }
        } else {
            const double mean = mean_of(values);
            const double deviation = std_of(values);
            for (std::size_t index = 0; index < entry.second.size(); ++index) {
                factors.values[entry.second[index]] =
                    deviation > 0.0
                        ? (values[index] - mean) / deviation
                        : 0.0;
            }
        }
    }
}

factor_series window_and_targets(const factor_series& factors,
                                 const research_spec& spec) {
    factor_series filtered;
    for (std::size_t row = 0; row < factors.dates.size(); ++row) {
        if (factors.dates[row] < spec.start ||
            factors.dates[row] > spec.end) {
            continue;
        }
        if (!spec.targets.empty()) {
            const auto found = std::find(spec.targets.begin(),
                                         spec.targets.end(),
                                         factors.assets[row]);
            if (found == spec.targets.end()) {
                continue;
            }
        }
        filtered.dates.push_back(factors.dates[row]);
        filtered.assets.push_back(factors.assets[row]);
        filtered.values.push_back(factors.values[row]);
    }
    return filtered;
}

}  // namespace

research_capabilities research_capabilities_report() {
    return {};
}

research_result evaluate_factor_research(const factor_series& factors,
                                         const dataframe& prices,
                                         const research_spec& spec) {
    if (!(spec.max_loss >= 0.0 && spec.max_loss <= 1.0)) {
        throw factor_data_error{
            "max_loss must be a number between 0 and 1"};
    }
    if (spec.quantiles < 2) {
        throw factor_data_error{"quantiles must be at least two"};
    }
    if (spec.periods.empty()) {
        throw factor_data_error{"periods must contain positive integers"};
    }
    for (const int lag : spec.stability_lags) {
        if (lag <= 0) {
            throw factor_data_error{
                "stability_lags must contain positive integers"};
        }
    }
    factor_series panel = window_and_targets(factors, spec);
    for (auto& value : panel.values) {
        value = std::isfinite(value) ? value : nan;
    }
    if (panel.dates.empty()) {
        throw factor_data_error{
            "factors contain no cells inside the requested window"};
    }
    for (std::size_t row = 0; row < panel.dates.size(); ++row) {
        const auto direction =
            spec.direction_overrides.find(panel.assets[row]);
        if (direction != spec.direction_overrides.end() &&
            std::isfinite(panel.values[row])) {
            panel.values[row] *= static_cast<double>(direction->second);
        }
    }
    preprocess(panel, spec.preprocess);

    prepare_factor_config config;
    config.periods = spec.periods;
    config.quantiles = spec.quantiles;
    config.max_loss = spec.max_loss;
    const auto prepared =
        prepare_factor_data_by_horizon(panel, prices, config);

    research_result result;
    result.manifest = {
        {"api_version", research_api_version},
        {"schema_version", std::to_string(research_result_schema_version)},
        {"snapshot_id", spec.snapshot_id},
        {"price_basis", spec.price_basis},
        {"calendar_id", spec.calendar_id},
        {"frequency", spec.frequency},
        {"preprocess", spec.preprocess},
        {"quantiles", std::to_string(spec.quantiles)},
        {"periods", std::to_string(spec.periods.size())}};
    result.spread.assign(spec.periods.size(), nan);
    result.coverage.assign(spec.periods.size(), 0U);
    if (prepared.by_horizon.empty()) {
        result.status = "UNEVALUABLE";
        result.diagnostics.push_back("no prepared horizons");
        return result;
    }

    // Combine per-horizon ICs into one date x horizon table and summaries.
    std::set<timestamp> dates_set;
    for (const auto& entry : prepared.by_horizon) {
        const auto ic = factor_information_coefficient(entry.second);
        for (const auto date : ic.dates) {
            dates_set.insert(date);
        }
    }
    result.information_coefficient.dates.assign(dates_set.begin(),
                                                dates_set.end());
    for (const auto& entry : prepared.by_horizon) {
        result.information_coefficient.labels.push_back(entry.first);
    }
    const std::size_t labels =
        result.information_coefficient.labels.size();
    const std::size_t dates =
        result.information_coefficient.dates.size();
    result.information_coefficient.values.assign(dates * labels, nan);
    result.information_coefficient_summary.labels =
        result.information_coefficient.labels;
    result.information_coefficient_summary.means.assign(labels, nan);
    result.quantile_returns.labels = result.information_coefficient.labels;
    std::set<int> quantiles;
    for (const auto& entry : prepared.by_horizon) {
        for (const auto& row : entry.second.rows) {
            quantiles.insert(row.quantile);
        }
    }
    result.quantile_returns.quantiles.assign(quantiles.begin(),
                                             quantiles.end());
    result.quantile_returns.means.assign(
        result.quantile_returns.quantiles.size() * labels, nan);
    result.quantile_returns.standard_errors.assign(
        result.quantile_returns.quantiles.size() * labels, nan);
    std::map<timestamp, std::size_t> date_position;
    for (std::size_t index = 0; index < dates; ++index) {
        date_position[result.information_coefficient.dates[index]] = index;
    }
    for (std::size_t label = 0; label < labels; ++label) {
        const auto& prepared_horizon = prepared.by_horizon[label].second;
        const auto ic = factor_information_coefficient(prepared_horizon);
        for (std::size_t index = 0; index < ic.dates.size(); ++index) {
            result.information_coefficient.values
                [date_position[ic.dates[index]] * labels + label] =
                ic.values[index];
        }
        result.coverage[label] = prepared_horizon.rows.size();
        const auto summary = mean_information_coefficient(prepared_horizon);
        if (!summary.means.empty()) {
            result.information_coefficient_summary.means[label] =
                summary.means.front();
        }
        const auto quantile_table =
            mean_return_by_quantile(prepared_horizon);
        for (std::size_t quantile = 0;
             quantile < result.quantile_returns.quantiles.size();
             ++quantile) {
            const auto found = std::find(
                quantile_table.quantiles.begin(),
                quantile_table.quantiles.end(),
                result.quantile_returns.quantiles[quantile]);
            if (found == quantile_table.quantiles.end()) {
                continue;
            }
            const std::size_t source =
                static_cast<std::size_t>(found -
                                         quantile_table.quantiles.begin());
            if (!quantile_table.means.empty()) {
                result.quantile_returns.means
                    [quantile * labels + label] =
                    quantile_table.means[source];
                result.quantile_returns.standard_errors
                    [quantile * labels + label] =
                    quantile_table.standard_errors[source];
            }
        }
        if (result.quantile_returns.quantiles.size() >= 2U &&
            !quantile_table.means.empty()) {
            const double top =
                quantile_table.means[result.quantile_returns.quantiles.size() -
                                     1U];
            const double bottom = quantile_table.means[0];
            result.spread[label] = top - bottom;
        }
        if (label == 0U) {
            for (const int lag : spec.stability_lags) {
                result.stability.push_back(
                    factor_rank_autocorrelation(prepared_horizon, lag));
            }
            for (const int quantile : result.quantile_returns.quantiles) {
                result.turnover.push_back(
                    quantile_turnover(prepared_horizon, quantile, 1));
            }
        }
    }
    result.stability_lags = spec.stability_lags;
    result.status = result.coverage.empty() ? "UNEVALUABLE" : "OK";
    for (const std::size_t count : result.coverage) {
        if (count == 0U) {
            result.status = "PARTIAL";
            result.diagnostics.push_back("a horizon has no prepared rows");
        }
    }
    return result;
}

namespace {

std::string csv_escape(const std::string& text) {
    if (text.find_first_of(",\"\n\r") == std::string::npos) {
        return text;
    }
    std::string escaped{"\""};
    for (const char character : text) {
        if (character == '"') {
            escaped.push_back('"');
        }
        escaped.push_back(character);
    }
    escaped.push_back('"');
    return escaped;
}

std::string csv_number(const double value) {
    if (!std::isfinite(value)) {
        return {};
    }
    return detail::python_float(value, "research export");
}

std::string json_escape(const std::string& text) {
    std::string escaped;
    for (const char character : text) {
        switch (character) {
            case '"': escaped += "\\\""; break;
            case '\\': escaped += "\\\\"; break;
            case '\n': escaped += "\\n"; break;
            case '\r': escaped += "\\r"; break;
            case '\t': escaped += "\\t"; break;
            default: escaped.push_back(character); break;
        }
    }
    return escaped;
}

std::string row_text(const std::vector<std::string>& cells) {
    std::string line;
    for (std::size_t index = 0; index < cells.size(); ++index) {
        if (index > 0U) {
            line.push_back(',');
        }
        line += csv_escape(cells[index]);
    }
    line.push_back('\n');
    return line;
}

void write_table(const std::filesystem::path& path,
                 const std::vector<std::string>& header,
                 const std::vector<std::vector<std::string>>& rows) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw factor_data_error{"could not create research export file"};
    }
    stream << row_text(header);
    for (const auto& row : rows) {
        stream << row_text(row);
    }
    if (!stream) {
        throw factor_data_error{"could not write research export file"};
    }
}

}  // namespace

research_export_result export_factor_research(
    const research_result& result, const std::string& directory,
    const bool charts) {
    if (charts) {
        throw factor_data_error{
            "chart export requires a raster renderer that is not available "
            "natively"};
    }
    if (directory.empty()) {
        throw factor_data_error{"research export directory must not be empty"};
    }
    const std::filesystem::path target{directory};
    if (std::filesystem::exists(target)) {
        throw factor_data_error{
            "evaluation export already exists and will not be overwritten"};
    }
    std::error_code error;
    std::filesystem::create_directories(target, error);
    if (error) {
        throw factor_data_error{"could not create research export directory"};
    }

    research_export_result exported;
    exported.directory = directory;
    const auto labels = result.information_coefficient.labels;

    std::vector<std::vector<std::string>> ic_rows;
    for (std::size_t date = 0;
         date < result.information_coefficient.dates.size(); ++date) {
        std::vector<std::string> row{
            detail::format_timestamp(result.information_coefficient.dates[date])};
        for (std::size_t label = 0; label < labels.size(); ++label) {
            row.push_back(csv_number(
                result.information_coefficient.values[date * labels.size() +
                                                      label]));
        }
        ic_rows.push_back(std::move(row));
    }
    std::vector<std::string> ic_header{"date"};
    ic_header.insert(ic_header.end(), labels.begin(), labels.end());
    write_table(target / "ic.csv", ic_header, ic_rows);
    exported.files.push_back("ic.csv");

    std::vector<std::vector<std::string>> summary_rows;
    for (std::size_t label = 0; label < labels.size(); ++label) {
        summary_rows.push_back(
            {labels[label],
             csv_number(label < result.information_coefficient_summary.means.size()
                            ? result.information_coefficient_summary.means[label]
                            : std::nan(""))});
    }
    write_table(target / "ic_summary.csv", {"label", "mean_ic"}, summary_rows);
    exported.files.push_back("ic_summary.csv");

    std::vector<std::vector<std::string>> quantile_rows;
    const std::size_t quantiles = result.quantile_returns.quantiles.size();
    for (std::size_t quantile = 0; quantile < quantiles; ++quantile) {
        for (std::size_t label = 0; label < labels.size(); ++label) {
            quantile_rows.push_back(
                {std::to_string(
                     result.quantile_returns.quantiles[quantile]),
                 labels[label],
                 csv_number(result.quantile_returns.means
                                [quantile * labels.size() + label]),
                 csv_number(result.quantile_returns.standard_errors
                                [quantile * labels.size() + label])});
        }
    }
    write_table(target / "quantile_returns.csv",
                {"quantile", "label", "mean", "standard_error"},
                quantile_rows);
    exported.files.push_back("quantile_returns.csv");

    std::vector<std::vector<std::string>> turnover_rows;
    for (std::size_t index = 0; index < result.turnover.size(); ++index) {
        const std::string quantile =
            index < quantiles
                ? std::to_string(result.quantile_returns.quantiles[index])
                : std::to_string(index + 1U);
        const auto& series = result.turnover[index];
        for (std::size_t date = 0; date < series.dates.size(); ++date) {
            turnover_rows.push_back(
                {quantile, detail::format_timestamp(series.dates[date]),
                 csv_number(series.values[date])});
        }
    }
    write_table(target / "turnover.csv", {"quantile", "date", "value"},
                turnover_rows);
    exported.files.push_back("turnover.csv");

    std::vector<std::vector<std::string>> stability_rows;
    for (std::size_t index = 0; index < result.stability.size(); ++index) {
        const std::string lag =
            index < result.stability_lags.size()
                ? std::to_string(result.stability_lags[index])
                : std::to_string(index + 1U);
        const auto& series = result.stability[index];
        for (std::size_t date = 0; date < series.dates.size(); ++date) {
            stability_rows.push_back(
                {lag, detail::format_timestamp(series.dates[date]),
                 csv_number(series.values[date])});
        }
    }
    write_table(target / "stability.csv", {"lag", "date", "value"},
                stability_rows);
    exported.files.push_back("stability.csv");

    std::vector<std::vector<std::string>> spread_rows;
    std::vector<std::vector<std::string>> coverage_rows;
    for (std::size_t label = 0; label < labels.size(); ++label) {
        spread_rows.push_back(
            {labels[label],
             csv_number(label < result.spread.size() ? result.spread[label]
                                                     : std::nan(""))});
        coverage_rows.push_back(
            {labels[label],
             label < result.coverage.size()
                 ? std::to_string(result.coverage[label])
                 : std::string{}});
    }
    write_table(target / "spreads.csv", {"label", "spread"}, spread_rows);
    exported.files.push_back("spreads.csv");
    write_table(target / "coverage.csv", {"label", "count"}, coverage_rows);
    exported.files.push_back("coverage.csv");

    std::vector<std::vector<std::string>> diagnostic_rows;
    for (std::size_t index = 0; index < result.diagnostics.size(); ++index) {
        diagnostic_rows.push_back(
            {std::to_string(index + 1U), result.diagnostics[index]});
    }
    write_table(target / "diagnostics.csv", {"index", "message"},
                diagnostic_rows);
    exported.files.push_back("diagnostics.csv");

    std::ostringstream manifest;
    manifest << "{\n  \"status\": \"" << json_escape(result.status)
             << "\",\n  \"exports\": [";
    for (std::size_t index = 0; index < exported.files.size(); ++index) {
        if (index > 0U) {
            manifest << ", ";
        }
        manifest << '\"' << json_escape(exported.files[index]) << '\"';
    }
    manifest << "]";
    for (const auto& [key, value] : result.manifest) {
        manifest << ",\n  \"" << json_escape(key) << "\": \""
                 << json_escape(value) << '\"';
    }
    manifest << "\n}\n";
    {
        std::ofstream stream(target / "manifest.json", std::ios::binary);
        if (!stream) {
            throw factor_data_error{"could not create research manifest"};
        }
        stream << manifest.str();
    }
    exported.files.push_back("manifest.json");
    return exported;
}

}  // namespace fincore
