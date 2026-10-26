#include <fincore/factor/data.hpp>

#include <algorithm>
#include <array>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <iomanip>
#include <iostream>
#include <limits>
#include <numeric>
#include <optional>
#include <set>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <utility>
#include <vector>

namespace {

constexpr std::size_t kDateCount = 250U;
constexpr std::size_t kDenseAssetCount = 100U;
constexpr std::size_t kSparseAssetCount = 1000U;
constexpr std::size_t kSparseStride = 20U;
constexpr std::size_t kGroupCount = 16U;
constexpr std::size_t kWarmups = 2U;
constexpr std::size_t kRepetitions = 9U;
constexpr std::uint64_t kPayloadBudgetBytes = 128ULL * 1024ULL * 1024ULL;
constexpr std::uint64_t kProcessRssBudgetBytes = 256ULL * 1024ULL * 1024ULL;
constexpr std::int64_t kDayNanoseconds = 86'400'000'000'000LL;

struct source_record {
    std::uint16_t date_id = 0U;
    std::uint16_t asset_id = 0U;
    double factor = 0.0;
    std::int8_t quantile = 0;
    std::int16_t group_id = -1;
    std::array<double, 2> forward_returns{};
};

struct fixture {
    std::string case_name;
    std::vector<fincore::timestamp> dates;
    std::vector<std::string> assets;
    std::vector<std::string> groups;
    std::vector<std::string> forward_labels;
    std::vector<source_record> records;
    std::size_t nan_value_count = 0U;
};

struct rows_layout {
    fincore::prepared_factor_data prepared;
};

struct dense_soa_layout {
    std::vector<fincore::timestamp> dates;
    std::vector<std::string> assets;
    std::vector<std::string> groups;
    std::vector<std::string> forward_labels;
    std::vector<std::uint8_t> membership;
    std::vector<double> factor;
    std::vector<std::int8_t> quantile;
    std::vector<std::int16_t> group_id;
    std::vector<double> forward_1d;
    std::vector<double> forward_5d;
};

struct compact_long_layout {
    std::vector<fincore::timestamp> dates;
    std::vector<std::string> assets;
    std::vector<std::string> groups;
    std::vector<std::string> forward_labels;
    std::vector<std::uint16_t> date_id;
    std::vector<std::uint16_t> asset_id;
    std::vector<double> factor;
    std::vector<std::int8_t> quantile;
    std::vector<std::int16_t> group_id;
    std::vector<double> forward_1d;
    std::vector<double> forward_5d;
};

struct payload_size {
    std::uint64_t object_bytes = 0U;
    std::uint64_t vector_capacity_bytes = 0U;
    std::uint64_t external_string_capacity_bytes = 0U;

    [[nodiscard]] std::uint64_t total_bytes() const noexcept {
        return object_bytes + vector_capacity_bytes +
               external_string_capacity_bytes;
    }
};

class fnv64 {
public:
    void add_byte(const std::uint8_t value) noexcept {
        value_ ^= value;
        value_ *= 1099511628211ULL;
    }

    void add_u64(std::uint64_t value) noexcept {
        for (unsigned shift = 0U; shift < 64U; shift += 8U) {
            add_byte(static_cast<std::uint8_t>(value & 0xffU));
            value >>= 8U;
        }
    }

    void add_string(const std::string_view value) noexcept {
        add_u64(static_cast<std::uint64_t>(value.size()));
        for (const unsigned char character : value) add_byte(character);
    }

