#include <fincore/analysis_context.hpp>
#include <fincore/calendar.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/rolling.hpp>
#include <fincore/rolling_engine.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/xlsx.hpp>
#include <fincore/factor/report.hpp>

#include <algorithm>
#include <array>
#include <atomic>
#include <bit>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <map>
#include <numeric>
#include <new>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <malloc.h>
#include <psapi.h>
#else
#include <sys/resource.h>
#endif

#if __has_include(<fincore/detail/factor_compute.hpp>)
#include <fincore/detail/factor_compute.hpp>
#define FINCORE_FACTOR_BENCHMARK_HAS_KERNEL_DIAGNOSTICS 1
#else
#define FINCORE_FACTOR_BENCHMARK_HAS_KERNEL_DIAGNOSTICS 0
#endif

namespace benchmark_allocation_counter {

struct snapshot {
    std::uint64_t calls = 0U;
    std::uint64_t requested_bytes = 0U;
};

constinit std::atomic<bool> enabled{false};
constinit std::atomic<std::uint64_t> calls{0U};
constinit std::atomic<std::uint64_t> requested_bytes{0U};

void record(const std::size_t size) noexcept {
    if (!enabled.load(std::memory_order_relaxed)) return;
    calls.fetch_add(1U, std::memory_order_relaxed);
    requested_bytes.fetch_add(static_cast<std::uint64_t>(size),
                              std::memory_order_relaxed);
}

[[nodiscard]] void* allocate(const std::size_t size,
                              const std::size_t alignment,
                              const bool aligned_allocation) {
    const std::size_t actual_size = size == 0U ? 1U : size;
    for (;;) {
        void* result = nullptr;
#if defined(_WIN32)
        if (aligned_allocation) {
            result = _aligned_malloc(actual_size, alignment);
        } else {
            result = std::malloc(actual_size);
        }
#else
        if (!aligned_allocation) {
            result = std::malloc(actual_size);
        } else {
            const auto remainder = actual_size % alignment;
            const auto padding = remainder == 0U ? 0U : alignment - remainder;
            if (actual_size <= std::numeric_limits<std::size_t>::max() - padding) {
                result = std::aligned_alloc(alignment, actual_size + padding);
            }
        }
#endif
        if (result != nullptr) {
            record(size);
            return result;
        }
        const auto handler = std::get_new_handler();
        if (handler == nullptr) throw std::bad_alloc{};
        handler();
    }
}

class scope {
public:
    scope() noexcept {
        enabled.store(false, std::memory_order_relaxed);
        calls.store(0U, std::memory_order_relaxed);
        requested_bytes.store(0U, std::memory_order_relaxed);
        enabled.store(true, std::memory_order_relaxed);
    }

    scope(const scope&) = delete;
    scope& operator=(const scope&) = delete;

    ~scope() { enabled.store(false, std::memory_order_relaxed); }

    [[nodiscard]] snapshot finish() noexcept {
        enabled.store(false, std::memory_order_relaxed);
        return {calls.load(std::memory_order_relaxed),
                requested_bytes.load(std::memory_order_relaxed)};
    }
};

}  // namespace benchmark_allocation_counter

void* operator new(const std::size_t size) {
    return benchmark_allocation_counter::allocate(
        size, alignof(std::max_align_t), false);
}

void* operator new[](const std::size_t size) {
    return benchmark_allocation_counter::allocate(
        size, alignof(std::max_align_t), false);
}

void* operator new(const std::size_t size, const std::align_val_t alignment) {
    return benchmark_allocation_counter::allocate(
        size, static_cast<std::size_t>(alignment), true);
}

void* operator new[](const std::size_t size, const std::align_val_t alignment) {
    return benchmark_allocation_counter::allocate(
        size, static_cast<std::size_t>(alignment), true);
}

void operator delete(void* pointer) noexcept { std::free(pointer); }
void operator delete[](void* pointer) noexcept { std::free(pointer); }
void operator delete(void* pointer, const std::size_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::size_t) noexcept { std::free(pointer); }
#if defined(_WIN32)
void operator delete(void* pointer, const std::align_val_t) noexcept { _aligned_free(pointer); }
void operator delete[](void* pointer, const std::align_val_t) noexcept { _aligned_free(pointer); }
#else
void operator delete(void* pointer, const std::align_val_t) noexcept { std::free(pointer); }
void operator delete[](void* pointer, const std::align_val_t) noexcept { std::free(pointer); }
#endif
void operator delete(void* pointer, const std::size_t,
                     const std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}
void operator delete[](void* pointer, const std::size_t,
                       const std::align_val_t) noexcept {
#if defined(_WIN32)
    _aligned_free(pointer);
#else
    std::free(pointer);
#endif
}

namespace {

constexpr std::int64_t day_ns = 86'400'000'000'000LL;
constexpr std::string_view source_schema = "factor-benchmark-v5";
bool allocation_profile_enabled = false;

class fnv64 {
public:
    void add_byte(const std::uint8_t value) noexcept {
        value_ ^= value;
        value_ *= 1099511628211ULL;
    }

    void add_bytes(const std::string_view value) noexcept {
        for (const unsigned char byte : value) {
            add_byte(byte);
        }
    }

    void add_u64(std::uint64_t value) noexcept {
        for (unsigned int shift = 0; shift < 64U; shift += 8U) {
            add_byte(static_cast<std::uint8_t>(value >> shift));
        }
    }

    void add_i64(const std::int64_t value) noexcept {
        add_u64(std::bit_cast<std::uint64_t>(value));
    }

    void add_i32(const std::int32_t value) noexcept {
        const auto bits = std::bit_cast<std::uint32_t>(value);
        for (unsigned int shift = 0; shift < 32U; shift += 8U) {
            add_byte(static_cast<std::uint8_t>(bits >> shift));
        }
    }

    void add_double(const double value) noexcept {
        add_u64(std::bit_cast<std::uint64_t>(value));
    }

