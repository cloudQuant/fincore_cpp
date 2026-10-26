#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/detail/stable_double_order.hpp>
#include <fincore/factor/research_detailed.hpp>

#include <algorithm>
#include <array>
#include <charconv>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <initializer_list>
#include <iterator>
#include <limits>
#include <numeric>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <psapi.h>
#elif defined(__unix__) || defined(__APPLE__)
#include <sys/resource.h>
#if defined(__linux__)
#include <sched.h>
#endif
#endif

namespace {

using fincore::timestamp;
using clock_type = std::chrono::steady_clock;
constexpr std::int64_t nanoseconds_per_second = 1'000'000'000LL;
constexpr std::int64_t shanghai_offset_ns = 8LL * 3'600LL * nanoseconds_per_second;
constexpr std::size_t window = 10U;
constexpr std::size_t local_timestamp_prefix_length = 19U;
constexpr const char* factor_name = "alpha003";
constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using timestamp_prefix = std::array<char, local_timestamp_prefix_length>;

struct timestamp_prefix_hash {
    std::size_t operator()(const timestamp_prefix& prefix) const noexcept {
        std::size_t hash = 2166136261U;
        for (const unsigned char character : prefix) {
            hash ^= character;
            hash *= 16777619U;
        }
        return hash;
    }
};

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

double parse_number(const std::string_view text) {
    if (text.empty()) {
        return nan;
    }
    auto remaining = text;
    if (remaining.front() == '+') {
        remaining.remove_prefix(1U);
        if (remaining.empty()) {
            return nan;
        }
    }
    double value = 0.0;
    const auto result = std::from_chars(
        remaining.data(), remaining.data() + remaining.size(), value);
    if (result.ec != std::errc{} ||
        result.ptr != remaining.data() + remaining.size() ||
        !std::isfinite(value)) {
        return nan;
    }
    return value;
}

timestamp parse_local_timestamp(const std::string_view text) {
    const auto digit = [](const char character) {
        if (character < '0' || character > '9') {
            throw std::runtime_error("cannot parse timestamp");
        }
        return static_cast<int>(character - '0');
    };
    if (text.size() < 19U || text[4] != '-' || text[7] != '-' ||
        text[10] != ' ' || text[13] != ':' || text[16] != ':') {
        throw std::runtime_error("cannot parse timestamp");
    }
    const int year = digit(text[0]) * 1000 + digit(text[1]) * 100 +
                     digit(text[2]) * 10 + digit(text[3]);
    const int month = digit(text[5]) * 10 + digit(text[6]);
    const int day = digit(text[8]) * 10 + digit(text[9]);
    const int hour = digit(text[11]) * 10 + digit(text[12]);
    const int minute = digit(text[14]) * 10 + digit(text[15]);
    const int second = digit(text[17]) * 10 + digit(text[18]);
    return fincore::utc_timestamp(
               static_cast<unsigned>(year), static_cast<unsigned>(month),
               static_cast<unsigned>(day), static_cast<unsigned>(hour),
               static_cast<unsigned>(minute), static_cast<unsigned>(second)) -
           shanghai_offset_ns;
}

struct csv_fields {
    std::array<std::string_view, 8> values{};
    std::size_t count = 0U;
};

csv_fields split_fields(const std::string_view line) {
    csv_fields fields;
    std::size_t start = 0U;
    while (start <= line.size() && fields.count < fields.values.size()) {
        const auto separator = line.find(',', start);
        const auto end = separator == std::string_view::npos
                             ? line.size()
                             : separator;
        fields.values[fields.count++] = line.substr(start, end - start);
        if (separator == std::string_view::npos) {
            break;
        }
        start = separator + 1U;
    }
    return fields;
}

std::size_t header_index(const csv_fields& header, const std::string_view name) {
    for (std::size_t index = 0; index < header.count; ++index) {
        if (header.values[index] == name) {
            return index;
        }
    }
    throw std::runtime_error("CSV is missing column: " + std::string(name));
}

std::string format_utc(const timestamp value) {
    if (value % nanoseconds_per_second != 0) {
        throw std::runtime_error(
            "alpha003 daily timestamps must have whole-second precision");
    }
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
    result.volume.assign(cells, nan);

    const auto columns = result.columns();
    const auto load_range = [&result, &files, columns](
                                const std::size_t begin,
                                const std::size_t end) {
        std::vector<unsigned char> seen(result.rows(), 0U);
        std::unordered_map<timestamp_prefix, std::size_t,
                           timestamp_prefix_hash>
            timestamp_rows;
        timestamp_rows.reserve(result.rows());
        std::string text;
        for (std::size_t asset = begin; asset < end; ++asset) {
            std::ifstream stream(files[asset], std::ios::binary);
            if (!stream) {
                throw std::runtime_error("cannot open " +
                                         files[asset].string());
            }
            stream.seekg(0, std::ios::end);
            const auto size = stream.tellg();
            stream.seekg(0, std::ios::beg);
            text.resize(static_cast<std::size_t>(size));
            stream.read(text.data(),
                        static_cast<std::streamsize>(text.size()));
            if (!stream) {
                throw std::runtime_error("cannot read " +
                                         files[asset].string());
            }
            std::size_t position = 0U;
            const auto next_line = [&text, &position]() -> std::string_view {
                if (position >= text.size()) {
                    return {};
                }
                const auto newline = text.find('\n', position);
                auto end =
                    newline == std::string::npos ? text.size() : newline;
                if (end > position && text[end - 1U] == '\r') {
                    --end;
                }
                const std::string_view line(text.data() + position,
                                            end - position);
                position =
                    newline == std::string::npos ? text.size() : newline + 1U;
                return line;
            };
            const auto header = split_fields(next_line());
            const auto time_column = header_index(header, "trade_time");
            const auto open_column = header_index(header, "open");
            const auto close_column = header_index(header, "close");
            const auto volume_column = header_index(header, "volume");
            const auto required = std::max({time_column, open_column,
                                            close_column, volume_column});
            std::fill(seen.begin(), seen.end(), 0U);
            while (position < text.size()) {
                const auto line = next_line();
                if (line.empty()) {
                    continue;
                }
                const auto fields = split_fields(line);
                if (fields.count <= required) {
                    throw std::runtime_error("malformed CSV row: " +
                                             files[asset].string());
                }
                const auto timestamp_text = fields.values[time_column];
                timestamp_prefix prefix{};
                const bool cacheable =
                    timestamp_text.size() >= local_timestamp_prefix_length;
                auto cached = timestamp_rows.end();
                if (cacheable) {
                    std::copy_n(timestamp_text.data(), prefix.size(),
                                prefix.data());
                    cached = timestamp_rows.find(prefix);
                }
                std::size_t row = 0U;
                bool found_grid_row = false;
                if (cached != timestamp_rows.end()) {
                    row = cached->second;
                    found_grid_row = true;
                } else {
                    const auto date = parse_local_timestamp(timestamp_text);
                    const auto grid_position = std::lower_bound(
                        result.grid.begin(), result.grid.end(), date);
                    if (grid_position != result.grid.end() &&
                        *grid_position == date) {
                        row = static_cast<std::size_t>(
                            grid_position - result.grid.begin());
                        found_grid_row = true;
                        if (cacheable &&
                            timestamp_rows.size() < result.rows()) {
                            timestamp_rows.emplace(prefix, row);
                        }
                    }
                }
                if (!found_grid_row) {
                    continue;
                }
                if (seen[row] != 0U) {
                    throw std::runtime_error("duplicate trade_time: " +
                                             files[asset].string());
                }
                seen[row] = 1U;
                result.open[row * columns + asset] =
                    parse_number(fields.values[open_column]);
                result.volume[row * columns + asset] =
                    parse_number(fields.values[volume_column]);
            }
        }
    };
    load_range(0U, files.size());
    return result;
}

void average_ranks(
    const double* values, const std::vector<std::uint32_t>& members,
    std::vector<std::uint32_t>& order,
    std::vector<std::uint32_t>& sort_scratch, double* ranks) {
    order.assign(members.begin(), members.end());
    fincore::detail::stable_sort_indices_by_double_value(
        order, sort_scratch,
        [values](const std::uint32_t member) { return values[member]; });
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
}

bool unit_deviations(const std::array<double, window>& values,
                     std::array<double, window>& unit) {
    const auto anchor = values[0];
    std::array<double, window> differences{};
    double scale = 0.0;
    for (std::size_t index = 0; index < window; ++index) {
        differences[index] = values[index] - anchor;
        if (!std::isfinite(differences[index])) {
            return false;
        }
        scale = std::max(scale, std::abs(differences[index]));
    }
    if (scale == 0.0) {
        return false;
    }
    double mean = 0.0;
    for (std::size_t index = 0; index < window; ++index) {
        differences[index] /= scale;
        mean += differences[index];
    }
    mean /= static_cast<double>(window);
    double centered_scale = 0.0;
    for (std::size_t index = 0; index < window; ++index) {
        differences[index] -= mean;
        centered_scale = std::max(centered_scale, std::abs(differences[index]));
    }
    if (centered_scale == 0.0) {
        return false;
    }
    for (std::size_t index = 0; index < window; ++index) {
        unit[index] = differences[index] / centered_scale;
    }
    return true;
}

double correlation(const std::array<double, window>& left,
                   const std::array<double, window>& right) {
    std::array<double, window> left_unit{};
    std::array<double, window> right_unit{};
    if (!unit_deviations(left, left_unit) ||
        !unit_deviations(right, right_unit)) {
        return nan;
    }
    double numerator = 0.0;
    double left_squares = 0.0;
    double right_squares = 0.0;
    for (std::size_t index = 0; index < window; ++index) {
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
    std::vector<std::uint32_t> members;
    members.reserve(columns);
    std::vector<std::uint32_t> order;
    std::vector<std::uint32_t> sort_scratch;
    order.reserve(columns);

    for (std::size_t row = 0; row < rows; ++row) {
        members.clear();
        const auto* open_row = prices.open.data() + row * columns;
        const auto* volume_row = prices.volume.data() + row * columns;
        for (std::size_t column = 0; column < columns; ++column) {
            if (std::isfinite(open_row[column]) &&
                std::isfinite(volume_row[column])) {
                members.push_back(static_cast<std::uint32_t>(column));
            }
        }
        if (members.empty()) {
            continue;
        }
        average_ranks(open_row, members, order, sort_scratch,
                      rank_open.data() + row * columns);
        average_ranks(volume_row, members, order, sort_scratch,
                      rank_volume.data() + row * columns);
    }

    factor_output output;
    if (rows >= window) {
        const auto reserve =
            (rows - window + 1U) * columns;
        output.series.dates.reserve(reserve);
        output.series.assets.reserve(reserve);
        output.series.values.reserve(reserve);
    }
    std::vector<char> has_factor(columns, 0);
    for (std::size_t row = window - 1U; row < rows; ++row) {
        for (std::size_t column = 0; column < columns; ++column) {
            std::array<double, window> open_window{};
            std::array<double, window> volume_window{};
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
                open_window[offset] = open_value;
                volume_window[offset] = volume_value;
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

std::string json_string(std::string_view value);

std::string workload_json(const fincore::research_spec& spec,
                          const std::size_t grid_dates,
                          const std::size_t assets,
                          const std::size_t factor_cells) {
    std::ostringstream output;
    output << "{\n"
           << "  \"contract\": \"alpha003-html-v1\",\n"
           << "  \"factor\": " << json_string(factor_name) << ",\n"
           << "  \"periods\": [";
    for (std::size_t index = 0U; index < spec.periods.size(); ++index) {
        if (index != 0U) output << ", ";
        output << json_string(std::to_string(spec.periods[index]) + "D");
    }
    output << "],\n"
           << "  \"quantiles\": " << spec.quantiles << ",\n"
           << "  \"min_assets\": " << spec.min_assets << ",\n"
           << "  \"min_quantile_assets\": " << spec.min_quantile_assets
           << ",\n"
           << "  \"preprocess\": " << json_string(spec.preprocess) << ",\n"
           << "  \"label\": " << json_string(spec.label) << ",\n"
           << "  \"direction\": 1,\n"
           << "  \"stability_lags\": [";
    for (std::size_t index = 0U; index < spec.stability_lags.size(); ++index) {
        if (index != 0U) output << ", ";
        output << spec.stability_lags[index];
    }
    output << "],\n"
           << "  \"start\": " << json_string(format_utc(spec.start)) << ",\n"
           << "  \"research_end\": " << json_string(format_utc(spec.end))
           << ",\n"
           << "  \"grid_dates\": " << grid_dates << ",\n"
           << "  \"assets\": " << assets << ",\n"
           << "  \"factor_cells\": " << factor_cells << ",\n"
           << "  \"label_config\": {\n"
           << "    \"snapshot_id\": " << json_string(spec.snapshot_id)
           << ",\n"
           << "    \"price_basis\": " << json_string(spec.price_basis)
           << ",\n"
           << "    \"frequency\": " << json_string(spec.frequency)
           << ",\n"
           << "    \"calendar_id\": " << json_string(spec.calendar_id)
           << ",\n"
           << "    \"feed_time_kind\": \"close_time\",\n"
           << "    \"source_timezone\": \"Asia/Shanghai\"\n"
           << "  },\n"
           << "  \"serialization\": {\n"
           << "    \"encoding\": \"utf-8\",\n"
           << "    \"line_ending\": \"LF\",\n"
           << "    \"timestamp\": \"ISO8601-UTC-Z\",\n"
           << "    \"float\": \"roundtrip-binary64\",\n"
           << "    \"nan\": \"null\",\n"
           << "    \"format\": \"single-file-html\"\n"
           << "  }\n"
           << '}';
    return output.str();
}

std::string json_number(const double value) {
    if (std::isinf(value)) {
        throw std::runtime_error("report contains an infinite numeric value");
    }
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

std::string json_string(const std::string_view value) {
    constexpr char hex[] = "0123456789abcdef";
    std::string output;
    output.reserve(value.size() + 2U);
    output.push_back('"');
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output += "\\\""; break;
            case '\\': output += "\\\\"; break;
            case '\b': output += "\\b"; break;
            case '\f': output += "\\f"; break;
            case '\n': output += "\\n"; break;
            case '\r': output += "\\r"; break;
            case '\t': output += "\\t"; break;
            default:
                if (character < 0x20U) {
                    output += "\\u00";
                    output.push_back(hex[(character >> 4U) & 0x0fU]);
                    output.push_back(hex[character & 0x0fU]);
                } else {
                    output.push_back(static_cast<char>(character));
                }
        }
    }
    output.push_back('"');
    return output;
}

std::string json_optional_number(const std::optional<double> value) {
    if (!value.has_value()) {
        return "null";
    }
    return json_number(*value);
}

std::string json_optional_integer(
    const std::optional<std::uint64_t> value) {
    if (!value.has_value()) {
        return "null";
    }
    return std::to_string(*value);
}

struct process_snapshot {
    std::optional<double> cpu_seconds;
    std::optional<std::uint64_t> peak_memory_bytes;
    std::string cpu_source = "unavailable";
    std::string peak_memory_source = "unavailable";
    std::string peak_memory_status = "unsupported_platform";
};

process_snapshot current_process_snapshot() {
    process_snapshot snapshot;
#if defined(_WIN32)
    snapshot.cpu_source = "GetProcessTimes (kernel + user)";
    FILETIME creation_time{};
    FILETIME exit_time{};
    FILETIME kernel_time{};
    FILETIME user_time{};
    if (GetProcessTimes(GetCurrentProcess(), &creation_time, &exit_time,
                        &kernel_time, &user_time) != FALSE) {
        ULARGE_INTEGER kernel_ticks{};
        kernel_ticks.LowPart = kernel_time.dwLowDateTime;
        kernel_ticks.HighPart = kernel_time.dwHighDateTime;
        ULARGE_INTEGER user_ticks{};
        user_ticks.LowPart = user_time.dwLowDateTime;
        user_ticks.HighPart = user_time.dwHighDateTime;
        snapshot.cpu_seconds =
            static_cast<double>(kernel_ticks.QuadPart + user_ticks.QuadPart) /
            10'000'000.0;
    }

    snapshot.peak_memory_source = "GetProcessMemoryInfo.PeakWorkingSetSize";
    using get_process_memory_info_fn = BOOL(WINAPI*)(
        HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    auto kernel32 = GetModuleHandleW(L"kernel32.dll");
    auto query = kernel32 == nullptr ? nullptr
        : reinterpret_cast<get_process_memory_info_fn>(GetProcAddress(
              kernel32, "K32GetProcessMemoryInfo"));
    HMODULE psapi = nullptr;
    if (query == nullptr) {
        psapi = LoadLibraryW(L"Psapi.dll");
        if (psapi != nullptr) {
            query = reinterpret_cast<get_process_memory_info_fn>(
                GetProcAddress(psapi, "GetProcessMemoryInfo"));
        }
    }
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (query != nullptr &&
        query(GetCurrentProcess(), &counters, counters.cb) != FALSE) {
        snapshot.peak_memory_bytes = static_cast<std::uint64_t>(
            counters.PeakWorkingSetSize);
        snapshot.peak_memory_status = "available";
    } else {
        snapshot.peak_memory_status =
            query == nullptr ? "api_unavailable" : "query_failed";
    }
    if (psapi != nullptr) {
        FreeLibrary(psapi);
    }
#elif defined(__unix__) || defined(__APPLE__)
    snapshot.cpu_source = "getrusage(RUSAGE_SELF) (user + system)";
    snapshot.peak_memory_source = "getrusage(RUSAGE_SELF).ru_maxrss";
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) == 0) {
        const auto user_seconds =
            static_cast<double>(usage.ru_utime.tv_sec) +
            static_cast<double>(usage.ru_utime.tv_usec) / 1'000'000.0;
        const auto system_seconds =
            static_cast<double>(usage.ru_stime.tv_sec) +
            static_cast<double>(usage.ru_stime.tv_usec) / 1'000'000.0;
        const auto total_cpu_seconds = user_seconds + system_seconds;
        if (std::isfinite(total_cpu_seconds) && total_cpu_seconds >= 0.0) {
            snapshot.cpu_seconds = total_cpu_seconds;
        }
        if (usage.ru_maxrss >= 0) {
            const auto raw_peak =
                static_cast<std::uint64_t>(usage.ru_maxrss);
#if defined(__APPLE__)
            snapshot.peak_memory_bytes = raw_peak;
            snapshot.peak_memory_status = "available";
#elif defined(__linux__) || defined(__FreeBSD__)
            if (raw_peak <= std::numeric_limits<std::uint64_t>::max() / 1024U) {
                snapshot.peak_memory_bytes = raw_peak * 1024U;
                snapshot.peak_memory_status = "available";
            } else {
                snapshot.peak_memory_status = "value_overflow";
            }
#else
            snapshot.peak_memory_status = "unit_unknown_for_platform";
#endif
        } else {
            snapshot.peak_memory_status = "invalid_value";
        }
    } else {
        snapshot.peak_memory_status = "query_failed";
    }
#endif
    return snapshot;
}

std::optional<double> process_cpu_delta(const process_snapshot& start,
                                        const process_snapshot& end) {
    if (!start.cpu_seconds.has_value() || !end.cpu_seconds.has_value() ||
        *end.cpu_seconds < *start.cpu_seconds) {
        return std::nullopt;
    }
    const auto delta = *end.cpu_seconds - *start.cpu_seconds;
    if (!std::isfinite(delta)) {
        return std::nullopt;
    }
    return delta;
}

std::string affinity_observation_json() {
    std::ostringstream output;
#if defined(_WIN32)
    DWORD_PTR process_mask = 0;
    DWORD_PTR system_mask = 0;
    std::vector<unsigned> cpu_ids;
    if (GetProcessAffinityMask(GetCurrentProcess(), &process_mask,
                               &system_mask) != FALSE) {
        for (unsigned index = 0U;
             index < static_cast<unsigned>(sizeof(DWORD_PTR) * 8U); ++index) {
            const auto bit = static_cast<DWORD_PTR>(1U) << index;
            if ((process_mask & bit) != 0U) {
                cpu_ids.push_back(index);
            }
        }
        std::ostringstream mask;
        mask << "0x" << std::hex << process_mask;
        output << "{\"affinity_cpu_ids\":[";
        for (std::size_t index = 0U; index < cpu_ids.size(); ++index) {
            if (index != 0U) output << ',';
            output << cpu_ids[index];
        }
        output << "],\"affinity_mask_hex\":" << json_string(mask.str())
               << ",\"affinity_source\":"
               << json_string("GetProcessAffinityMask (current processor group)")
               << ",\"affinity_status\":\"available\"}";
        return output.str();
    }
    return "{\"affinity_cpu_ids\":null,\"affinity_mask_hex\":null,"
           "\"affinity_source\":\"GetProcessAffinityMask\","
           "\"affinity_status\":\"query_failed\"}";
#elif defined(__linux__)
    cpu_set_t cpus;
    CPU_ZERO(&cpus);
    if (sched_getaffinity(0, sizeof(cpus), &cpus) == 0) {
        output << "{\"affinity_cpu_ids\":[";
        bool first = true;
        for (int index = 0; index < CPU_SETSIZE; ++index) {
            if (CPU_ISSET(index, &cpus)) {
                if (!first) output << ',';
                output << index;
                first = false;
            }
        }
        output << "],\"affinity_mask_hex\":null,"
               "\"affinity_source\":\"sched_getaffinity\","
               "\"affinity_status\":\"available\"}";
        return output.str();
    }
    return "{\"affinity_cpu_ids\":null,\"affinity_mask_hex\":null,"
           "\"affinity_source\":\"sched_getaffinity\","
           "\"affinity_status\":\"query_failed\"}";
#else
    return "{\"affinity_cpu_ids\":null,\"affinity_mask_hex\":null,"
           "\"affinity_source\":\"unsupported\","
           "\"affinity_status\":\"api_unavailable\"}";
#endif
}

std::string thread_limit_environment_json() {
    constexpr std::array<const char*, 6U> names{
        "OMP_NUM_THREADS", "OPENBLAS_NUM_THREADS", "MKL_NUM_THREADS",
        "NUMEXPR_NUM_THREADS", "VECLIB_MAXIMUM_THREADS", "BLIS_NUM_THREADS"};
    std::ostringstream output;
    output << '{';
    for (std::size_t index = 0U; index < names.size(); ++index) {
        if (index != 0U) output << ',';
        output << json_string(names[index]) << ':';
        const auto* value = std::getenv(names[index]);
        output << (value == nullptr ? "null" : json_string(value));
    }
    output << '}';
    return output.str();
}

void write_json_row(std::ostream& output,
                    const std::initializer_list<std::string>& cells) {
    output << '[';
    bool first = true;
    for (const auto& cell : cells) {
        if (!first) {
            output << ',';
        }
        output << cell;
        first = false;
    }
    output << ']';
}

template <typename Rows, typename RowWriter>
std::string table_json(const std::initializer_list<std::string_view> columns,
                       const Rows& rows, RowWriter write_row) {
    std::ostringstream output;
    output << "{\"columns\":[";
    bool first = true;
    for (const auto column : columns) {
        if (!first) {
            output << ',';
        }
        output << json_string(column);
        first = false;
    }
    output << "],\"rows\":[";
    first = true;
    for (const auto& row : rows) {
        if (!first) {
            output << ',';
        }
        write_row(output, row);
        first = false;
    }
    output << "]}";
    return output.str();
}

std::string report_tables_json(
    const fincore::research_detailed_result& result) {
    const auto ic = table_json(
        {"time", "factor", "period", "ic", "rank_ic", "n_assets"},
        result.ic, [](std::ostream& output,
                      const fincore::research_detailed_ic_row& row) {
            write_json_row(output,
                           {json_string(format_utc(row.time)),
                            json_string(row.factor), json_string(row.period),
                            json_number(row.ic), json_number(row.rank_ic),
                            std::to_string(row.n_assets)});
        });
    const auto ic_summary = table_json(
        {"factor", "period", "mean_ic", "std_ic", "icir",
         "mean_rank_ic", "std_rank_ic", "rank_icir", "n_dates"},
        result.ic_summary,
        [](std::ostream& output,
           const fincore::research_detailed_ic_summary_row& row) {
            write_json_row(output,
                           {json_string(row.factor), json_string(row.period),
                            json_number(row.mean_ic), json_number(row.std_ic),
                            json_number(row.icir),
                            json_number(row.mean_rank_ic),
                            json_number(row.std_rank_ic),
                            json_number(row.rank_icir),
                            std::to_string(row.n_dates)});
        });
    const auto quantile_returns = table_json(
        {"time", "factor", "period", "quantile", "mean_return",
         "n_assets"},
        result.quantile_returns,
        [](std::ostream& output,
           const fincore::research_detailed_quantile_return_row& row) {
            write_json_row(output,
                           {json_string(format_utc(row.time)),
                            json_string(row.factor), json_string(row.period),
                            std::to_string(row.quantile),
                            json_number(row.mean_return),
                            std::to_string(row.n_assets)});
        });
    const auto turnover = table_json(
        {"time", "factor", "period", "quantile", "turnover"},
        result.turnover,
        [](std::ostream& output,
           const fincore::research_detailed_turnover_row& row) {
            write_json_row(output,
                           {json_string(format_utc(row.time)),
                            json_string(row.factor), json_string(row.period),
                            std::to_string(row.quantile),
                            json_number(row.turnover)});
        });
    const auto spreads = table_json(
        {"time", "factor", "period", "spread", "top_quantile",
         "bottom_quantile"},
        result.spreads,
        [](std::ostream& output,
           const fincore::research_detailed_spread_row& row) {
            write_json_row(output,
                           {json_string(format_utc(row.time)),
                            json_string(row.factor), json_string(row.period),
                            json_number(row.spread),
                            std::to_string(row.top_quantile),
                            std::to_string(row.bottom_quantile)});
        });
    const auto stability = table_json(
        {"factor", "lag", "mean_rank_autocorrelation",
         "std_rank_autocorrelation", "n_dates"},
        result.stability,
        [](std::ostream& output,
           const fincore::research_detailed_stability_row& row) {
            write_json_row(output,
                           {json_string(row.factor),
                            std::to_string(row.lag),
                            json_number(row.mean_rank_autocorrelation),
                            json_number(row.std_rank_autocorrelation),
                            std::to_string(row.n_dates)});
        });
    const auto coverage = table_json(
        {"factor", "kind", "direction", "input_cells", "finite_cells",
         "label_cells", "evaluable_cells", "status"},
        result.coverage,
        [](std::ostream& output,
           const fincore::research_detailed_coverage_row& row) {
            write_json_row(output,
                           {json_string(row.factor), json_string(row.kind),
                            std::to_string(row.direction),
                            std::to_string(row.input_cells),
                            std::to_string(row.finite_cells),
                            std::to_string(row.label_cells),
                            std::to_string(row.evaluable_cells),
                            json_string(row.status)});
        });
    const auto diagnostics = table_json(
        {"factor", "stage", "severity", "code", "message"},
        result.diagnostics,
        [](std::ostream& output,
           const fincore::research_detailed_diagnostic& row) {
            write_json_row(output,
                           {row.factor.has_value()
                                ? json_string(*row.factor)
                                : "null",
                            json_string(row.stage),
                            json_string(row.severity),
                            json_string(row.code),
                            json_string(row.message)});
        });

    std::ostringstream output;
    output << "{\"ic\":" << ic << ",\"ic_summary\":" << ic_summary
           << ",\"quantile_returns\":" << quantile_returns
           << ",\"turnover\":" << turnover << ",\"spreads\":"
           << spreads << ",\"stability\":" << stability
           << ",\"coverage\":" << coverage
           << ",\"diagnostics\":" << diagnostics << '}';
    return output.str();
}

std::string html_safe_json(const std::string_view json) {
    std::string escaped;
    escaped.reserve(json.size());
    for (std::size_t index = 0U; index < json.size(); ++index) {
        const auto character = static_cast<unsigned char>(json[index]);
        if (character == '<') {
            escaped += "\\u003c";
        } else if (character == '>') {
            escaped += "\\u003e";
        } else if (character == '&') {
            escaped += "\\u0026";
        } else if (character == 0xe2U && index + 2U < json.size() &&
                   static_cast<unsigned char>(json[index + 1U]) == 0x80U &&
                   (static_cast<unsigned char>(json[index + 2U]) == 0xa8U ||
                    static_cast<unsigned char>(json[index + 2U]) == 0xa9U)) {
            escaped += static_cast<unsigned char>(json[index + 2U]) == 0xa8U
                           ? "\\u2028"
                           : "\\u2029";
            index += 2U;
        } else {
            escaped.push_back(static_cast<char>(character));
        }
    }
    return escaped;
}

std::filesystem::path default_report_template_path() {
    return (std::filesystem::path(__FILE__).parent_path() / ".." /
            "report_template.html")
        .lexically_normal();
}

std::string read_report_template(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error("cannot open report template: " +
                                 path.string());
    }
    std::string content((std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());
    if (!stream.eof() && stream.fail()) {
        throw std::runtime_error("cannot read report template: " +
                                 path.string());
    }
    constexpr std::string_view marker = "__FINCORE_REPORT_DATA__";
    const auto position = content.find(marker);
    if (position == std::string::npos ||
        content.find(marker, position + marker.size()) != std::string::npos) {
        throw std::runtime_error(
            "report template must contain exactly one __FINCORE_REPORT_DATA__ marker");
    }
    return content;
}

void replace_report_data(std::string& html, const std::string_view payload) {
    constexpr std::string_view marker = "__FINCORE_REPORT_DATA__";
    const auto position = html.find(marker);
    if (position == std::string::npos ||
        html.find(marker, position + marker.size()) != std::string::npos) {
        throw std::runtime_error(
            "report template must contain exactly one __FINCORE_REPORT_DATA__ marker");
    }
    html.replace(position, marker.size(), html_safe_json(payload));
}

void ensure_empty_output_directory(const std::filesystem::path& path) {
    if (std::filesystem::exists(path)) {
        if (!std::filesystem::is_directory(path)) {
            throw std::runtime_error("output path is not a directory: " +
                                     path.string());
        }
        if (std::filesystem::directory_iterator(path) !=
            std::filesystem::directory_iterator{}) {
            throw std::runtime_error(
                "output directory must be empty: " + path.string());
        }
        return;
    }
    std::filesystem::create_directories(path);
}

void write_single_report(const std::filesystem::path& out_dir,
                         const std::string_view html) {
    const auto destination = out_dir / "research.html";
    const auto temporary = out_dir / ".research.html.tmp";
    if (std::filesystem::exists(destination) ||
        std::filesystem::exists(temporary)) {
        throw std::runtime_error(
            "output directory changed after its empty check: " +
            out_dir.string());
    }
    try {
        std::ofstream stream(temporary, std::ios::binary | std::ios::trunc);
        if (!stream) {
            throw std::runtime_error("cannot create report temporary file");
        }
        stream.write(html.data(), static_cast<std::streamsize>(html.size()));
        stream.flush();
        if (!stream) {
            throw std::runtime_error("cannot write report temporary file");
        }
        stream.close();
        if (!stream) {
            throw std::runtime_error("cannot close report temporary file");
        }
        if (std::filesystem::exists(destination)) {
            throw std::runtime_error(
                "research.html already exists; refusing to overwrite it");
        }
        std::filesystem::rename(temporary, destination);
    } catch (...) {
        std::error_code ignored;
        std::filesystem::remove(temporary, ignored);
        throw;
    }
}

std::string timings_json(
    const double preparation_seconds, const double load_seconds,
    const double factor_seconds, const double materialization_seconds,
    const double research_seconds, const double table_serialization_seconds,
    const std::optional<double> report_render_seconds,
    const std::optional<double> html_write_seconds,
    const std::optional<double> total_seconds,
    const std::optional<double> cpu_seconds,
    const std::optional<std::uint64_t> peak_memory_bytes,
    const std::string_view cpu_source,
    const std::string_view peak_memory_source,
    const std::string_view peak_memory_status,
    const std::optional<double> elapsed_before_report_render_seconds) {
    auto accounted_stages = preparation_seconds + load_seconds +
                            factor_seconds + materialization_seconds +
                            research_seconds + table_serialization_seconds;
    if (report_render_seconds.has_value()) {
        accounted_stages += *report_render_seconds;
    }
    if (html_write_seconds.has_value()) {
        accounted_stages += *html_write_seconds;
    }
    const auto wall_reference = total_seconds.has_value()
                                    ? total_seconds
                                    : elapsed_before_report_render_seconds;
    const auto other_seconds = wall_reference.has_value()
                                   ? std::optional<double>{
                                         *wall_reference - accounted_stages}
                                   : std::nullopt;
    std::ostringstream output;
    output << "{\"preparation_seconds\":"
           << json_number(preparation_seconds)
           << ",\"load_seconds\":" << json_number(load_seconds)
           << ",\"factor_seconds\":" << json_number(factor_seconds)
           << ",\"materialization_seconds\":"
           << json_number(materialization_seconds)
           << ",\"research_seconds\":" << json_number(research_seconds)
           << ",\"table_serialization_seconds\":"
           << json_number(table_serialization_seconds)
           << ",\"report_render_seconds\":";
    if (report_render_seconds.has_value()) {
        output << json_number(*report_render_seconds);
    } else {
        output << "null";
    }
    output << ",\"html_write_seconds\":";
    if (html_write_seconds.has_value()) {
        output << json_number(*html_write_seconds);
    } else {
        output << "null";
    }
    output << ",\"elapsed_before_report_render_seconds\":";
    if (elapsed_before_report_render_seconds.has_value()) {
        output << json_number(*elapsed_before_report_render_seconds);
    } else {
        output << "null";
    }
    output << ",\"total_seconds\":";
    if (total_seconds.has_value()) {
        output << json_number(*total_seconds);
    } else {
        output << "null";
    }
    output << ",\"other_seconds\":"
           << json_optional_number(other_seconds)
           << ",\"accounted_stage_seconds\":";
    if (wall_reference.has_value()) {
        output << json_number(*wall_reference);
    } else {
        output << "null";
    }
    output << ",\"internal_wall_seconds\":";
    if (total_seconds.has_value()) {
        output << json_number(*total_seconds);
    } else {
        output << "null";
    }
    output << ",\"cpu_seconds\":" << json_optional_number(cpu_seconds)
           << ",\"internal_process_cpu_seconds\":"
           << json_optional_number(cpu_seconds)
           << ",\"peak_memory_bytes\":"
           << json_optional_integer(peak_memory_bytes)
           << ",\"cpu_measurement_source\":" << json_string(cpu_source)
           << ",\"peak_memory_source\":"
           << json_string(peak_memory_source)
           << ",\"peak_memory_status\":"
           << json_string(peak_memory_status)
           << ",\"scope\":"
           << json_string(
                  total_seconds.has_value()
                      ? "complete through research.html close; excludes process startup and stdout"
                      : "through detailed table serialization; excludes report rendering, research.html write, process startup and stdout")
           << '}';
    return output.str();
}

std::string run_info_json(
    const std::string_view run_id, const std::size_t assets,
    const std::size_t grid_dates, const int end_offset,
    const timestamp research_end, const std::size_t factor_cells,
    const fincore::research_detailed_result& result,
    const std::string_view affinity_json,
    const std::string_view thread_limits_json,
    const std::string_view timings) {
    std::ostringstream output;
    output << "{\"run_id\":" << json_string(run_id)
           << ",\"engine\":\"fincore_cpp\","
              "\"engine_fingerprint\":{\"detailed_api_version\":"
           << json_string(fincore::research_detailed_api_version)
           << ",\"result_schema_version\":"
           << fincore::research_detailed_result_schema_version
           << ",\"report_schema_version\":\"alpha003-html-v1\"}"
           << ",\"assets\":" << assets << ",\"grid_dates\":"
           << grid_dates << ",\"end_offset\":" << end_offset
           << ",\"research_end\":" << json_string(format_utc(research_end))
           << ",\"factor_cells\":" << factor_cells
           << ",\"status\":" << json_string(result.status)
           << ",\"report_file\":\"research.html\",\"timings\":"
           << timings
           << ",\"measurement_scope\":{"
              "\"research\":\"evaluate_factor_research_detailed computes the full result in memory; no signal, label, or factor-value rows are written to the report\","
              "\"table_serialization\":\"the eight summary tables are serialized into the HTML payload\","
              "\"report_render\":\"workload, run_info, and template placeholder substitution\","
              "\"internal_wall\":\"stdout timing completes through research.html close; embedded timing excludes report rendering and the final write; process startup and stdout output are excluded\","
              "\"process_wall\":\"measured externally from process launch to exit by the parent runner and is the comparable wall metric\","
              "\"cpu\":\"stdout timing completes the process CPU delta through HTML close; embedded snapshot ends before rendering and writing; process startup is excluded\","
              "\"peak_memory\":\"process-lifetime peak resident working set at the timing snapshot; embedded snapshot precedes rendering and writing\","
              "\"affinity_and_thread_limits\":\"observed only; this runner does not apply limits\"},"
           << "\"environment\":{\"affinity\":" << affinity_json
           << ",\"thread_limit_environment\":" << thread_limits_json
           << ",\"effective_thread_pools_verified\":false}}";
    return output.str();
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto internal_started = clock_type::now();
        const auto process_started = current_process_snapshot();
        const auto affinity_json = affinity_observation_json();
        const auto thread_limits_json = thread_limit_environment_json();
        std::filesystem::path data_dir =
            std::filesystem::path("benchmarks") / "alpha003_a_stocks" /
            "data" / "a_stocks";
        std::filesystem::path out_dir =
            std::filesystem::path("benchmarks") / "alpha003_a_stocks" /
            "results" / "cpp";
        auto report_template = default_report_template_path();
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
            } else if (argument == "--report-template" &&
                       index + 1 < argc) {
                report_template = argv[++index];
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

        ensure_empty_output_directory(out_dir);
        const auto report_template_text =
            read_report_template(report_template);

        const auto load_start = clock_type::now();
        const auto preparation_seconds = std::chrono::duration<double>(
            load_start - internal_started).count();
        auto prices = load_panel(data_dir, max_symbols);
        if (std::any_of(prices.grid.begin(), prices.grid.end(),
                        [](const timestamp value) {
                            return value % nanoseconds_per_second != 0;
                        })) {
            throw std::runtime_error(
                "alpha003 daily timestamps must have whole-second precision");
        }
        const auto load_seconds = seconds_since(load_start);

        const auto factor_start = clock_type::now();
        auto factor = compute_alpha003(prices);
        const auto factor_seconds = seconds_since(factor_start);
        const auto materialization_start = clock_type::now();

        if (end_offset < 0 ||
            static_cast<std::size_t>(end_offset) + 2U > prices.grid.size()) {
            throw std::runtime_error(
                "--end-offset must leave at least two grid dates");
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
            throw std::runtime_error(
                "no asset produced a finite alpha003 value in the research window");
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

        const auto research_start = clock_type::now();
        const auto materialization_seconds =
            std::chrono::duration<double>(
                research_start - materialization_start)
                .count();
        const auto result = fincore::evaluate_factor_research_detailed(
            factor_name, factor.series, price_frame, spec);
        const auto research_seconds = seconds_since(research_start);

        const auto table_serialization_start = clock_type::now();
        const auto tables = report_tables_json(result);
        const auto table_serialization_seconds =
            seconds_since(table_serialization_start);
        const auto elapsed_before_report_render_seconds =
            seconds_since(internal_started);
        const auto before_render_snapshot = current_process_snapshot();
        const auto before_render_cpu_seconds =
            process_cpu_delta(process_started, before_render_snapshot);
        const auto before_render_timings = timings_json(
            preparation_seconds, load_seconds, factor_seconds,
            materialization_seconds, research_seconds,
            table_serialization_seconds, std::nullopt, std::nullopt,
            std::nullopt, before_render_cpu_seconds,
            before_render_snapshot.peak_memory_bytes,
            process_started.cpu_source,
            before_render_snapshot.peak_memory_source,
            before_render_snapshot.peak_memory_status,
            elapsed_before_report_render_seconds);

        const auto report_render_start = clock_type::now();
        const auto workload = workload_json(
            spec, prices.rows(), factor.target_columns.size(),
            factor.series.values.size());
        const auto embedded_run_info = run_info_json(
            run_id, factor.target_columns.size(), prices.rows(), end_offset,
            research_end, factor.series.values.size(), result, affinity_json,
            thread_limits_json, before_render_timings);
        std::ostringstream payload;
        payload << "{\"schema_version\":\"alpha003-html-v1\","
                   "\"engine\":\"fincore_cpp\",\"workload\":"
                << workload << ",\"run_info\":" << embedded_run_info
                << ",\"timings\":" << before_render_timings
                << ",\"tables\":" << tables << '}';
        auto html = report_template_text;
        replace_report_data(html, payload.str());
        const auto report_render_seconds = seconds_since(report_render_start);

        const auto html_write_start = clock_type::now();
        write_single_report(out_dir, html);
        const auto html_write_seconds = seconds_since(html_write_start);

        const auto process_finished = current_process_snapshot();
        const auto total_seconds = seconds_since(internal_started);
        const auto cpu_seconds =
            process_cpu_delta(process_started, process_finished);
        const auto final_timings = timings_json(
            preparation_seconds, load_seconds, factor_seconds,
            materialization_seconds, research_seconds,
            table_serialization_seconds, report_render_seconds,
            html_write_seconds, total_seconds, cpu_seconds,
            process_finished.peak_memory_bytes, process_started.cpu_source,
            process_finished.peak_memory_source,
            process_finished.peak_memory_status,
            elapsed_before_report_render_seconds);
        const auto final_run_info = run_info_json(
            run_id, factor.target_columns.size(), prices.rows(), end_offset,
            research_end, factor.series.values.size(), result, affinity_json,
            thread_limits_json, final_timings);
        std::printf("%s\n", final_run_info.c_str());
        return 0;
    } catch (const std::exception& error) {
        std::fprintf(stderr, "alpha003_research: %s\n", error.what());
        return 1;
    }
}