    void add_double(const double value) noexcept {
        if (std::isnan(value)) {
            add_byte(2U);
            return;
        }
        add_byte(1U);
        add_u64(std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] std::string hex() const {
        std::ostringstream output;
        output << std::hex << std::setfill('0') << std::setw(16) << value_;
        return output.str();
    }

private:
    std::uint64_t value_ = 14695981039346656037ULL;
};

[[nodiscard]] bool equal_value(const double left, const double right) noexcept {
    return (std::isnan(left) && std::isnan(right)) || left == right;
}

[[nodiscard]] bool is_present(const std::size_t date_id,
                              const std::size_t asset_id,
                              const std::string_view case_name) noexcept {
    if (case_name == "dense") return true;
    return (date_id * 7U + asset_id) % kSparseStride == 0U;
}

[[nodiscard]] std::string padded_name(const std::string_view prefix,
                                      const std::size_t index) {
    std::ostringstream output;
    output << prefix << '_' << std::setw(4) << std::setfill('0') << index
           << "_axis_value_with_owned_text_capacity";
    return output.str();
}

[[nodiscard]] fixture make_fixture(const std::string_view case_name) {
    if (case_name != "dense" && case_name != "sparse") {
        throw std::invalid_argument("unknown fixture case");
    }
    const std::size_t asset_count =
        case_name == "dense" ? kDenseAssetCount : kSparseAssetCount;
    fixture result;
    result.case_name = case_name;
    result.dates.reserve(kDateCount);
    result.assets.reserve(asset_count);
    result.groups.reserve(kGroupCount);
    result.forward_labels = {"forward_1_day", "forward_5_day"};

    constexpr fincore::timestamp start = 1'609'718'400'000'000'000LL;
    for (std::size_t date_id = 0U; date_id < kDateCount; ++date_id) {
        result.dates.push_back(
            start + static_cast<std::int64_t>(date_id) * kDayNanoseconds);
    }
    for (std::size_t asset_id = 0U; asset_id < asset_count; ++asset_id) {
        result.assets.push_back(padded_name("asset", asset_id));
    }
    for (std::size_t group_id = 0U; group_id < kGroupCount; ++group_id) {
        result.groups.push_back(padded_name("industry_group", group_id));
    }

    const std::size_t all_cells = kDateCount * asset_count;
    result.records.reserve(case_name == "dense" ? all_cells
                                                  : all_cells / kSparseStride + 1U);
    for (std::size_t date_id = 0U; date_id < kDateCount; ++date_id) {
        for (std::size_t asset_id = 0U; asset_id < asset_count; ++asset_id) {
            if (!is_present(date_id, asset_id, case_name)) continue;
            const double date = static_cast<double>(date_id + 1U);
            const double asset = static_cast<double>(asset_id + 1U);
            const double factor = std::sin(date * 0.17 + asset * 0.009) +
                                  0.03 * static_cast<double>(asset_id % 11U) +
                                  0.002 * std::cos(date * 0.05);
            const std::int16_t group_id =
                asset_id % 11U == 0U
                    ? static_cast<std::int16_t>(-1)
                    : static_cast<std::int16_t>(asset_id % kGroupCount);
            const double one_day =
                date_id == 1U && asset_id == 13U
                    ? std::numeric_limits<double>::quiet_NaN()
                    : 0.002 * std::sin(date * 0.11 + asset * 0.007) +
                          0.0004 * static_cast<double>(asset_id % 13U);
            const double five_day =
                0.01 * std::sin(date * 0.07 + asset * 0.003) -
                0.0002 * static_cast<double>(asset_id % 7U);
            result.nan_value_count += std::isnan(one_day) ? 1U : 0U;
            result.records.push_back(source_record{
                static_cast<std::uint16_t>(date_id),
                static_cast<std::uint16_t>(asset_id), factor,
                static_cast<std::int8_t>(1U + (date_id + asset_id * 3U) % 5U),
                group_id, {one_day, five_day}});
        }
    }
    return result;
}

[[nodiscard]] rows_layout make_rows(const fixture& input) {
    rows_layout result;
    result.prepared.forward_labels = input.forward_labels;
    result.prepared.rows.reserve(input.records.size());
    for (const auto& record : input.records) {
        const bool has_group = record.group_id >= 0;
        result.prepared.rows.push_back(fincore::factor_row{
            input.dates[record.date_id], input.assets[record.asset_id],
            record.factor, static_cast<int>(record.quantile), has_group,
            has_group ? input.groups[static_cast<std::size_t>(record.group_id)]
                      : std::string{},
            {record.forward_returns[0], record.forward_returns[1]}});
    }
    return result;
}

[[nodiscard]] dense_soa_layout make_dense_soa(const fixture& input) {
    dense_soa_layout result;
    result.dates = input.dates;
    result.assets = input.assets;
    result.groups = input.groups;
    result.forward_labels = input.forward_labels;
    const std::size_t cell_count = result.dates.size() * result.assets.size();
    result.membership.assign(cell_count, 0U);
    result.factor.assign(cell_count, std::numeric_limits<double>::quiet_NaN());
    result.quantile.assign(cell_count, 0);
    result.group_id.assign(cell_count, -1);
    result.forward_1d.assign(cell_count, std::numeric_limits<double>::quiet_NaN());
    result.forward_5d.assign(cell_count, std::numeric_limits<double>::quiet_NaN());
    for (const auto& record : input.records) {
        const std::size_t cell =
            static_cast<std::size_t>(record.date_id) * result.assets.size() +
            static_cast<std::size_t>(record.asset_id);
        result.membership[cell] = 1U;
        result.factor[cell] = record.factor;
        result.quantile[cell] = record.quantile;
        result.group_id[cell] = record.group_id;
        result.forward_1d[cell] = record.forward_returns[0];
        result.forward_5d[cell] = record.forward_returns[1];
    }
    return result;
}

[[nodiscard]] compact_long_layout make_compact_long(const fixture& input) {
    compact_long_layout result;
    result.dates = input.dates;
    result.assets = input.assets;
    result.groups = input.groups;
    result.forward_labels = input.forward_labels;
    const auto count = input.records.size();
    result.date_id.reserve(count);
    result.asset_id.reserve(count);
    result.factor.reserve(count);
    result.quantile.reserve(count);
    result.group_id.reserve(count);
    result.forward_1d.reserve(count);
    result.forward_5d.reserve(count);
    for (const auto& record : input.records) {
        result.date_id.push_back(record.date_id);
        result.asset_id.push_back(record.asset_id);
        result.factor.push_back(record.factor);
        result.quantile.push_back(record.quantile);
        result.group_id.push_back(record.group_id);
        result.forward_1d.push_back(record.forward_returns[0]);
        result.forward_5d.push_back(record.forward_returns[1]);
    }
    return result;
}

void add_metadata(fnv64& digest, const fixture& input) {
    digest.add_string("factor-layout-semantic-records-v1");
    digest.add_u64(input.dates.size());
    for (const auto date : input.dates) {
        digest.add_u64(static_cast<std::uint64_t>(date));
    }
    digest.add_u64(input.assets.size());
    for (const auto& asset : input.assets) digest.add_string(asset);
    digest.add_u64(input.groups.size());
    for (const auto& group : input.groups) digest.add_string(group);
    digest.add_u64(input.forward_labels.size());
    for (const auto& label : input.forward_labels) digest.add_string(label);
}

void add_record(fnv64& digest, const fixture& input,
                const source_record& record) {
    digest.add_u64(record.date_id);
    digest.add_u64(record.asset_id);
    digest.add_double(record.factor);
    digest.add_u64(static_cast<std::uint8_t>(record.quantile));
    digest.add_u64(static_cast<std::uint64_t>(record.group_id + 1));
    if (record.group_id >= 0) {
        digest.add_string(input.groups[static_cast<std::size_t>(record.group_id)]);
    } else {
        digest.add_string({});
    }
    digest.add_double(record.forward_returns[0]);
    digest.add_double(record.forward_returns[1]);
}

[[nodiscard]] std::string fixture_digest(const fixture& input) {
    fnv64 digest;
    add_metadata(digest, input);
    // The walk is over the dense date-by-asset grid, not over the record list, so a
    // layout that invents or loses an absent cell still changes the digest even
    // though that cell has no record to compare against.
    std::size_t record_index = 0U;
    for (std::size_t date_id = 0U; date_id < input.dates.size(); ++date_id) {
        for (std::size_t asset_id = 0U; asset_id < input.assets.size(); ++asset_id) {
            const bool present = record_index < input.records.size() &&
                                 input.records[record_index].date_id == date_id &&
                                 input.records[record_index].asset_id == asset_id;
            digest.add_byte(present ? 1U : 0U);
            if (present) add_record(digest, input, input.records[record_index++]);
        }
    }
    if (record_index != input.records.size()) {
        throw std::runtime_error("fixture records are not in axis order");
    }
    return digest.hex();
}

[[nodiscard]] std::uint16_t axis_id(
    const std::vector<fincore::timestamp>& dates,
    const fincore::timestamp value) {
    const auto found = std::lower_bound(dates.begin(), dates.end(), value);
    if (found == dates.end() || *found != value) {
        throw std::runtime_error("owned layout omitted a date axis value");
    }
    return static_cast<std::uint16_t>(std::distance(dates.begin(), found));
}

[[nodiscard]] std::uint16_t axis_id(const std::vector<std::string>& axis,
                                    const std::string_view value) {
    const auto found = std::lower_bound(axis.begin(), axis.end(), value);
    if (found == axis.end() || *found != value) {
        throw std::runtime_error("owned layout omitted an asset/group axis value");
    }
    return static_cast<std::uint16_t>(std::distance(axis.begin(), found));
}

[[nodiscard]] fixture snapshot_from(const rows_layout& layout) {
    fixture result;
    result.forward_labels = layout.prepared.forward_labels;
    std::set<fincore::timestamp> date_values;
    std::set<std::string> asset_values;
    std::set<std::string> group_values;
    for (const auto& row : layout.prepared.rows) {
        date_values.insert(row.date);
        asset_values.insert(row.asset);
        if (row.has_group) group_values.insert(row.group);
    }
    result.dates.assign(date_values.begin(), date_values.end());
    result.assets.assign(asset_values.begin(), asset_values.end());
    result.groups.assign(group_values.begin(), group_values.end());
    result.records.reserve(layout.prepared.rows.size());
    for (const auto& row : layout.prepared.rows) {
        if (row.forward_returns.size() != result.forward_labels.size()) {
            throw std::runtime_error("owned prepared row lost a horizon value");
        }
        const std::int16_t group_id = row.has_group
                                          ? static_cast<std::int16_t>(axis_id(
                                                result.groups, row.group))
                                          : static_cast<std::int16_t>(-1);
        result.records.push_back(source_record{
            axis_id(result.dates, row.date), axis_id(result.assets, row.asset),
            row.factor, static_cast<std::int8_t>(row.quantile), group_id,
            {row.forward_returns[0], row.forward_returns[1]}});
        result.nan_value_count += std::isnan(row.forward_returns[0]) ? 1U : 0U;
        result.nan_value_count += std::isnan(row.forward_returns[1]) ? 1U : 0U;
    }
    return result;
}

[[nodiscard]] fixture snapshot_from(const dense_soa_layout& layout) {
    fixture result;
    result.dates = layout.dates;
    result.assets = layout.assets;
    result.groups = layout.groups;
    result.forward_labels = layout.forward_labels;
    const std::size_t cells = result.dates.size() * result.assets.size();
    if (layout.membership.size() != cells || layout.factor.size() != cells ||
        layout.quantile.size() != cells || layout.group_id.size() != cells ||
        layout.forward_1d.size() != cells || layout.forward_5d.size() != cells) {
        throw std::runtime_error("owned dense columns lost their aligned shape");
    }
    for (std::size_t date_id = 0U; date_id < result.dates.size(); ++date_id) {
        for (std::size_t asset_id = 0U; asset_id < result.assets.size(); ++asset_id) {
            const std::size_t cell = date_id * result.assets.size() + asset_id;
            if (layout.membership[cell] == 0U) continue;
            result.records.push_back(source_record{
                static_cast<std::uint16_t>(date_id),
                static_cast<std::uint16_t>(asset_id), layout.factor[cell],
                layout.quantile[cell], layout.group_id[cell],
                {layout.forward_1d[cell], layout.forward_5d[cell]}});
            result.nan_value_count += std::isnan(layout.forward_1d[cell]) ? 1U : 0U;
            result.nan_value_count += std::isnan(layout.forward_5d[cell]) ? 1U : 0U;
        }
    }
    return result;
}

[[nodiscard]] fixture snapshot_from(const compact_long_layout& layout) {
    fixture result;
    result.dates = layout.dates;
    result.assets = layout.assets;
    result.groups = layout.groups;
    result.forward_labels = layout.forward_labels;
    const std::size_t rows = layout.date_id.size();
    if (layout.asset_id.size() != rows || layout.factor.size() != rows ||
        layout.quantile.size() != rows || layout.group_id.size() != rows ||
        layout.forward_1d.size() != rows || layout.forward_5d.size() != rows) {
        throw std::runtime_error("owned compact columns lost their aligned shape");
    }
    result.records.reserve(rows);
    for (std::size_t index = 0U; index < rows; ++index) {
        result.records.push_back(source_record{
            layout.date_id[index], layout.asset_id[index], layout.factor[index],
            layout.quantile[index], layout.group_id[index],
            {layout.forward_1d[index], layout.forward_5d[index]}});
        result.nan_value_count += std::isnan(layout.forward_1d[index]) ? 1U : 0U;
        result.nan_value_count += std::isnan(layout.forward_5d[index]) ? 1U : 0U;
    }
    return result;
}

template <typename Layout, typename Builder, typename Snapshotter>
[[nodiscard]] std::string verify_owned_after_source_destruction(
    const std::string_view case_name, Builder&& builder,
    Snapshotter&& snapshotter) {
    std::optional<Layout> owned;
    {
        const fixture source = make_fixture(case_name);
        owned.emplace(builder(source));
    }
    const fixture recovered = snapshotter(*owned);
    return fixture_digest(recovered);
}

[[nodiscard]] bool same_fixture_semantics(const fixture& expected,
                                          const fixture& actual) {
    if (expected.dates != actual.dates || expected.assets != actual.assets ||
        expected.groups != actual.groups ||
        expected.forward_labels != actual.forward_labels ||
        expected.records.size() != actual.records.size() ||
        expected.nan_value_count != actual.nan_value_count) {
        return false;
    }
    for (std::size_t index = 0U; index < expected.records.size(); ++index) {
        const auto& left = expected.records[index];
        const auto& right = actual.records[index];
        if (left.date_id != right.date_id || left.asset_id != right.asset_id ||
            !equal_value(left.factor, right.factor) ||
            left.quantile != right.quantile || left.group_id != right.group_id ||
            !equal_value(left.forward_returns[0], right.forward_returns[0]) ||
            !equal_value(left.forward_returns[1], right.forward_returns[1])) {
            return false;
        }
    }
    return true;
}

template <typename Layout>
[[nodiscard]] std::string validate_and_digest(const fixture& input,
                                              const Layout& layout) {
    const fixture recovered = snapshot_from(layout);
    if (!same_fixture_semantics(input, recovered)) {
        throw std::runtime_error("representation semantic records or axes differ");
    }
    return fixture_digest(recovered);
}

template <typename Value>
void add_vector_capacity(payload_size& result, const std::vector<Value>& values) {
    result.vector_capacity_bytes +=
        static_cast<std::uint64_t>(values.capacity()) * sizeof(Value);
}

void add_external_string_capacity(payload_size& result,
                                  const std::string& value) {
    // The fixtures use long identifiers. Values whose character capacity is
    // larger than an entire string object necessarily need separately owned
    // character storage on the measured standard library. Short/inline bytes
    // remain covered by their containing string object.
    if (value.capacity() + 1U > sizeof(std::string)) {
        result.external_string_capacity_bytes +=
            static_cast<std::uint64_t>(value.capacity()) + 1U;
    }
}

void add_external_string_capacities(payload_size& result,
                                    const std::vector<std::string>& values) {
    for (const auto& value : values) add_external_string_capacity(result, value);
}

[[nodiscard]] payload_size measure_payload(const rows_layout& layout) {
    payload_size result;
    result.object_bytes = sizeof(layout);
    const auto& prepared = layout.prepared;
    add_vector_capacity(result, prepared.rows);
    add_vector_capacity(result, prepared.forward_labels);
    add_external_string_capacities(result, prepared.forward_labels);
    for (const auto& row : prepared.rows) {
        add_external_string_capacity(result, row.asset);
        add_external_string_capacity(result, row.group);
        add_vector_capacity(result, row.forward_returns);
    }
    return result;
}

[[nodiscard]] payload_size measure_payload(const dense_soa_layout& layout) {
    payload_size result;
    result.object_bytes = sizeof(layout);
    add_vector_capacity(result, layout.dates);
    add_vector_capacity(result, layout.assets);
    add_vector_capacity(result, layout.groups);
    add_vector_capacity(result, layout.forward_labels);
    add_vector_capacity(result, layout.membership);
    add_vector_capacity(result, layout.factor);
    add_vector_capacity(result, layout.quantile);
    add_vector_capacity(result, layout.group_id);
    add_vector_capacity(result, layout.forward_1d);
    add_vector_capacity(result, layout.forward_5d);
    add_external_string_capacities(result, layout.assets);
    add_external_string_capacities(result, layout.groups);
    add_external_string_capacities(result, layout.forward_labels);
    return result;
}

[[nodiscard]] payload_size measure_payload(const compact_long_layout& layout) {
    payload_size result;
    result.object_bytes = sizeof(layout);
    add_vector_capacity(result, layout.dates);
    add_vector_capacity(result, layout.assets);
    add_vector_capacity(result, layout.groups);
    add_vector_capacity(result, layout.forward_labels);
    add_vector_capacity(result, layout.date_id);
    add_vector_capacity(result, layout.asset_id);
    add_vector_capacity(result, layout.factor);
    add_vector_capacity(result, layout.quantile);
    add_vector_capacity(result, layout.group_id);
    add_vector_capacity(result, layout.forward_1d);
    add_vector_capacity(result, layout.forward_5d);
    add_external_string_capacities(result, layout.assets);
    add_external_string_capacities(result, layout.groups);
    add_external_string_capacities(result, layout.forward_labels);
    return result;
}

[[nodiscard]] std::uint64_t process_peak_rss_bytes() {
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) {
        throw std::runtime_error("getrusage failed");
    }
#if defined(__APPLE__)
    return static_cast<std::uint64_t>(usage.ru_maxrss);
#else
    return static_cast<std::uint64_t>(usage.ru_maxrss) * 1024ULL;
#endif
}

[[nodiscard]] std::string json_string(const std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char character : value) {
        switch (character) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (character < 0x20U) {
                    output << "\\u" << std::hex << std::setw(4)
                           << std::setfill('0') << static_cast<unsigned>(character)
                           << std::dec;
                } else {
                    output << static_cast<char>(character);
                }
        }
    }
    output << '"';
    return output.str();
}