    [[nodiscard]] std::string hex() const {
        std::ostringstream output;
        output << "fnv1a64:" << std::hex << std::setw(16) << std::setfill('0')
               << value_;
        return output.str();
    }

private:
    std::uint64_t value_ = 14695981039346656037ULL;
};

std::string json_quote(const std::string_view value) {
    std::ostringstream output;
    output << '"';
    for (const unsigned char byte : value) {
        switch (byte) {
            case '"': output << "\\\""; break;
            case '\\': output << "\\\\"; break;
            case '\b': output << "\\b"; break;
            case '\f': output << "\\f"; break;
            case '\n': output << "\\n"; break;
            case '\r': output << "\\r"; break;
            case '\t': output << "\\t"; break;
            default:
                if (byte < 0x20U) {
                    output << "\\u" << std::hex << std::setw(4)
                           << std::setfill('0') << static_cast<int>(byte)
                           << std::dec;
                } else {
                    output << static_cast<char>(byte);
                }
        }
    }
    output << '"';
    return output.str();
}

std::string json_number(const double value) {
    if (!std::isfinite(value)) {
        return "null";
    }
    std::ostringstream output;
    output << std::setprecision(17) << value;
    return output.str();
}

// Digests are built from this rendered text rather than the raw binary value, so
// negative zero hashes like positive zero and a difference below the printed
// nine-decimal resolution cannot change an input or output digest.
std::string canonical_number(double value) {
    if (std::isnan(value)) return "nan";
    if (std::isinf(value)) return value < 0.0 ? "-inf" : "inf";
    if (value == 0.0) value = 0.0;
    std::ostringstream output;
    output << std::fixed << std::setprecision(9) << value;
    return output.str();
}

std::string values_digest(const std::vector<double>& values) {
    fnv64 digest;
    for (const double value : values) {
        digest.add_bytes(canonical_number(value));
        digest.add_byte(static_cast<std::uint8_t>(';'));
    }
    return digest.hex();
}

void add_canonical_value(fnv64& digest, const double value) {
    digest.add_bytes(canonical_number(value));
    digest.add_byte(static_cast<std::uint8_t>(';'));
}

struct count_summary {
    std::vector<std::uint64_t> samples;
    double median = 0.0;
    std::uint64_t minimum = 0U;
    std::uint64_t maximum = 0U;
    std::uint64_t spread = 0U;
};

struct timing_summary {
    std::vector<double> samples;
    double median = 0.0;
    double minimum = 0.0;
    double maximum = 0.0;
    double spread = 0.0;
    count_summary operator_new_calls;
    count_summary requested_bytes;
    std::uint64_t stage_invocations_per_sample = 1U;
};

struct kernel_diagnostic_sample {
    std::uint64_t weight_kernel_calls = 0U;
    std::uint64_t information_coefficient_kernel_calls = 0U;
};

void summarize_counts(count_summary& summary) {
    auto ordered = summary.samples;
    std::sort(ordered.begin(), ordered.end());
    summary.minimum = ordered.front();
    summary.maximum = ordered.back();
    summary.median = static_cast<double>(ordered[ordered.size() / 2U]);
    if (ordered.size() % 2U == 0U) {
        summary.median = (static_cast<double>(ordered[ordered.size() / 2U - 1U]) +
                          static_cast<double>(ordered[ordered.size() / 2U])) /
                         2.0;
    }
    summary.spread = summary.maximum - summary.minimum;
}

template <typename Function, typename Observer>
timing_summary measure(Function&& function, const int warmups,
                       const int repeats, Observer&& observer) {
    for (int index = 0; index < warmups; ++index) {
        function();
    }
    timing_summary result;
    result.samples.reserve(static_cast<std::size_t>(repeats));
    // Warmups never enter the sample set, and the observer runs after the stop
    // clock, so digest capture and kernel-counter reads stay outside the interval;
    // the allocation counter is the exception, because its atomics fire from
    // operator new inside the measured call whenever profiling is enabled.
    if (allocation_profile_enabled) {
        result.operator_new_calls.samples.reserve(static_cast<std::size_t>(repeats));
        result.requested_bytes.samples.reserve(static_cast<std::size_t>(repeats));
    }
    for (int index = 0; index < repeats; ++index) {
        std::optional<benchmark_allocation_counter::scope> allocations;
        if (allocation_profile_enabled) allocations.emplace();
        const auto started = std::chrono::steady_clock::now();
        function();
        const auto stopped = std::chrono::steady_clock::now();
        result.samples.push_back(
            std::chrono::duration<double, std::milli>(stopped - started).count());
        if (allocations) {
            const auto allocation_counts = allocations->finish();
            result.operator_new_calls.samples.push_back(allocation_counts.calls);
            result.requested_bytes.samples.push_back(
                allocation_counts.requested_bytes);
        }
        observer(index);
    }
    auto ordered = result.samples;
    std::sort(ordered.begin(), ordered.end());
    result.minimum = ordered.front();
    result.maximum = ordered.back();
    result.median = ordered[ordered.size() / 2U];
    if (ordered.size() % 2U == 0U) {
        result.median = (ordered[ordered.size() / 2U - 1U] +
                         ordered[ordered.size() / 2U]) /
                        2.0;
    }
    result.spread = result.maximum - result.minimum;
    if (allocation_profile_enabled) {
        summarize_counts(result.operator_new_calls);
        summarize_counts(result.requested_bytes);
    }
    return result;
}

template <typename Function>
timing_summary measure(Function&& function, const int warmups,
                       const int repeats) {
    return measure(std::forward<Function>(function), warmups, repeats,
                   [](const int) noexcept {});
}

std::string count_json(const count_summary& summary) {
    std::ostringstream output;
    output << "{\"samples\":[";
    for (std::size_t index = 0; index < summary.samples.size(); ++index) {
        if (index != 0U) output << ',';
        output << summary.samples[index];
    }
    output << "],\"median\":" << json_number(summary.median)
           << ",\"min\":" << summary.minimum
           << ",\"max\":" << summary.maximum
           << ",\"spread\":" << summary.spread << '}';
    return output.str();
}

std::string timing_json(const timing_summary& summary) {
    std::ostringstream output;
    output << "{\"samples_ms\":[";
    for (std::size_t index = 0; index < summary.samples.size(); ++index) {
        if (index != 0U) output << ',';
        output << json_number(summary.samples[index]);
    }
    output << "],\"median_ms\":" << json_number(summary.median)
           << ",\"min_ms\":" << json_number(summary.minimum)
           << ",\"max_ms\":" << json_number(summary.maximum)
           << ",\"spread_ms\":" << json_number(summary.spread)
           << ",\"stage_invocations_per_sample\":"
           << summary.stage_invocations_per_sample;
    if (allocation_profile_enabled) {
        output << ",\"global_new_calls\":"
               << count_json(summary.operator_new_calls)
               << ",\"requested_bytes\":"
               << count_json(summary.requested_bytes);
    } else {
        output << ",\"global_new_calls\":null,\"requested_bytes\":null";
    }
    output << '}';
    return output.str();
}

struct raw_row {
    int date = 0;
    int asset = 0;
    double factor = 0.0;
    double one_day = 0.0;
    double five_day = 0.0;
};

struct panel_data {
    fincore::prepared_factor_data data;
    std::size_t dates = 0U;
    std::size_t asset_count = 0U;
    std::size_t source_rows = 0U;
};

struct panel_spec {
    int dates;
    int assets;
    int stride;
};

panel_spec spec_for(const std::string_view name) {
    if (name == "dense") return {250, 100, 1};
    if (name == "sparse") return {250, 1000, 20};
    if (name == "scaling-small") return {50, 50, 1};
    if (name == "scaling-large") return {100, 100, 1};
    if (name == "portfolio") return {250, 100, 1};
    throw std::invalid_argument("unknown panel case");
}

std::vector<raw_row> generate_panel(const panel_spec spec) {
    std::vector<raw_row> rows;
    rows.reserve(static_cast<std::size_t>(spec.dates) *
                 static_cast<std::size_t>(spec.assets));
    for (int date = 0; date < spec.dates; ++date) {
        for (int asset = 0; asset < spec.assets; ++asset) {
            if (spec.stride > 1 && (date * 7 + asset) % spec.stride != 0) {
                continue;
            }
            rows.push_back({
                date,
                asset,
                std::sin((date + 1) * 0.17 + asset * 0.9) +
                    0.03 * static_cast<double>(asset % 11) +
                    0.002 * std::cos((date + 1) * 0.05),
                0.002 * std::sin((date + 1) * 0.11 + asset * 0.7) +
                    0.0004 * static_cast<double>(asset % 13),
                0.01 * std::sin((date + 1) * 0.07 + asset * 0.3) -
                    0.0002 * static_cast<double>(asset % 7)});
        }
    }
    return rows;
}

panel_data convert_panel(const std::vector<raw_row>& raw,
                         const panel_spec spec) {
    panel_data result;
    result.data.forward_labels = {"1D", "5D"};
    result.dates = static_cast<std::size_t>(spec.dates);
    result.asset_count = static_cast<std::size_t>(spec.assets);
    result.source_rows = raw.size();
    result.data.rows.reserve(raw.size());
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    std::size_t first = 0U;
    while (first < raw.size()) {
        std::size_t last = first + 1U;
        while (last < raw.size() && raw[last].date == raw[first].date) {
            ++last;
        }
        std::vector<std::size_t> ranks(last - first);
        std::iota(ranks.begin(), ranks.end(), 0U);
        std::sort(ranks.begin(), ranks.end(), [&raw, first](const auto left,
                                                            const auto right) {
            const auto& lhs = raw[first + left];
            const auto& rhs = raw[first + right];
            if (lhs.factor != rhs.factor) return lhs.factor < rhs.factor;
            return lhs.asset < rhs.asset;
        });
        std::vector<int> quantiles(last - first, 1);
        const auto count = static_cast<int>(last - first);
        for (int rank = 0; rank < count; ++rank) {
            quantiles[ranks[static_cast<std::size_t>(rank)]] =
                1 + rank * 5 / std::max(count, 1);
        }
        for (std::size_t index = first; index < last; ++index) {
            const auto& source = raw[index];
            result.data.rows.push_back(
                {start + static_cast<std::int64_t>(source.date) * day_ns,
                 "A" + std::to_string(source.asset), source.factor,
                 quantiles[index - first], false, "",
                 {source.one_day, source.five_day}});
        }
        first = last;
    }
    return result;
}

std::string panel_input_digest(const fincore::prepared_factor_data& data) {
    fnv64 digest;
    digest.add_bytes("panel-input-v1");
    for (const auto& label : data.forward_labels) {
        digest.add_u64(label.size());
        digest.add_bytes(label);
    }
    for (const auto& row : data.rows) {
        digest.add_i64(row.date);
        digest.add_u64(row.asset.size());
        digest.add_bytes(row.asset);
        add_canonical_value(digest, row.factor);
        digest.add_i32(row.quantile);
        for (const double value : row.forward_returns) add_canonical_value(digest, value);
    }
    return digest.hex();
}

std::string bytes_digest(const std::string_view bytes) {
    fnv64 digest;
    digest.add_bytes(bytes);
    return digest.hex();
}

double rss_megabytes() {
#if defined(_WIN32)
    using get_process_memory_info_fn = BOOL(WINAPI*)(
        HANDLE, PPROCESS_MEMORY_COUNTERS, DWORD);
    const auto kernel32 = GetModuleHandleW(L"kernel32.dll");
    const auto query = kernel32 == nullptr
        ? nullptr
        : reinterpret_cast<get_process_memory_info_fn>(GetProcAddress(
              kernel32, "K32GetProcessMemoryInfo"));
    if (query == nullptr) return 0.0;
    PROCESS_MEMORY_COUNTERS counters{};
    counters.cb = static_cast<DWORD>(sizeof(counters));
    if (query(GetCurrentProcess(), &counters, counters.cb) == FALSE) {
        return 0.0;
    }
    return static_cast<double>(counters.PeakWorkingSetSize) /
           (1024.0 * 1024.0);
#else
    rusage usage{};
    if (getrusage(RUSAGE_SELF, &usage) != 0) return 0.0;
#if defined(__APPLE__)
    return static_cast<double>(usage.ru_maxrss) / (1024.0 * 1024.0);
#else
    return static_cast<double>(usage.ru_maxrss) / 1024.0;
#endif
#endif
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) throw std::runtime_error("could not open benchmark artifact");
    return {std::istreambuf_iterator<char>{input},
            std::istreambuf_iterator<char>{}};
}

