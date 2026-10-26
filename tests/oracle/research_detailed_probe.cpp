#include <fincore/factor/research_detailed_export.hpp>
#include <fincore/factor/serialization.hpp>

#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <limits>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

using fincore::factor_value;

const factor_value& member(const factor_value& value,
                           const std::string_view key) {
    const auto* found = value.find(key);
    if (found == nullptr) {
        throw std::runtime_error{"missing JSON member: " + std::string{key}};
    }
    return *found;
}

std::string string_value(const factor_value& value) {
    if (!value.is_string()) {
        throw std::runtime_error{"expected a JSON string"};
    }
    return value.as_string();
}

std::int64_t integer_value(const factor_value& value) {
    if (value.is_integer()) {
        return value.as_integer();
    }
    throw std::runtime_error{"expected a JSON integer"};
}

// The reference fixture encodes gaps in prices and factor values as JSON null,
// so a null is a missing observation to hand the engine rather than a malformed
// input; only a value that is neither null nor numeric is an error.
double number_value(const factor_value& value) {
    if (value.is_null()) {
        return std::numeric_limits<double>::quiet_NaN();
    }
    if (value.is_number()) {
        return value.as_number();
    }
    if (value.is_integer()) {
        return static_cast<double>(value.as_integer());
    }
    throw std::runtime_error{"expected a JSON number or null"};
}

int integer_as_int(const factor_value& value) {
    const auto number = integer_value(value);
    if (number < std::numeric_limits<int>::min() ||
        number > std::numeric_limits<int>::max()) {
        throw std::runtime_error{"JSON integer is outside int range"};
    }
    return static_cast<int>(number);
}

int parse_digits(const std::string_view text, const std::size_t offset,
                 const std::size_t count) {
    int result = 0;
    for (std::size_t index = 0; index < count; ++index) {
        const char character = text.at(offset + index);
        if (character < '0' || character > '9') {
            throw std::runtime_error{"invalid UTC timestamp digits"};
        }
        result = result * 10 + (character - '0');
    }
    return result;
}

fincore::timestamp parse_utc_timestamp(const factor_value& value) {
    using namespace std::chrono;
    const std::string text = string_value(value);
    if (text.size() < 20U || text[4] != '-' || text[7] != '-' ||
        text[10] != 'T' || text[13] != ':' || text[16] != ':') {
        throw std::runtime_error{"expected an ISO-8601 UTC timestamp"};
    }
    const int year_value = parse_digits(text, 0U, 4U);
    const unsigned month_value =
        static_cast<unsigned>(parse_digits(text, 5U, 2U));
    const unsigned day_value =
        static_cast<unsigned>(parse_digits(text, 8U, 2U));
    const int hour_value = parse_digits(text, 11U, 2U);
    const int minute_value = parse_digits(text, 14U, 2U);
    const int second_value = parse_digits(text, 17U, 2U);
    std::size_t offset = 19U;
    std::int64_t fractional_nanoseconds = 0;
    if (offset < text.size() && text[offset] == '.') {
        ++offset;
        std::size_t digits = 0U;
        while (offset < text.size() && text[offset] >= '0' &&
               text[offset] <= '9') {
            if (digits == 9U) {
                throw std::runtime_error{
                    "UTC timestamp has more than nine fractional digits"};
            }
            fractional_nanoseconds = fractional_nanoseconds * 10 +
                                     (text[offset] - '0');
            ++digits;
            ++offset;
        }
        if (digits == 0U) {
            throw std::runtime_error{"UTC timestamp fraction is empty"};
        }
        while (digits < 9U) {
            fractional_nanoseconds *= 10;
            ++digits;
        }
    }
    if (offset + 1U != text.size() || text[offset] != 'Z') {
        throw std::runtime_error{"UTC timestamp must end in Z"};
    }
    const year_month_day date{year{year_value}, month{month_value},
                              day{day_value}};
    if (!date.ok() || hour_value < 0 || hour_value > 23 ||
        minute_value < 0 || minute_value > 59 || second_value < 0 ||
        second_value > 59) {
        throw std::runtime_error{"UTC timestamp contains invalid fields"};
    }
    const auto days = sys_days{date};
    const auto point = time_point_cast<nanoseconds>(days) +
                       hours{hour_value} + minutes{minute_value} +
                       seconds{second_value} +
                       nanoseconds{fractional_nanoseconds};
    return point.time_since_epoch().count();
}

std::vector<std::string> string_array(const factor_value& value) {
    if (!value.is_array()) {
        throw std::runtime_error{"expected a JSON array"};
    }
    std::vector<std::string> result;
    result.reserve(value.as_array().size());
    for (const auto& item : value.as_array()) {
        result.push_back(string_value(item));
    }
    return result;
}

std::vector<int> int_array(const factor_value& value) {
    if (!value.is_array()) {
        throw std::runtime_error{"expected a JSON integer array"};
    }
    std::vector<int> result;
    result.reserve(value.as_array().size());
    for (const auto& item : value.as_array()) {
        result.push_back(integer_as_int(item));
    }
    return result;
}