[[nodiscard]] std::string json_number(const double value) {
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

struct timing_result {
    std::vector<double> samples_ms;
    double median_ms = 0.0;
    double minimum_ms = 0.0;
    double maximum_ms = 0.0;
    payload_size payload;
    std::string semantic_digest;
};

struct measurement_output {
    timing_result timing;
    std::string expected_digest;
    std::string detached_digest;
    std::size_t axis_dates = 0U;
    std::size_t axis_assets = 0U;
    std::size_t axis_cells = 0U;
    std::size_t present_records = 0U;
    std::size_t nan_value_count = 0U;
    std::size_t group_count = 0U;
};

template <typename Layout, typename Builder, typename Validator,
          typename PayloadMeasurer>
[[nodiscard]] timing_result measure_layout(const fixture& input,
                                           Builder&& builder,
                                           Validator&& validator,
                                           PayloadMeasurer&& payload_measurer) {
    // The clock brackets the build alone: validation, digesting, the payload
    // measurement and the ownership snapshot all run outside it, and the payload
    // is read once from the last repetition. Any repetition whose digest differs
    // aborts the run, so the samples and the reported digest describe one result.
    timing_result result;
    std::string expected_digest;
    for (std::size_t iteration = 0U; iteration < kWarmups + kRepetitions;
         ++iteration) {
        const auto started = std::chrono::steady_clock::now();
        Layout layout = builder(input);
        const auto finished = std::chrono::steady_clock::now();
        const double elapsed =
            std::chrono::duration<double, std::milli>(finished - started).count();
        const std::string digest = validator(input, layout);
        if (expected_digest.empty()) {
            expected_digest = digest;
        } else if (digest != expected_digest) {
            throw std::runtime_error("semantic digest changed across repetitions");
        }
        if (iteration >= kWarmups) result.samples_ms.push_back(elapsed);
        if (iteration + 1U == kWarmups + kRepetitions) {
            result.payload = payload_measurer(layout);
        }
    }
    std::vector<double> sorted_samples = result.samples_ms;
    std::sort(sorted_samples.begin(), sorted_samples.end());
    result.minimum_ms = sorted_samples.front();
    result.maximum_ms = sorted_samples.back();
    result.median_ms = sorted_samples[sorted_samples.size() / 2U];
    result.semantic_digest = std::move(expected_digest);
    return result;
}

template <typename Layout, typename Builder, typename Validator,
          typename PayloadMeasurer, typename Snapshotter>
[[nodiscard]] measurement_output measure_representation(
    const std::string_view case_name, Builder&& builder,
    Validator&& validator, PayloadMeasurer&& payload_measurer,
    Snapshotter&& snapshotter) {
    measurement_output result;
    {
        const fixture input = make_fixture(case_name);
        result.axis_dates = input.dates.size();
        result.axis_assets = input.assets.size();
        result.axis_cells = input.dates.size() * input.assets.size();
        result.present_records = input.records.size();
        result.nan_value_count = input.nan_value_count;
        result.group_count = input.groups.size();
        result.expected_digest = fixture_digest(input);
        result.timing = measure_layout<Layout>(input,
                                               std::forward<Builder>(builder),
                                               std::forward<Validator>(validator),
                                               std::forward<PayloadMeasurer>(
                                                   payload_measurer));
    }
    if (result.timing.semantic_digest != result.expected_digest) {
        throw std::runtime_error("representation semantic digest differs from fixture");
    }
    result.detached_digest = verify_owned_after_source_destruction<Layout>(
        case_name, std::forward<Builder>(builder),
        std::forward<Snapshotter>(snapshotter));
    if (result.detached_digest != result.expected_digest) {
        throw std::runtime_error("layout changed after the source fixture was destroyed");
    }
    return result;
}

[[nodiscard]] std::string run_measurement(const std::string_view case_name,
                                          const std::string_view layout_name) {
    measurement_output measured;
    if (layout_name == "prepared_rows") {
        measured = measure_representation<rows_layout>(
            case_name, make_rows,
            [](const fixture& source, const rows_layout& layout) {
                return validate_and_digest(source, layout);
            },
            [](const rows_layout& layout) { return measure_payload(layout); },
            [](const rows_layout& layout) { return snapshot_from(layout); });
    } else if (layout_name == "dense_soa") {
        measured = measure_representation<dense_soa_layout>(
            case_name, make_dense_soa,
            [](const fixture& source, const dense_soa_layout& layout) {
                return validate_and_digest(source, layout);
            },
            [](const dense_soa_layout& layout) { return measure_payload(layout); },
            [](const dense_soa_layout& layout) { return snapshot_from(layout); });
    } else if (layout_name == "compact_long") {
        measured = measure_representation<compact_long_layout>(
            case_name, make_compact_long,
            [](const fixture& source, const compact_long_layout& layout) {
                return validate_and_digest(source, layout);
            },
            [](const compact_long_layout& layout) { return measure_payload(layout); },
            [](const compact_long_layout& layout) { return snapshot_from(layout); });
    } else {
        throw std::invalid_argument("unknown representation");
    }
    const std::uint64_t absent = measured.axis_cells - measured.present_records;
    // Read inside the child exec'd for this one case/layout pair, so the high-water
    // mark is attributable to this representation's fixture, build, validation and
    // snapshots rather than to the other pairs.
    const std::uint64_t rss = process_peak_rss_bytes();
    std::ostringstream output;
    output << "{\"case\":" << json_string(case_name)
           << ",\"representation\":" << json_string(layout_name)
           << ",\"axis_dates\":" << measured.axis_dates
           << ",\"axis_assets\":" << measured.axis_assets
           << ",\"axis_cells\":" << measured.axis_cells
           << ",\"present_records\":" << measured.present_records
           << ",\"absent_cells\":" << absent
           << ",\"horizons\":[\"forward_1_day\",\"forward_5_day\"]"
           << ",\"groups\":" << measured.group_count
           << ",\"present_nan_values\":" << measured.nan_value_count
           << ",\"semantic_digest\":" << json_string(measured.expected_digest)
           << ",\"detached_semantic_digest\":"
           << json_string(measured.detached_digest)
           << ",\"ownership_check\":\"passed_after_fixture_scope\""
           << ",\"construction\":{\"warmups\":" << kWarmups
           << ",\"repetitions\":" << kRepetitions
           << ",\"samples_ms\":[";
    for (std::size_t index = 0U; index < measured.timing.samples_ms.size(); ++index) {
        if (index != 0U) output << ',';
        output << json_number(measured.timing.samples_ms[index]);
    }
    output << "],\"median_ms\":" << json_number(measured.timing.median_ms)
           << ",\"min_ms\":" << json_number(measured.timing.minimum_ms)
           << ",\"max_ms\":" << json_number(measured.timing.maximum_ms) << '}'
           << ",\"payload_bytes\":{\"object_bytes\":"
           << measured.timing.payload.object_bytes
           << ",\"vector_capacity_bytes\":"
           << measured.timing.payload.vector_capacity_bytes
           << ",\"external_string_capacity_bytes\":"
           << measured.timing.payload.external_string_capacity_bytes
           << ",\"sizeof_std_string_object_bytes\":"
           << sizeof(std::string)
           << ",\"total_owned_representation_bytes\":"
           << measured.timing.payload.total_bytes()
           << ",\"allocator_headers_included\":false}"
           << ",\"whole_process_peak_rss_bytes\":" << rss
           << ",\"declared_owned_payload_budget_bytes\":"
           << kPayloadBudgetBytes
           << ",\"declared_process_peak_rss_budget_bytes\":"
           << kProcessRssBudgetBytes << '}';
    return output.str();
}

[[nodiscard]] std::string run_child(const std::string& executable,
                                    const std::string_view case_name,
                                    const std::string_view layout_name) {
    int output_pipe[2]{};
    if (pipe(output_pipe) != 0) throw std::runtime_error("pipe failed");
    const pid_t child = fork();
    if (child < 0) {
        close(output_pipe[0]);
        close(output_pipe[1]);
        throw std::runtime_error("fork failed");
    }
    if (child == 0) {
        close(output_pipe[0]);
        if (dup2(output_pipe[1], STDOUT_FILENO) < 0) _exit(126);
        close(output_pipe[1]);
        std::string case_arg(case_name);
        std::string layout_arg(layout_name);
        char measure_arg[] = "--measure";
        char* child_argv[] = {const_cast<char*>(executable.c_str()), measure_arg,
                              case_arg.data(), layout_arg.data(), nullptr};
        execv(executable.c_str(), child_argv);
        _exit(127);
    }

    close(output_pipe[1]);
    std::string output;
    std::array<char, 4096> buffer{};
    while (true) {
        const ssize_t count = read(output_pipe[0], buffer.data(), buffer.size());
        if (count == 0) break;
        if (count < 0) {
            close(output_pipe[0]);
            int status = 0;
            (void)waitpid(child, &status, 0);
            throw std::runtime_error("reading child output failed");
        }
        output.append(buffer.data(), static_cast<std::size_t>(count));
    }
    close(output_pipe[0]);
    int status = 0;
    if (waitpid(child, &status, 0) < 0 || !WIFEXITED(status) ||
        WEXITSTATUS(status) != 0) {
        throw std::runtime_error("layout measurement child failed");
    }
    if (output.empty()) throw std::runtime_error("layout child returned no result");
    return output;
}

[[nodiscard]] int run_parent(const char* executable_arg) {
    const std::string executable =
        std::filesystem::absolute(executable_arg).string();
    constexpr std::array<std::string_view, 2> cases{"dense", "sparse"};
    constexpr std::array<std::string_view, 3> layouts{
        "prepared_rows", "dense_soa", "compact_long"};
    std::cout << "{\"experiment\":\"phase77_factor_layout_comparison_v1\""
              << ",\"cases\":[\"dense_250x100\",\"sparse_250x1000_stride20\"]"
              << ",\"representations\":[\"prepared_rows\",\"dense_soa\",\"compact_long\"]"
              << ",\"records\":[\n";
    bool first = true;
    for (const auto case_name : cases) {
        for (const auto layout_name : layouts) {
            if (!first) std::cout << ",\n";
            std::cout << run_child(executable, case_name, layout_name);
            first = false;
        }
    }
    std::cout << "\n]}\n";
    return 0;
}

}  // namespace

int main(const int argc, char** argv) {
    try {
        if (argc == 4 && std::string_view(argv[1]) == "--measure") {
            std::cout << run_measurement(argv[2], argv[3]) << '\n';
            return 0;
        }
        if (argc != 1) {
            std::cerr << "usage: factor_layout_comparison [--measure CASE LAYOUT]\n";
            return 2;
        }
        return run_parent(argv[0]);
    } catch (const std::exception& error) {
        std::cerr << "factor layout comparison: " << error.what() << '\n';
        return 1;
    }
}