void write_file(const std::filesystem::path& path,
                const std::string_view content) {
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    if (!output) throw std::runtime_error("could not create benchmark artifact");
    output.write(content.data(), static_cast<std::streamsize>(content.size()));
    output.close();
    if (!output) throw std::runtime_error("could not write benchmark artifact");
}

struct options {
    std::string case_name;
    std::filesystem::path scratch_dir;
    int repeats = 9;
    int warmups = 2;
    bool allocation_profile = false;
};

options parse_options(const int argc, char** argv) {
    options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view arg{argv[index]};
        if (arg == "--allocation-profile") {
            result.allocation_profile = true;
            continue;
        }
        if (index + 1 >= argc) {
            throw std::invalid_argument("missing value after " + std::string{arg});
        }
        const std::string value{argv[++index]};
        if (arg == "--case") {
            result.case_name = value;
        } else if (arg == "--repeats") {
            result.repeats = std::stoi(value);
        } else if (arg == "--warmups") {
            result.warmups = std::stoi(value);
        } else if (arg == "--scratch-dir") {
            result.scratch_dir = value;
        } else {
            throw std::invalid_argument("unknown argument: " + std::string{arg});
        }
    }
    if (result.case_name.empty()) throw std::invalid_argument("--case is required");
    if (result.repeats < 7) throw std::invalid_argument("--repeats must be at least 7");
    if (result.warmups < 0) throw std::invalid_argument("--warmups must not be negative");
    return result;
}

