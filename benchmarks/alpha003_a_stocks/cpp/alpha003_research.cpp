#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/factor/research.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <limits>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace {

using fincore::timestamp;
using clock_type = std::chrono::steady_clock;
constexpr std::int64_t nanoseconds_per_second = 1'000'000'000LL;
constexpr std::int64_t shanghai_offset_ns = 8LL * 3'600LL * nanoseconds_per_second;
constexpr std::size_t window = 10U;
constexpr const char* factor_name = "alpha003";
constexpr double nan = std::numeric_limits<double>::quiet_NaN();

double seconds_since(const clock_type::time_point start) {
    return std::chrono::duration<double>(clock_type::now() - start).count();
}

std::vector<std::string> split_csv(const std::string& line) {
    std::vector<std::string> fields;
    std::string field;
    for (const char character : line) {
        if (character == ',') {
            fields.push_back(field);
            field.clear();
        } else {
            field.push_back(character);
        }
    }
    fields.push_back(field);
    return fields;
}

void trim_carriage_return(std::string& value) {
    if (!value.empty() && value.back() == '\r') {
        value.pop_back();
    }
}

double parse_number(const std::string& text) {
    if (text.empty()) {
        return nan;
    }
    try {
        std::size_t consumed = 0U;
        const double value = std::stod(text, &consumed);
        if (consumed != text.size() || !std::isfinite(value)) {
            return nan;
        }
        return value;
    } catch (const std::exception&) {
        return nan;
    }
}

timestamp parse_local_timestamp(const std::string& text) {
    int year = 0;
    int month = 0;
    int day = 0;
    int hour = 0;
    int minute = 0;
    int second = 0;
    if (std::sscanf(text.c_str(), "%d-%d-%d %d:%d:%d", &year, &month, &day,
                    &hour, &minute, &second) != 6) {
        throw std::runtime_error("cannot parse timestamp: " + text);
    }
    return fincore::utc_timestamp(
               static_cast<unsigned>(year), static_cast<unsigned>(month),
               static_cast<unsigned>(day), static_cast<unsigned>(hour),
               static_cast<unsigned>(minute), static_cast<unsigned>(second)) -
           shanghai_offset_ns;
}

std::string format_utc(const timestamp value) {
    const auto seconds = std::chrono::seconds{
        value / nanoseconds_per_second};
    const auto point = std::chrono::sys_time<std::chrono::seconds>{seconds};
    const auto day = std::chrono::floor<std::chrono::days>(point);
    const std::chrono::year_month_day date{day};
    const std::chrono::hh_mm_ss time{point - day};
    std::ostringstream output;
    output << std::setfill('0') << std::setw(4)
           << static_cast<int>(date.year()) << '-' << std::setw(2)
           << static_cast<unsigned>(date.month()) << '-' << std::setw(2)
           << static_cast<unsigned>(date.day()) << 'T' << std::setw(2)
           << time.hours().count() << ':' << std::setw(2)
           << time.minutes().count() << ':' << std::setw(2)
           << time.seconds().count() << 'Z';
    return output.str();
}

std::size_t column_index(const std::vector<std::string>& header,
                         const std::string& name) {
    const auto found = std::find(header.begin(), header.end(), name);
    if (found == header.end()) {
        throw std::runtime_error("CSV is missing column: " + name);
    }
    return static_cast<std::size_t>(found - header.begin());
}

std::vector<timestamp> load_grid(const std::filesystem::path& data_dir) {
    std::ifstream stream(data_dir / "_metadata" / "observed_calendar.csv");
    if (!stream) {
        throw std::runtime_error("cannot open observed_calendar.csv");
    }
    std::string line;
    if (!std::getline(stream, line)) {
        throw std::runtime_error("observed_calendar.csv is empty");
    }
    trim_carriage_return(line);
    const auto header = split_csv(line);
    const auto close_column = column_index(header, "last_trade_time");
    std::vector<timestamp> grid;
    while (std::getline(stream, line)) {
        trim_carriage_return(line);
        if (line.empty()) {
            continue;
        }
        const auto fields = split_csv(line);
        if (fields.size() <= close_column) {
            throw std::runtime_error("malformed observed_calendar.csv row");
        }
        grid.push_back(parse_local_timestamp(fields[close_column]));
    }
    if (grid.size() < 2U) {
        throw std::runtime_error("observed calendar must contain at least two rows");
    }
    if (!std::is_sorted(grid.begin(), grid.end()) ||
        std::adjacent_find(grid.begin(), grid.end()) != grid.end()) {
        throw std::runtime_error("observed calendar must be sorted and unique");
    }
    return grid;
}