fincore::research_spec make_spec(const factor_value& value,
                                 const factor_value& direction_overrides) {
    fincore::research_spec spec;
    spec.targets = string_array(member(value, "targets"));
    spec.start = parse_utc_timestamp(member(value, "start"));
    spec.end = parse_utc_timestamp(member(value, "end"));
    spec.snapshot_id = string_value(member(value, "snapshot_id"));
    spec.price_basis = string_value(member(value, "price_basis"));
    spec.calendar_id = string_value(member(value, "calendar_id"));
    spec.frequency = string_value(member(value, "frequency"));
    spec.periods = int_array(member(value, "periods"));
    spec.label = string_value(member(value, "label"));
    spec.preprocess = string_value(member(value, "preprocess"));
    spec.quantiles = integer_as_int(member(value, "quantiles"));
    spec.min_assets = integer_as_int(member(value, "min_assets"));
    spec.min_quantile_assets =
        integer_as_int(member(value, "min_quantile_assets"));
    spec.max_loss = number_value(member(value, "max_loss"));
    spec.stability_lags = int_array(member(value, "stability_lags"));
    if (!direction_overrides.is_object()) {
        throw std::runtime_error{"direction_overrides must be a JSON object"};
    }
    for (const auto& [name, direction] : direction_overrides.as_object()) {
        spec.direction_overrides.emplace(name, integer_as_int(direction));
    }
    return spec;
}

fincore::factor_series make_factor_series(const factor_value& rows) {
    if (!rows.is_array()) {
        throw std::runtime_error{"factor_rows must be a JSON array"};
    }
    fincore::factor_series factors;
    factors.dates.reserve(rows.as_array().size());
    factors.assets.reserve(rows.as_array().size());
    factors.values.reserve(rows.as_array().size());
    for (const auto& row : rows.as_array()) {
        factors.dates.push_back(parse_utc_timestamp(member(row, "time")));
        factors.assets.push_back(string_value(member(row, "asset")));
        factors.values.push_back(number_value(member(row, "value")));
    }
    return factors;
}

fincore::dataframe make_prices(const factor_value& rows,
                               const std::vector<std::string>& assets) {
    if (!rows.is_array() || rows.as_array().empty()) {
        throw std::runtime_error{"price_rows must be a non-empty JSON array"};
    }
    std::vector<fincore::timestamp> dates;
    dates.reserve(rows.as_array().size());
    for (const auto& row : rows.as_array()) {
        dates.push_back(parse_utc_timestamp(member(row, "time")));
    }
    fincore::dataframe prices;
    prices.load_index(std::move(dates));
    for (const auto& asset : assets) {
        std::vector<double> values;
        values.reserve(rows.as_array().size());
        for (const auto& row : rows.as_array()) {
            values.push_back(number_value(
                member(member(row, "values"), asset)));
        }
        prices.load_column<double>(asset.c_str(), std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return prices;
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    if (!stream) {
        throw std::runtime_error{"could not open probe input JSON"};
    }
    return {std::istreambuf_iterator<char>{stream},
            std::istreambuf_iterator<char>{}};
}

void run_probe(const std::filesystem::path& input_path,
               const std::filesystem::path& output_root) {
    if (!std::filesystem::is_directory(output_root)) {
        throw std::runtime_error{"output root must already exist"};
    }
    const auto document = fincore::parse_json_value(read_file(input_path));
    const auto& spec_value = member(document, "spec");
    const auto assets = string_array(member(spec_value, "targets"));
    const auto& cases = member(document, "cases");
    if (!cases.is_array() || cases.as_array().empty()) {
        throw std::runtime_error{"probe input must contain cases"};
    }
    for (const auto& item : cases.as_array()) {
        const auto factor_name = string_value(member(item, "factor"));
        auto factors = make_factor_series(member(item, "factor_rows"));
        auto prices = make_prices(member(item, "price_rows"), assets);
        const auto spec = make_spec(spec_value,
                                    member(item, "direction_overrides"));
        const auto result = fincore::evaluate_factor_research_detailed(
            factor_name, factors, prices, spec);
        const auto destination = (output_root / factor_name).string();
        const auto exported = fincore::export_factor_research_detailed(
            result, destination);
        if (exported.files.size() != 11U) {
            throw std::runtime_error{
                "detailed exporter did not return ten tables plus manifest"};
        }
        std::cout << "exported " << factor_name << ' '
                  << exported.files.size() << '\n';
    }
}

}  // namespace

int main(int argc, char** argv) {
    try {
        if (argc != 3) {
            std::cerr << "usage: research_detailed_probe <input.json> "
                         "<existing-output-root>\n";
            return 2;
        }
        run_probe(argv[1], argv[2]);
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "research detailed oracle probe failed: " << error.what()
                  << '\n';
        return 1;
    }
}