std::string platform_name() {
#if defined(__APPLE__)
    return "macos";
#elif defined(__linux__)
    return "linux";
#elif defined(_WIN32)
    return "windows";
#else
    return "unknown";
#endif
}

std::string compiler_name() {
#if defined(_MSC_VER)
    return "MSVC " + std::to_string(_MSC_VER);
#elif defined(__VERSION__)
    return __VERSION__;
#else
    return "unknown";
#endif
}

std::string case_result_json(
    const std::string_view case_name, const std::string_view workload_json,
    const std::string_view input_digest, const std::string_view output_digest,
    const std::map<std::string, timing_summary>& timings,
    const std::map<std::string, std::string>& checks,
    const int warmups, const int repeats, const std::string_view details_json) {
    std::ostringstream output;
    output << "{\"schema_version\":3,\"source_schema\":"
           << json_quote(source_schema) << ",\"case\":" << json_quote(case_name)
           << ",\"digest_scheme\":\"fnv1a64-canonical-decimal-9\""
           << ",\"workload\":" << workload_json
           << ",\"warmups\":" << warmups << ",\"repeats\":" << repeats
           << ",\"input_digest\":" << json_quote(input_digest)
           << ",\"output_digest\":" << json_quote(output_digest)
           << ",\"measurements_ms\":{";
    bool first = true;
    for (const auto& [name, summary] : timings) {
        if (!first) output << ',';
        first = false;
        output << json_quote(name) << ':' << timing_json(summary);
    }
    output << "},\"checks\":{";
    first = true;
    for (const auto& [name, value] : checks) {
        if (!first) output << ',';
        first = false;
        output << json_quote(name) << ':' << value;
    }
    output << "},\"details\":" << details_json
           << ",\"rss_peak_mb\":" << json_number(rss_megabytes())
           << ",\"allocation_instrumentation\":{\"enabled\":"
           << (allocation_profile_enabled ? "true" : "false")
           << ",\"counted_calls\":\"successful global operator new/new[] and aligned forms\",\"bytes\":\"cumulative requested bytes, not live bytes or RSS\"}"
           << ",\"environment\":{\"platform\":"
           << json_quote(platform_name()) << ",\"compiler\":"
           << json_quote(compiler_name()) << ",\"cplusplus\":" << __cplusplus
           << "}}";
    return output.str();
}

std::vector<double> generate_returns(const std::size_t rows,
                                     const std::size_t column) {
    std::vector<double> values;
    values.reserve(rows);
    for (std::size_t row = 0; row < rows; ++row) {
        const auto r = static_cast<double>(row);
        const auto c = static_cast<double>(column);
        const int residue = static_cast<int>((row + 3U * column) % 19U) - 9;
        values.push_back(std::sin((r + 1.0) * 0.013 + c * 0.21) * 0.01 +
                         0.0002 * static_cast<double>(residue));
    }
    return values;
}

std::vector<fincore::timestamp> generate_index(const std::size_t rows) {
    std::vector<fincore::timestamp> index;
    index.reserve(rows);
    const auto start = fincore::utc_timestamp(2000, 1, 1);
    for (std::size_t row = 0; row < rows; ++row) {
        index.push_back(start + static_cast<std::int64_t>(row) * day_ns);
    }
    return index;
}

std::string returns_input_digest(const std::vector<std::vector<double>>& columns) {
    fnv64 digest;
    digest.add_bytes("returns-input-v1");
    digest.add_u64(columns.size());
    if (!columns.empty()) digest.add_u64(columns.front().size());
    for (std::size_t row = 0; !columns.empty() && row < columns.front().size(); ++row) {
        for (const auto& column : columns) add_canonical_value(digest, column[row]);
    }
    return digest.hex();
}

std::string workload_json_for_returns(const std::string_view kind,
                                     const std::size_t rows,
                                     const std::size_t columns,
                                     const std::size_t window = 0U) {
    std::ostringstream output;
    output << "{\"id\":" << json_quote(kind) << ",\"rows\":" << rows
           << ",\"columns\":" << columns
           << ",\"annualization\":252";
    if (window != 0U) output << ",\"window\":" << window;
    output << '}';
    return output.str();
}

constexpr std::size_t rolling_engine_rows = 50'000U;
constexpr std::size_t rolling_engine_window = 252U;
constexpr std::size_t rolling_engine_benchmark_stride = 97U;
constexpr std::size_t rolling_engine_return_nan_stride = 997U;
constexpr std::size_t rolling_engine_benchmark_nan_stride = 1'237U;

struct rolling_engine_input {
    std::vector<fincore::timestamp> return_index;
    std::vector<double> returns;
    std::vector<fincore::timestamp> benchmark_index;
    std::vector<double> benchmark;
};