std::vector<std::filesystem::path> symbol_files(
    const std::filesystem::path& data_dir, const int limit) {
    std::vector<std::filesystem::path> files;
    for (const auto& entry : std::filesystem::directory_iterator(data_dir)) {
        if (entry.is_regular_file() && entry.path().extension() == ".csv") {
            files.push_back(entry.path());
        }
    }
    std::sort(files.begin(), files.end());
    if (files.empty()) {
        throw std::runtime_error("no stock CSVs under data directory");
    }
    if (limit < 0) {
        throw std::runtime_error("--max-symbols must be non-negative");
    }
    if (limit > 0 && static_cast<std::size_t>(limit) < files.size()) {
        std::vector<std::filesystem::path> sampled;
        sampled.reserve(static_cast<std::size_t>(limit));
        const auto count = files.size();
        for (std::size_t index = 0; index < static_cast<std::size_t>(limit);
             ++index) {
            sampled.push_back(files[index * count /
                                    static_cast<std::size_t>(limit)]);
        }
        files = std::move(sampled);
    }
    return files;
}

struct panel {
    std::vector<timestamp> grid;
    std::vector<std::string> assets;
    std::vector<double> open;
    std::vector<double> close;
    std::vector<double> volume;

    std::size_t rows() const noexcept { return grid.size(); }
    std::size_t columns() const noexcept { return assets.size(); }
    double& at(std::vector<double>& values, const std::size_t row,
               const std::size_t column) {
        return values[row * columns() + column];
    }
    double at(const std::vector<double>& values, const std::size_t row,
              const std::size_t column) const {
        return values[row * columns() + column];
    }
};

panel load_panel(const std::filesystem::path& data_dir, const int limit) {
    panel result;
    result.grid = load_grid(data_dir);
    const auto files = symbol_files(data_dir, limit);
    result.assets.reserve(files.size());
    for (const auto& file : files) {
        result.assets.push_back(file.stem().string());
    }
    const auto cells = result.rows() * result.columns();
    result.open.assign(cells, nan);
    result.close.assign(cells, nan);
    result.volume.assign(cells, nan);

    std::unordered_map<timestamp, std::size_t> row_for_date;
    row_for_date.reserve(result.grid.size());
    for (std::size_t row = 0; row < result.grid.size(); ++row) {
        row_for_date.emplace(result.grid[row], row);
    }

    for (std::size_t asset = 0; asset < files.size(); ++asset) {
        std::ifstream stream(files[asset]);
        if (!stream) {
            throw std::runtime_error("cannot open " + files[asset].string());
        }
        std::string line;
        if (!std::getline(stream, line)) {
            throw std::runtime_error("empty CSV: " + files[asset].string());
        }
        trim_carriage_return(line);
        const auto header = split_csv(line);
        const auto time_column = column_index(header, "trade_time");
        const auto open_column = column_index(header, "open");
        const auto close_column = column_index(header, "close");
        const auto volume_column = column_index(header, "volume");
        std::unordered_set<timestamp> seen;
        while (std::getline(stream, line)) {
            trim_carriage_return(line);
            if (line.empty()) {
                continue;
            }
            const auto fields = split_csv(line);
            if (fields.size() <= std::max({time_column, open_column,
                                           close_column, volume_column})) {
                throw std::runtime_error("malformed CSV row: " +
                                         files[asset].string());
            }
            const auto date = parse_local_timestamp(fields[time_column]);
            if (!seen.insert(date).second) {
                throw std::runtime_error("duplicate trade_time: " +
                                         files[asset].string());
            }
            const auto found = row_for_date.find(date);
            if (found == row_for_date.end()) {
                continue;
            }
            const auto row = found->second;
            result.at(result.open, row, asset) =
                parse_number(fields[open_column]);
            result.at(result.close, row, asset) =
                parse_number(fields[close_column]);
            result.at(result.volume, row, asset) =
                parse_number(fields[volume_column]);
        }
    }
    return result;
}

std::vector<double> average_ranks(const std::vector<double>& values,
                                  const std::vector<std::size_t>& members) {
    std::vector<std::size_t> order = members;
    std::sort(order.begin(), order.end(),
              [&values](const std::size_t left, const std::size_t right) {
                  return values[left] < values[right];
              });
    std::vector<double> ranks(values.size(), nan);
    const auto count = static_cast<double>(order.size());
    std::size_t start = 0U;
    while (start < order.size()) {
        std::size_t end = start + 1U;
        while (end < order.size() &&
               values[order[end]] == values[order[start]]) {
            ++end;
        }
        const auto rank =
            (static_cast<double>(start + 1U) + static_cast<double>(end)) /
            2.0 / count;
        for (std::size_t position = start; position < end; ++position) {
            ranks[order[position]] = rank;
        }
        start = end;
    }
    return ranks;
}

bool unit_deviations(const std::vector<double>& values,
                      std::vector<double>& unit) {
    const auto anchor = values[0];
    std::vector<double> differences(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        differences[index] = values[index] - anchor;
        if (!std::isfinite(differences[index])) {
            return false;
        }
    }
    const auto difference_scale = *std::max_element(
        differences.begin(), differences.end(),
        [](const double left, const double right) {
            return std::abs(left) < std::abs(right);
        });
    if (difference_scale == 0.0) {
        return false;
    }
    const auto scale = std::abs(difference_scale);
    std::vector<double> normalized(values.size());
    double mean = 0.0;
    for (std::size_t index = 0; index < values.size(); ++index) {
        normalized[index] = differences[index] / scale;
        mean += normalized[index];
    }
    mean /= static_cast<double>(normalized.size());
    double centered_scale = 0.0;
    for (auto& value : normalized) {
        value -= mean;
        centered_scale = std::max(centered_scale, std::abs(value));
    }
    if (centered_scale == 0.0) {
        return false;
    }
    unit.resize(values.size());
    for (std::size_t index = 0; index < values.size(); ++index) {
        unit[index] = normalized[index] / centered_scale;
    }
    return true;
}

double correlation(const std::vector<double>& left,
                   const std::vector<double>& right) {
    std::vector<double> left_unit;
    std::vector<double> right_unit;
    if (!unit_deviations(left, left_unit) ||
        !unit_deviations(right, right_unit)) {
        return nan;
    }
    double numerator = 0.0;
    double left_squares = 0.0;
    double right_squares = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        numerator += left_unit[index] * right_unit[index];
        left_squares += left_unit[index] * left_unit[index];
        right_squares += right_unit[index] * right_unit[index];
    }
    const auto denominator =
        std::sqrt(left_squares) * std::sqrt(right_squares);
    if (denominator == 0.0) {
        return nan;
    }
    auto value = numerator / denominator;
    if (!std::isfinite(value)) {
        return nan;
    }
    constexpr double epsilon = 4.0 * 2.220446049250313e-16;
    if (value > 1.0 && value <= 1.0 + epsilon) {
        value = 1.0;
    } else if (value < -1.0 && value >= -1.0 - epsilon) {
        value = -1.0;
    }
    if (value > 1.0 || value < -1.0) {
        return nan;
    }
    return value;
}


struct factor_output {
    fincore::factor_series series;
    std::vector<std::size_t> target_columns;
};