rolling_engine_input generate_rolling_engine_input() {
    rolling_engine_input input;
    input.return_index.reserve(rolling_engine_rows);
    input.returns.reserve(rolling_engine_rows);
    input.benchmark_index.reserve(rolling_engine_rows);
    input.benchmark.reserve(rolling_engine_rows);
    const auto start = fincore::utc_timestamp(2000, 1, 1);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    for (std::size_t row = 0U; row < rolling_engine_rows; ++row) {
        const auto timestamp =
            start + static_cast<std::int64_t>(row) * day_ns;
        const int return_cycle = static_cast<int>((row * 37U + 11U) % 199U) - 99;
        const int return_noise = static_cast<int>((row * 13U + 7U) % 23U) - 11;
        double return_value = static_cast<double>(return_cycle) * 0.00008 +
                              static_cast<double>(return_noise) * 0.000003;
        if (row != 0U && row % rolling_engine_return_nan_stride == 0U) {
            return_value = nan;
        }
        input.return_index.push_back(timestamp);
        input.returns.push_back(return_value);

        if (row != 0U && row % rolling_engine_benchmark_stride == 0U) {
            continue;
        }
        const int benchmark_cycle =
            static_cast<int>((row * 71U + 3U) % 211U) - 105;
        const int benchmark_noise =
            static_cast<int>((row * 11U + 2U) % 17U) - 8;
        double benchmark_value =
            static_cast<double>(benchmark_cycle) * 0.00005 +
            static_cast<double>(benchmark_noise) * 0.000002;
        if (row != 0U && row % rolling_engine_benchmark_nan_stride == 0U) {
            benchmark_value = nan;
        }
        input.benchmark_index.push_back(timestamp);
        input.benchmark.push_back(benchmark_value);
    }
    return input;
}

std::string rolling_engine_input_digest(const rolling_engine_input& input) {
    fnv64 digest;
    digest.add_bytes("rolling-engine-input-v1");
    digest.add_u64(input.return_index.size());
    for (std::size_t row = 0U; row < input.return_index.size(); ++row) {
        digest.add_i64(input.return_index[row]);
        add_canonical_value(digest, input.returns[row]);
    }
    digest.add_u64(input.benchmark_index.size());
    for (std::size_t row = 0U; row < input.benchmark_index.size(); ++row) {
        digest.add_i64(input.benchmark_index[row]);
        add_canonical_value(digest, input.benchmark[row]);
    }
    return digest.hex();
}

std::string rolling_engine_output_digest(
    const std::map<std::string, fincore::dataframe>& outputs) {
    fnv64 digest;
    digest.add_bytes("rolling-engine-output-v1");
    digest.add_u64(outputs.size());
    for (const auto& [name, frame] : outputs) {
        digest.add_u64(name.size());
        digest.add_bytes(name);
        const auto& index = frame.get_index();
        const auto values = fincore::require_double_column(
            frame, fincore::returns_column);
        digest.add_u64(index.size());
        for (std::size_t row = 0U; row < index.size(); ++row) {
            digest.add_i64(index[row]);
            add_canonical_value(digest, values[row]);
        }
    }
    return digest.hex();
}

struct series_comparison {
    bool index_valid = false;
    bool nan_mask_valid = false;
    bool values_valid = false;
};

series_comparison compare_rolling_series(const fincore::dataframe& actual,
                                        const fincore::dataframe& expected) {
    series_comparison comparison;
    comparison.index_valid = actual.get_index() == expected.get_index();
    const auto actual_values =
        fincore::require_double_column(actual, fincore::returns_column);
    const auto expected_values =
        fincore::require_double_column(expected, fincore::returns_column);
    if (actual_values.size() != expected_values.size()) {
        return comparison;
    }

    comparison.nan_mask_valid = true;
    comparison.values_valid = true;
    for (std::size_t row = 0U; row < actual_values.size(); ++row) {
        const double actual_value = actual_values[row];
        const double expected_value = expected_values[row];
        if (std::isnan(actual_value) != std::isnan(expected_value)) {
            comparison.nan_mask_valid = false;
            comparison.values_valid = false;
            continue;
        }
        if (std::isnan(expected_value)) {
            continue;
        }
        if (std::isinf(actual_value) || std::isinf(expected_value)) {
            comparison.values_valid = comparison.values_valid &&
                                      actual_value == expected_value;
            continue;
        }
        const double tolerance =
            1e-12 * std::max(1.0, std::abs(expected_value));
        comparison.values_valid = comparison.values_valid &&
                                  std::abs(actual_value - expected_value) <= tolerance;
    }
    return comparison;
}

fincore::dataframe rolling_drawdown_nan_as_zero_oracle(
    const std::vector<fincore::timestamp>& index,
    const std::vector<double>& values, const std::size_t window) {
    const std::size_t output_count = values.size() < window
                                         ? 0U
                                         : values.size() - window + 1U;
    std::vector<fincore::timestamp> output_index;
    std::vector<double> output_values;
    output_index.reserve(output_count);
    output_values.reserve(output_count);
    for (std::size_t begin = 0U; begin < output_count; ++begin) {
        double wealth = 100.0;
        double peak = wealth;
        double worst_drawdown = std::numeric_limits<double>::quiet_NaN();
        for (std::size_t offset = 0U; offset < window; ++offset) {
            const double entry = values[begin + offset];
            wealth *= 1.0 + (std::isnan(entry) ? 0.0 : entry);
            peak = std::fmax(peak, wealth);
            const double drawdown = (wealth - peak) / peak;
            if (!std::isnan(drawdown) &&
                (std::isnan(worst_drawdown) || drawdown < worst_drawdown)) {
                worst_drawdown = drawdown;
            }
        }
        output_index.push_back(index[begin + window - 1U]);
        output_values.push_back(worst_drawdown);
    }
    return fincore::make_series(std::move(output_index), fincore::returns_column,
                                std::move(output_values));
}