factor_output compute_alpha003(const panel& prices) {
    const auto rows = prices.rows();
    const auto columns = prices.columns();
    std::vector<double> rank_open(rows * columns, nan);
    std::vector<double> rank_volume(rows * columns, nan);
    std::vector<std::size_t> members;
    members.reserve(columns);

    for (std::size_t row = 0; row < rows; ++row) {
        members.clear();
        for (std::size_t column = 0; column < columns; ++column) {
            if (std::isfinite(prices.at(prices.open, row, column)) &&
                std::isfinite(prices.at(prices.volume, row, column))) {
                members.push_back(column);
            }
        }
        if (members.empty()) {
            continue;
        }
        std::vector<double> open_values(columns, nan);
        std::vector<double> volume_values(columns, nan);
        for (const auto column : members) {
            open_values[column] = prices.at(prices.open, row, column);
            volume_values[column] = prices.at(prices.volume, row, column);
        }
        const auto open_ranks = average_ranks(open_values, members);
        const auto volume_ranks = average_ranks(volume_values, members);
        for (const auto column : members) {
            rank_open[row * columns + column] = open_ranks[column];
            rank_volume[row * columns + column] = volume_ranks[column];
        }
    }

    factor_output output;
    std::vector<char> has_factor(columns, 0);
    for (std::size_t row = window - 1U; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            std::vector<double> open_window;
            std::vector<double> volume_window;
            open_window.reserve(window);
            volume_window.reserve(window);
            bool complete = true;
            for (std::size_t offset = 0; offset < window; ++offset) {
                const auto source_row = row + offset - (window - 1U);
                const auto open_value =
                    rank_open[source_row * columns + column];
                const auto volume_value =
                    rank_volume[source_row * columns + column];
                if (!std::isfinite(open_value) ||
                    !std::isfinite(volume_value)) {
                    complete = false;
                    break;
                }
                open_window.push_back(open_value);
                volume_window.push_back(volume_value);
            }
            if (!complete) {
                continue;
            }
            const auto value = correlation(open_window, volume_window);
            if (!std::isfinite(value)) {
                continue;
            }
            output.series.dates.push_back(prices.grid[row]);
            output.series.assets.push_back(prices.assets[column]);
            output.series.values.push_back(-value);
            has_factor[column] = 1;
        }
    }
    for (std::size_t column = 0; column < columns; ++column) {
        if (has_factor[column]) {
            output.target_columns.push_back(column);
        }
    }
    return output;
}

void write_factor_values(const std::filesystem::path& path,
                         const factor_output& factor,
                         const panel& prices) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot write " + path.string());
    }
    stream << "date,asset," << factor_name << '\n';
    stream << std::setprecision(17);
    for (std::size_t index = 0; index < factor.series.values.size(); ++index) {
        stream << format_utc(factor.series.dates[index]) << ','
               << factor.series.assets[index] << ','
               << factor.series.values[index] << '\n';
    }
}

void write_json(const std::filesystem::path& path, const std::string& text) {
    std::ofstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot write " + path.string());
    }
    stream << text;
}