void run_rolling_engine_case(const options& args) {
    constexpr std::array metrics{
        fincore::rolling_metric::mean_return,
        fincore::rolling_metric::volatility,
        fincore::rolling_metric::sharpe,
        fincore::rolling_metric::max_drawdown,
        fincore::rolling_metric::beta};
    const auto metric_names = std::array<std::string, metrics.size()>{
        "mean_return", "volatility", "sharpe", "max_drawdown", "beta"};

    rolling_engine_input input;
    const auto generation = measure(
        [&] { input = generate_rolling_engine_input(); }, args.warmups,
        args.repeats);

    std::optional<fincore::rolling_engine> engine;
    const auto conversion = measure([&] {
        auto returns = fincore::make_series(
            input.return_index, fincore::returns_column, input.returns);
        auto benchmark = fincore::make_series(
            input.benchmark_index, fincore::benchmark_column, input.benchmark);
        engine.emplace(std::move(returns), std::move(benchmark),
                       rolling_engine_window);
    }, args.warmups, args.repeats);

    std::map<std::string, fincore::dataframe> outputs;
    std::vector<std::string> output_digests;
    output_digests.reserve(static_cast<std::size_t>(args.repeats));
    const auto compute = measure(
        [&] { outputs = engine->compute(metrics); }, args.warmups,
        args.repeats,
        [&output_digests, &outputs](const int) {
            output_digests.push_back(rolling_engine_output_digest(outputs));
        });

    const auto returns_frame = fincore::make_series(
        input.return_index, fincore::returns_column, input.returns);
    const auto benchmark_frame = fincore::make_series(
        input.benchmark_index, fincore::benchmark_column, input.benchmark);
    const auto volatility_oracle = fincore::roll_annual_volatility(
        returns_frame, rolling_engine_window);
    const auto sharpe_oracle = fincore::roll_sharpe_ratio(
        returns_frame, rolling_engine_window);
    const auto drawdown_oracle = rolling_drawdown_nan_as_zero_oracle(
        input.return_index, input.returns, rolling_engine_window);
    const auto beta_oracle = fincore::roll_beta(
        returns_frame, benchmark_frame, rolling_engine_window);
    std::vector<fincore::timestamp> mean_index;
    std::vector<double> mean_values;
    const std::size_t mean_count = input.returns.size() < rolling_engine_window
                                       ? 0U
                                       : input.returns.size() -
                                             rolling_engine_window + 1U;
    mean_index.reserve(mean_count);
    mean_values.reserve(mean_count);
    for (std::size_t begin = 0U; begin < mean_count; ++begin) {
        double total = 0.0;
        for (std::size_t offset = 0U; offset < rolling_engine_window; ++offset) {
            total += input.returns[begin + offset];
        }
        mean_index.push_back(input.return_index[
            begin + rolling_engine_window - 1U]);
        mean_values.push_back(
            total / static_cast<double>(rolling_engine_window) * 252.0);
    }
    const auto mean_oracle = fincore::make_series(
        std::move(mean_index), fincore::returns_column, std::move(mean_values));

    const std::map<std::string, fincore::dataframe> oracle{
        {"mean_return", mean_oracle}, {"volatility", volatility_oracle},
        {"sharpe", sharpe_oracle}, {"max_drawdown", drawdown_oracle},
        {"beta", beta_oracle}};
    bool metric_names_valid = outputs.size() == metric_names.size();
    bool output_count_valid = metric_names_valid;
    bool oracle_indexes_valid = metric_names_valid;
    bool oracle_nan_masks_valid = metric_names_valid;
    bool oracle_values_valid = metric_names_valid;
    std::ostringstream details;
    details << "{\"output_count\":{";
    bool first_metric = true;
    for (const auto& metric_name : metric_names) {
        const auto actual = outputs.find(metric_name);
        const auto expected = oracle.find(metric_name);
        if (actual == outputs.end() || expected == oracle.end()) {
            metric_names_valid = false;
            output_count_valid = false;
            oracle_indexes_valid = false;
            oracle_nan_masks_valid = false;
            oracle_values_valid = false;
            continue;
        }
        const auto comparison = compare_rolling_series(actual->second,
                                                       expected->second);
        const auto actual_values = fincore::require_double_column(
            actual->second, fincore::returns_column);
        output_count_valid = output_count_valid &&
                             actual_values.size() ==
                                 fincore::require_double_column(
                                     expected->second,
                                     fincore::returns_column).size();
        oracle_indexes_valid = oracle_indexes_valid && comparison.index_valid;
        oracle_nan_masks_valid = oracle_nan_masks_valid &&
                                 comparison.nan_mask_valid;
        oracle_values_valid = oracle_values_valid && comparison.values_valid;
        if (!first_metric) details << ',';
        first_metric = false;
        details << json_quote(metric_name) << ':' << actual_values.size();
    }
    details << "},\"metric_output_digests\":{";
    first_metric = true;
    for (const auto& metric_name : metric_names) {
        const auto actual = outputs.find(metric_name);
        if (actual == outputs.end()) continue;
        if (!first_metric) details << ',';
        first_metric = false;
        const std::map<std::string, fincore::dataframe> one_metric{
            {metric_name, actual->second}};
        details << json_quote(metric_name) << ':'
                << json_quote(rolling_engine_output_digest(one_metric));
    }
    details << "},\"metrics\":{";
    first_metric = true;
    for (const auto& metric_name : metric_names) {
        const auto actual = outputs.find(metric_name);
        if (actual == outputs.end()) continue;
        if (!first_metric) details << ',';
        first_metric = false;
        const auto& frame = actual->second;
        const auto& index = frame.get_index();
        const auto values = fincore::require_double_column(
            frame, fincore::returns_column);
        details << json_quote(metric_name) << ":{\"index\":[";
        for (std::size_t row = 0U; row < index.size(); ++row) {
            if (row != 0U) details << ',';
            details << index[row];
        }
        details << "],\"values\":[";
        for (std::size_t row = 0U; row < values.size(); ++row) {
            if (row != 0U) details << ',';
            if (std::isnan(values[row])) {
                details << "null";
            } else if (std::isinf(values[row])) {
                details << json_quote(values[row] < 0.0 ? "-inf" : "inf");
            } else {
                details << json_number(values[row]);
            }
        }
        details << "]}";
    }
    details << "},\"oracle\":\"rolling_scalar_apis_plus_explicit_mean_and_nan_neutral_drawdown\","
               "\"oracle_in_measurement\":false}";

    const bool output_stable = !output_digests.empty() &&
        std::all_of(output_digests.begin(), output_digests.end(),
                    [&output_digests](const std::string& digest) {
                        return digest == output_digests.front();
                    });
    std::map<std::string, timing_summary> timings{
        {"generation", generation}, {"conversion", conversion},
        {"compute", compute}};
    std::map<std::string, std::string> checks{
        {"metric_names_valid", metric_names_valid ? "true" : "false"},
        {"output_count_valid", output_count_valid ? "true" : "false"},
        {"oracle_indexes_valid", oracle_indexes_valid ? "true" : "false"},
        {"oracle_nan_masks_valid", oracle_nan_masks_valid ? "true" : "false"},
        {"oracle_values_valid", oracle_values_valid ? "true" : "false"},
        {"output_stable_across_repeats", output_stable ? "true" : "false"}};
    const std::string input_digest = rolling_engine_input_digest(input);
    const std::string output_digest = rolling_engine_output_digest(outputs);
    const auto benchmark_nan_count = static_cast<std::size_t>(std::count_if(
        input.benchmark.begin(), input.benchmark.end(),
        [](const double value) { return std::isnan(value); }));
    const auto return_nan_count = static_cast<std::size_t>(std::count_if(
        input.returns.begin(), input.returns.end(),
        [](const double value) { return std::isnan(value); }));
    std::ostringstream workload;
    workload << "{\"id\":\"rolling-engine-v1\",\"rows\":"
             << input.return_index.size()
             << ",\"benchmark_rows\":" << input.benchmark_index.size()
             << ",\"window\":" << rolling_engine_window
             << ",\"annualization\":252,\"metrics\":[";
    for (std::size_t index = 0U; index < metric_names.size(); ++index) {
        if (index != 0U) workload << ',';
        workload << json_quote(metric_names[index]);
    }
    workload << "],\"benchmark_omission_stride\":"
             << rolling_engine_benchmark_stride
             << ",\"return_nan_stride\":"
             << rolling_engine_return_nan_stride
             << ",\"benchmark_nan_stride\":"
             << rolling_engine_benchmark_nan_stride
             << ",\"return_nan_count\":" << return_nan_count
             << ",\"benchmark_nan_count\":" << benchmark_nan_count << '}';
    std::cout << case_result_json(args.case_name, workload.str(), input_digest,
                                  output_digest, timings, checks,
                                  args.warmups, args.repeats, details.str())
              << '\n';
}