std::string json_number(const double value) {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        std::filesystem::path data_dir =
            std::filesystem::path("benchmarks") / "alpha003_a_stocks" /
            "data" / "a_stocks";
        std::filesystem::path out_dir =
            std::filesystem::path("benchmarks") / "alpha003_a_stocks" /
            "results" / "cpp";
        int max_symbols = 500;
        int end_offset = 22;
        std::string preprocess = "cross_section_rank";
        std::string run_id = "run1";
        for (int index = 1; index < argc; ++index) {
            const std::string argument = argv[index];
            if (argument == "--data-dir" && index + 1 < argc) {
                data_dir = argv[++index];
            } else if (argument == "--out" && index + 1 < argc) {
                out_dir = argv[++index];
            } else if (argument == "--max-symbols" && index + 1 < argc) {
                max_symbols = std::stoi(argv[++index]);
            } else if (argument == "--end-offset" && index + 1 < argc) {
                end_offset = std::stoi(argv[++index]);
            } else if (argument == "--preprocess" && index + 1 < argc) {
                preprocess = argv[++index];
            } else if (argument == "--run-id" && index + 1 < argc) {
                run_id = argv[++index];
            } else {
                std::fprintf(stderr, "unknown argument: %s\n",
                             argument.c_str());
                return 2;
            }
        }

        const auto started = clock_type::now();
        auto load_start = clock_type::now();
        auto prices = load_panel(data_dir, max_symbols);
        const auto load_seconds = seconds_since(load_start);

        auto factor_start = clock_type::now();
        auto factor = compute_alpha003(prices);
        const auto factor_seconds = seconds_since(factor_start);

        if (end_offset < 0 ||
            static_cast<std::size_t>(end_offset) + 2U > prices.grid.size()) {
            throw std::runtime_error("--end-offset must leave at least two grid dates");
        }
        const auto research_end = prices.grid[
            prices.grid.size() - 1U - static_cast<std::size_t>(end_offset)];
        std::unordered_set<std::string> window_assets;
        for (std::size_t index = 0; index < factor.series.values.size();
             ++index) {
            if (factor.series.dates[index] >= prices.grid.front() &&
                factor.series.dates[index] <= research_end) {
                window_assets.insert(factor.series.assets[index]);
            }
        }
        std::vector<std::size_t> window_columns;
        for (const auto column : factor.target_columns) {
            if (window_assets.contains(prices.assets[column])) {
                window_columns.push_back(column);
            }
        }
        factor.target_columns = std::move(window_columns);
        if (factor.target_columns.empty()) {
            throw std::runtime_error("no asset produced a finite alpha003 value in the research window");
        }

        fincore::dataframe price_frame;
        price_frame.load_index(std::vector<timestamp>(prices.grid));
        for (const auto column : factor.target_columns) {
            std::vector<double> values(prices.rows());
            for (std::size_t row = 0; row < prices.rows(); ++row) {
                values[row] = prices.at(prices.open, row, column);
            }
            price_frame.load_column<double>(
                prices.assets[column].c_str(), std::move(values),
                hmdf::nan_policy::dont_pad_with_nans);
        }

        fincore::research_spec spec;
        for (const auto column : factor.target_columns) {
            spec.targets.push_back(prices.assets[column]);
        }
        spec.start = prices.grid.front();
        spec.end = research_end;
        spec.periods = {1, 5, 20};
        spec.quantiles = 5;
        spec.min_assets = 10;
        spec.min_quantile_assets = 1;
        spec.label = "next_open_to_open";
        spec.preprocess = preprocess;
        spec.max_loss = 1.0;
        spec.stability_lags = {1, 5, 20};
        spec.snapshot_id = "a_stocks_2025";
        spec.price_basis = "unadjusted_open";
        spec.calendar_id = "observed_calendar";
        spec.frequency = "1D";

        auto research_start = clock_type::now();
        const auto result = fincore::evaluate_factor_research(
            factor.series, price_frame, spec);
        const auto research_seconds = seconds_since(research_start);

        std::filesystem::create_directories(out_dir);
        const auto tables_dir = out_dir / "tables";
        std::error_code error;
        std::filesystem::remove_all(tables_dir, error);
        auto export_start = clock_type::now();
        const auto exported = fincore::export_factor_research(
            result, tables_dir.string(), false);
        write_factor_values(out_dir / "factor_values.csv", factor, prices);
        const auto export_seconds = seconds_since(export_start);

        const auto total_seconds = seconds_since(started);
        std::ostringstream timings;
        timings << "{\n"
                << "  \"load_seconds\": " << json_number(load_seconds)
                << ",\n  \"factor_seconds\": " << json_number(factor_seconds)
                << ",\n  \"research_seconds\": " << json_number(research_seconds)
                << ",\n  \"export_seconds\": " << json_number(export_seconds)
                << ",\n  \"total_seconds\": " << json_number(total_seconds)
                << "\n}\n";
        write_json(out_dir / "timings.json", timings.str());

        std::ostringstream info;
        info << "{\n  \"run_id\": \"" << run_id
             << "\",\n  \"engine\": \"cpp-fincore\",\n  \"assets\": "
             << factor.target_columns.size() << ",\n  \"grid_dates\": "
             << prices.rows() << ",\n  \"end_offset\": " << end_offset
             << ",\n  \"research_end\": \""
             << format_utc(prices.grid[prices.grid.size() - 1U -
                                       static_cast<std::size_t>(end_offset)])
             << "\",\n  \"factor_cells\": "
             << factor.series.values.size() << ",\n  \"status\": \""
             << result.status << "\",\n  \"exported_files\": "
             << exported.files.size() << ",\n  \"timings\": "
             << timings.str() << "}\n";
        write_json(out_dir / "run_info.json", info.str());
        std::printf("%s", info.str().c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "alpha003_research: %s\n", error.what());
        return 1;
    }
}