void run_returns_case(const options& args) {
    const bool batch = args.case_name == "batched-sharpe";
    const bool rolling = args.case_name == "rolling-sharpe";
    if (!batch && !rolling && args.case_name != "scalar-sharpe") {
        throw std::invalid_argument("unknown case: " + args.case_name);
    }
    const std::size_t rows = batch ? 50'000U : (rolling ? 50'000U : 100'000U);
    const std::size_t columns_count = batch ? 16U : 1U;
    const std::size_t window = rolling ? 252U : 0U;
    std::vector<fincore::timestamp> index;
    std::vector<std::vector<double>> columns(columns_count);
    const auto generation = measure([&] {
        index = generate_index(rows);
        for (std::size_t column = 0; column < columns_count; ++column) {
            columns[column] = generate_returns(rows, column);
        }
    }, args.warmups, args.repeats);

    fincore::dataframe frame;
    const auto conversion = measure([&] {
        frame = fincore::dataframe{};
        auto copied_index = index;
        frame.load_index(std::move(copied_index));
        for (std::size_t column = 0; column < columns_count; ++column) {
            const std::string name = batch ? "r" + std::to_string(column) : "returns";
            auto copied_values = columns[column];
            frame.load_column<double>(name.c_str(), std::move(copied_values),
                                     hmdf::nan_policy::dont_pad_with_nans);
        }
    }, args.warmups, args.repeats);

    std::vector<double> result_values;
    const auto compute = measure([&] {
        result_values.clear();
        result_values.reserve(columns_count);
        if (rolling) {
            const auto output = fincore::rolling_sharpe(frame, window);
            const auto values = fincore::require_double_column(output, "returns");
            result_values.assign(values.begin(), values.end());
        } else {
            for (std::size_t column = 0; column < columns_count; ++column) {
                const std::string name = batch ? "r" + std::to_string(column) : "returns";
                result_values.push_back(fincore::sharpe_ratio(frame, 0.0, 252.0, name));
            }
        }
    }, args.warmups, args.repeats);

    const std::string input_digest = returns_input_digest(columns);
    const std::string output_digest = values_digest(result_values);
    std::map<std::string, timing_summary> timings{{"generation", generation},
                                                   {"conversion", conversion},
                                                   {"compute", compute}};
    const bool output_count_valid = result_values.size() ==
        (rolling ? rows : columns_count);
    std::map<std::string, std::string> checks{
        {"output_count_valid", output_count_valid ? "true" : "false"}};
    const std::string case_kind = rolling ? "rolling-sharpe-v1" :
                                  batch ? "batched-sharpe-v1" : "scalar-sharpe-v1";
    const auto workload = workload_json_for_returns(case_kind, rows, columns_count, window);
    std::ostringstream details;
    details << "{\"output_count\":" << result_values.size()
            << ",\"semantic_values\":[";
    for (std::size_t index = 0; index < result_values.size(); ++index) {
        if (index != 0U) details << ',';
        details << json_number(result_values[index]);
    }
    details << "]}";
    std::cout << case_result_json(args.case_name, workload, input_digest,
                                  output_digest, timings, checks,
                                  args.warmups, args.repeats, details.str()) << '\n';
}

void run_panel_case(const options& args) {
    const auto spec = spec_for(args.case_name);
    std::vector<raw_row> raw;
    const auto generation = measure([&] { raw = generate_panel(spec); },
                                    args.warmups, args.repeats);
    panel_data panel;
    const auto conversion = measure([&] { panel = convert_panel(raw, spec); },
                                    args.warmups, args.repeats);
    fincore::factor_analysis_config config;
    config.periods = {"1D", "5D"};
    config.turnover_periods = {1, 5};
    config.time_aggregation = {"M"};

    std::map<std::string, timing_summary> timings{
        {"generation", generation}, {"conversion", conversion}};
    std::map<std::string, std::string> checks;
    const std::string input_digest = panel_input_digest(panel.data);
    std::string output_digest;
    std::string details_json = "{}";
    if (args.case_name == "portfolio") {
        std::vector<fincore::factor_weight_row> weights;
        fincore::factor_return_table returns;
        timings.emplace("portfolio_weights", measure([&] {
            weights = fincore::factor_weights(panel.data);
        }, args.warmups, args.repeats));
        timings.emplace("portfolio_returns", measure([&] {
            returns = fincore::factor_returns(panel.data);
        }, args.warmups, args.repeats));
        std::vector<double> weight_values;
        weight_values.reserve(weights.size());
        for (const auto& row : weights) weight_values.push_back(row.weight);
        const std::string weight_digest = values_digest(weight_values);
        const std::string return_digest = values_digest(returns.values);
        fnv64 combined;
        combined.add_bytes(weight_digest);
        combined.add_bytes(return_digest);
        output_digest = combined.hex();
        std::ostringstream details;
        details << "{\"rows\":" << panel.data.rows.size()
                << ",\"weight_rows\":" << weights.size()
                << ",\"return_values\":" << returns.values.size()
                << ",\"weights_digest\":" << json_quote(weight_digest)
                << ",\"returns_digest\":" << json_quote(return_digest) << '}';
        details_json = details.str();
        checks.emplace("weights_nonempty", weights.empty() ? "false" : "true");
        checks.emplace("returns_nonempty", returns.values.empty() ? "false" : "true");
    } else {
        fincore::factor_analysis_model model;
        std::vector<std::string> fingerprints;
#if FINCORE_FACTOR_BENCHMARK_HAS_KERNEL_DIAGNOSTICS
        std::vector<kernel_diagnostic_sample> kernel_samples;
#endif
        auto analyze = [&] {
#if FINCORE_FACTOR_BENCHMARK_HAS_KERNEL_DIAGNOSTICS
            fincore::detail::reset_factor_compute_diagnostics();
#endif
            model = fincore::analyze_factor(panel.data, config);
            fingerprints.push_back(model.result_fingerprint);
        };
#if FINCORE_FACTOR_BENCHMARK_HAS_KERNEL_DIAGNOSTICS
        timings.emplace("compute", measure(analyze, args.warmups, args.repeats,
            [&kernel_samples](const int) {
                const auto counters = fincore::detail::get_factor_compute_diagnostics();
                kernel_samples.push_back({counters.weight_kernel_calls,
                                          counters.information_coefficient_kernel_calls});
            }));
#else
        timings.emplace("compute", measure(analyze, args.warmups, args.repeats));
#endif
        const bool stable = !fingerprints.empty() &&
            std::all_of(fingerprints.begin(), fingerprints.end(),
                        [&fingerprints](const auto& value) {
                            return value == fingerprints.front();
                        });
        bool isolated = false;
        timings.emplace("model_copy", measure([&] {
            auto copy = model;
            copy.forward_periods.push_back("99D");
            isolated = model.forward_periods.size() == 2U &&
                       copy.forward_periods.size() == 3U;
        }, args.warmups, args.repeats));

        fincore::report_document document;
        timings.emplace("report_projection", measure([&] {
            document = fincore::build_factor_report(model);
        }, args.warmups, args.repeats));
        std::string html;
        timings.emplace("html_render", measure([&] {
            html = fincore::render_html(document);
        }, args.warmups, args.repeats));
        std::string xlsx;
        timings.emplace("xlsx_render", measure([&] {
            xlsx = fincore::render_xlsx(document);
        }, args.warmups, args.repeats));

        if (args.scratch_dir.empty()) {
            throw std::invalid_argument("--scratch-dir is required for panel cases");
        }
        std::filesystem::create_directories(args.scratch_dir);
        const auto html_path = args.scratch_dir / "factor-report.html";
        const auto xlsx_path = args.scratch_dir / "factor-report.xlsx";
        bool io_ok = false;
        timings.emplace("file_io", measure([&] {
            write_file(html_path, html);
            write_file(xlsx_path, xlsx);
            const auto html_read = read_file(html_path);
            const auto xlsx_read = read_file(xlsx_path);
            io_ok = html_read == html && xlsx_read == xlsx;
        }, args.warmups, args.repeats));

        fnv64 combined;
        combined.add_bytes(model.result_fingerprint);
        combined.add_bytes(bytes_digest(html));
        combined.add_bytes(bytes_digest(xlsx));
        output_digest = combined.hex();
        const auto returns_digest = values_digest(model.factor_returns.values);
        std::ostringstream details;
        details << "{\"rows\":" << panel.data.rows.size()
                << ",\"model_fingerprint\":"
                << json_quote(model.result_fingerprint)
                << ",\"model_copy_invocations_per_measured_sample\":1"
                << ",\"factor_returns_digest\":" << json_quote(returns_digest)
                << ",\"html_bytes\":" << html.size()
                << ",\"xlsx_bytes\":" << xlsx.size()
                << ",\"html_digest\":" << json_quote(bytes_digest(html))
                << ",\"xlsx_digest\":" << json_quote(bytes_digest(xlsx));
#if FINCORE_FACTOR_BENCHMARK_HAS_KERNEL_DIAGNOSTICS
        details << ",\"factor_compute_diagnostics\":{\"available\":true"
                << ",\"weight_kernel_calls_per_analyze\":[";
        for (std::size_t index = 0; index < kernel_samples.size(); ++index) {
            if (index != 0U) details << ',';
            details << kernel_samples[index].weight_kernel_calls;
        }
        details << "],\"information_coefficient_kernel_calls_per_analyze\":[";
        for (std::size_t index = 0; index < kernel_samples.size(); ++index) {
            if (index != 0U) details << ',';
            details << kernel_samples[index].information_coefficient_kernel_calls;
        }
        details << "]}";
#else
        details << ",\"factor_compute_diagnostics\":{\"available\":false}";
#endif
        details << '}';
        details_json = details.str();
        checks.emplace("compute_fingerprint_stable", stable ? "true" : "false");
        checks.emplace("model_copy_isolated", isolated ? "true" : "false");
        checks.emplace("file_io_roundtrip", io_ok ? "true" : "false");
    }
    std::ostringstream workload;
    workload << "{\"id\":" << json_quote(args.case_name == "portfolio" ?
                                              "factor-portfolio-v1" :
                                              "factor-analysis-panel-v1")
             << ",\"dates\":" << spec.dates << ",\"assets\":" << spec.assets
             << ",\"stride\":" << spec.stride
             << ",\"forward_periods\":[\"1D\",\"5D\"]"
             << ",\"turnover_periods\":[1,5],\"time_aggregation\":[\"M\"]"
             << ",\"long_short\":true,\"group_neutral\":false"
             << ",\"equal_weight\":false,\"by_group\":false}";
    std::cout << case_result_json(args.case_name, workload.str(), input_digest,
                                  output_digest, timings, checks,
                                  args.warmups, args.repeats, details_json) << '\n';
}

}  // namespace

int main(int argc, char** argv) {
    try {
        const auto args = parse_options(argc, argv);
        allocation_profile_enabled = args.allocation_profile;
        if (args.case_name == "rolling-engine") {
            run_rolling_engine_case(args);
        } else if (args.case_name == "scalar-sharpe" ||
            args.case_name == "batched-sharpe" ||
            args.case_name == "rolling-sharpe") {
            run_returns_case(args);
        } else {
            run_panel_case(args);
        }
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n';
        return 2;
    }
}
