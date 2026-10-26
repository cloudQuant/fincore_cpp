#include <fincore/calendar.hpp>
#include <fincore/detail/digest.hpp>
#include <fincore/factor/analysis.hpp>
#include <fincore/factor/data.hpp>
#include <fincore/factor/performance.hpp>
#include <fincore/factor/portfolio.hpp>
#include <fincore/factor/research.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <iterator>
#include <numeric>
#include <new>
#include <sstream>
#include <string>
#include <string_view>
#include <sys/resource.h>
#include <unistd.h>
#include <vector>


namespace bench_tracker {

// The header is written immediately before the address handed back, so every
// tracked block carries its own size and the malloc pointer needed to release
// it; the magic word is what tells a tracked block from memory allocated
// elsewhere.
struct header {
    std::uint64_t magic = 0;
    std::size_t size = 0;
    void* raw = nullptr;
};

constexpr std::uint64_t magic_value = 0x46494e43374b4954ULL;
std::atomic<std::uint64_t> live_bytes{0};
std::atomic<std::uint64_t> peak_bytes{0};

void* allocate(const std::size_t size, const std::size_t alignment) {
    const std::size_t effective_alignment =
        std::max(alignment, alignof(header));
    void* raw = std::malloc(size + effective_alignment + sizeof(header));
    if (raw == nullptr) {
        throw std::bad_alloc{};
    }
    const auto base = reinterpret_cast<std::uintptr_t>(raw) +
                      sizeof(header);
    const auto aligned =
        (base + effective_alignment - 1U) &
        ~(static_cast<std::uintptr_t>(effective_alignment) - 1U);
    auto* block = reinterpret_cast<header*>(aligned) - 1;
    block->magic = magic_value;
    block->size = size;
    block->raw = raw;
    const std::uint64_t current =
        live_bytes.fetch_add(size, std::memory_order_relaxed) + size;
    std::uint64_t observed = peak_bytes.load(std::memory_order_relaxed);
    while (current > observed &&
           !peak_bytes.compare_exchange_weak(observed, current,
                                             std::memory_order_relaxed)) {
    }
    return reinterpret_cast<void*>(aligned);
}

void deallocate(void* pointer) {
    if (pointer == nullptr) {
        return;
    }
    auto* block = reinterpret_cast<header*>(pointer) - 1;
    // A block that does not carry the magic word was not allocated here, so it
    // is released untouched rather than counted against the tracked ledger.
    if (block->magic != magic_value) {
        std::free(pointer);
        return;
    }
    block->magic = 0;
    live_bytes.fetch_sub(block->size, std::memory_order_relaxed);
    std::free(block->raw);
}

void reset_peak() {
    // Seeding from the bytes already live means a measurement window reports
    // its true high-water mark instead of the growth inside the window alone.
    peak_bytes.store(live_bytes.load(std::memory_order_relaxed),
                     std::memory_order_relaxed);
}

struct snapshot {
    std::uint64_t live = 0;
    std::uint64_t peak = 0;
};

snapshot current() {
    return {live_bytes.load(std::memory_order_relaxed),
            peak_bytes.load(std::memory_order_relaxed)};
}

struct validation {
    bool passed = false;
    std::uint64_t live_before = 0;
    std::uint64_t live_after = 0;
    std::uint64_t peak = 0;
};

validation validate() {
    // Self-test of the tracker itself: the 1 MiB and 2 MiB requests have to move
    // the ledger as expected, so live rises by at least each size, the
    // high-water mark reaches the baseline plus the 2 MiB step, and releasing
    // both brings live back to where it started.
    validation result;
    result.live_before = current().live;
    reset_peak();
    void* first = ::operator new(1U << 20U);
    const auto middle = current();
    ::operator delete(first);
    void* second = ::operator new(2U << 20U);
    const auto high = current();
    ::operator delete(second);
    result.live_after = current().live;
    result.peak = high.peak;
    result.passed = middle.live >= result.live_before + (1U << 20U) &&
                    high.peak >= result.live_before + (2U << 20U) &&
                    result.live_after == result.live_before;
    return result;
}

}  // namespace bench_tracker

void* operator new(const std::size_t size) {
    return bench_tracker::allocate(size, alignof(std::max_align_t));
}
void* operator new[](const std::size_t size) {
    return bench_tracker::allocate(size, alignof(std::max_align_t));
}
void operator delete(void* pointer) noexcept {
    bench_tracker::deallocate(pointer);
}
void operator delete[](void* pointer) noexcept {
    bench_tracker::deallocate(pointer);
}
void operator delete(void* pointer, std::size_t) noexcept {
    bench_tracker::deallocate(pointer);
}
void operator delete[](void* pointer, std::size_t) noexcept {
    bench_tracker::deallocate(pointer);
}
void* operator new(const std::size_t size, const std::align_val_t alignment) {
    return bench_tracker::allocate(size, static_cast<std::size_t>(alignment));
}
void* operator new[](const std::size_t size, const std::align_val_t alignment) {
    return bench_tracker::allocate(size, static_cast<std::size_t>(alignment));
}
void operator delete(void* pointer, const std::align_val_t) noexcept {
    bench_tracker::deallocate(pointer);
}
void operator delete[](void* pointer, const std::align_val_t) noexcept {
    bench_tracker::deallocate(pointer);
}
void operator delete(void* pointer, std::size_t,
                     const std::align_val_t) noexcept {
    bench_tracker::deallocate(pointer);
}
void operator delete[](void* pointer, std::size_t,
                       const std::align_val_t) noexcept {
    bench_tracker::deallocate(pointer);
}

namespace {

bool track_enabled = false;
std::uint64_t tracked_live_before = 0U;
std::uint64_t tracked_peak = 0U;
std::uint64_t tracked_live_after = 0U;

constexpr fincore::timestamp day_ns = 86'400'000'000'000LL;

struct bench_inputs {
    fincore::factor_series factor;
    fincore::dataframe prices;
    std::vector<std::string> assets;
    fincore::prepared_factor_data prepared;
};

bench_inputs make_inputs(const std::size_t assets, const std::size_t dates) {
    bench_inputs input;
    const auto start = fincore::utc_timestamp(2021, 1, 4);
    std::vector<fincore::timestamp> grid;
    grid.reserve(dates + 6U);
    for (std::size_t index = 0; index < dates + 6U; ++index) {
        grid.push_back(start + static_cast<fincore::timestamp>(index) *
                                   day_ns);
    }
    input.prices.load_index(std::move(grid));
    input.assets.reserve(assets);
    for (std::size_t asset = 0; asset < assets; ++asset) {
        std::ostringstream name;
        name << "A" << asset;
        input.assets.push_back(name.str());
        std::vector<double> values;
        values.reserve(grid.size());
        double price = 100.0 + 0.1 * static_cast<double>(asset);
        for (std::size_t position = 0; position < grid.size(); ++position) {
            price *= 1.0 + 0.0005 +
                     0.004 * std::sin(
                                 (static_cast<double>(position) + 1.0) * 0.11 +
                                 static_cast<double>(asset) * 0.07) +
                     0.002 * std::cos(
                                 (static_cast<double>(position) + 1.0) * 0.23 +
                                 static_cast<double>(asset));
            values.push_back(price);
        }
        input.prices.load_column<double>(
            input.assets.back().c_str(), std::move(values),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    for (std::size_t date = 0; date < dates; ++date) {
        for (std::size_t asset = 0; asset < assets; ++asset) {
            input.factor.dates.push_back(
                start + static_cast<fincore::timestamp>(date) * day_ns);
            input.factor.assets.push_back(input.assets[asset]);
            input.factor.values.push_back(
                0.3 * std::sin(
                          (static_cast<double>(date) + 1.0) * 0.17 +
                          static_cast<double>(asset) * 0.013) +
                0.1 * std::cos(
                          (static_cast<double>(date) + 1.0) * 0.05 +
                          static_cast<double>(asset) * 0.07) +
                0.001 * static_cast<double>(asset));
        }
    }
    fincore::prepare_factor_config config;
    config.periods = {1, 5, 10};
    config.quantiles = 5;
    config.max_loss = 1.0;
    input.prepared =
        fincore::prepare_factor_data(input.factor, input.prices, config);
    return input;
}

fincore::research_spec research_spec(const bench_inputs& input) {
    fincore::research_spec spec;
    spec.targets = input.assets;
    spec.start = input.factor.dates.front();
    spec.end = input.factor.dates.back();
    spec.periods = {1, 5, 10};
    spec.quantiles = 5;
    spec.min_assets = 10;
    spec.max_loss = 1.0;
    spec.preprocess = "cross_section_rank";
    spec.stability_lags = {1};
    return spec;
}

std::string digest_bytes(const std::string& text) {
    return fincore::detail::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

std::string double_bits(const double value) {
    std::uint64_t bits = 0;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    return std::to_string(bits);
}

template <typename Result, typename Function>
double measure(const int warmup, const int repeat, Function&& function,
               Result& result) {
    for (int index = 0; index < warmup; ++index) {
        result = function();
    }
    if (track_enabled) {
        tracked_live_before = bench_tracker::current().live;
        bench_tracker::reset_peak();
    }
    std::vector<double> samples;
    samples.reserve(static_cast<std::size_t>(repeat));
    for (int index = 0; index < repeat; ++index) {
        const auto begin = std::chrono::steady_clock::now();
        result = function();
        const auto end = std::chrono::steady_clock::now();
        if (track_enabled) {
            const auto tracked = bench_tracker::current();
            tracked_peak = tracked.peak;
            tracked_live_after = tracked.live;
        }
        samples.push_back(
            std::chrono::duration<double, std::milli>(end - begin).count());
    }
    std::sort(samples.begin(), samples.end());
    return samples[samples.size() / 2U];
}

struct options {
    std::string case_name;
    std::string label;
    std::size_t assets = 200U;
    std::size_t dates = 120U;
    int warmup = 2;
    int repeat = 1;
    bool track = false;
    bool validate_tracker = false;
    std::string preprocess;
};

options parse_options(const int argc, char** argv) {
    options result;
    for (int index = 1; index < argc; ++index) {
        const std::string_view key = argv[index];
        const auto next = [&]() -> std::string {
            if (index + 1 >= argc) {
                throw std::runtime_error{"missing option value"};
            }
            return argv[++index];
        };
        if (key == "--case") {
            result.case_name = next();
        } else if (key == "--label") {
            result.label = next();
        } else if (key == "--assets") {
            result.assets = static_cast<std::size_t>(std::stoull(next()));
        } else if (key == "--dates") {
            result.dates = static_cast<std::size_t>(std::stoull(next()));
        } else if (key == "--warmup") {
            result.warmup = std::stoi(next());
        } else if (key == "--repeat") {
            result.repeat = std::stoi(next());
        } else if (key == "--track") {
            result.track = true;
        } else if (key == "--validate-tracker") {
            result.validate_tracker = true;
        } else if (key == "--preprocess") {
            result.preprocess = next();
        } else {
            throw std::runtime_error{"unknown option: " + std::string{key}};
        }
    }
    return result;
}

long max_rss_bytes() {
    struct rusage usage {};
    getrusage(RUSAGE_SELF, &usage);
#if defined(__APPLE__)
    return usage.ru_maxrss;
#else
    return usage.ru_maxrss * 1024L;
#endif
}

std::string json_escape(const std::string& value) {
    std::string result;
    for (const char character : value) {
        if (character == '"' || character == '\\') {
            result.push_back('\\');
        }
        result.push_back(character);
    }
    return result;
}

}  // namespace

int main(const int argc, char** argv) {
    try {
        const auto args = parse_options(argc, argv);
        if (args.validate_tracker) {
            const auto validation = bench_tracker::validate();
            std::cout << "{\"tracker_validation\":"
                      << (validation.passed ? "true" : "false")
                      << ",\"live_before\":" << validation.live_before
                      << ",\"live_after\":" << validation.live_after
                      << ",\"peak\":" << validation.peak << "}\n";
            return validation.passed ? 0 : 1;
        }
        if (args.case_name.empty()) {
            throw std::runtime_error{"--case is required"};
        }
        track_enabled = args.track;
        auto input = make_inputs(args.assets, args.dates);
        double milliseconds = 0.0;
        std::string digest;
        std::string detail;

        if (args.case_name == "target_validation") {
            auto spec = research_spec(input);
            if (!args.preprocess.empty()) {
                spec.preprocess = args.preprocess;
            }
            fincore::factor_series panel;
            milliseconds = measure(
                args.warmup, args.repeat,
                [&]() {
                    return fincore::detail::research_panel_for_benchmark(
                        input.factor, spec);
                },
                panel);
            std::string text;
            for (std::size_t row = 0; row < panel.dates.size(); ++row) {
                text += std::to_string(panel.dates[row]);
                text += panel.assets[row];
                text += double_bits(panel.values[row]);
            }
            digest = digest_bytes(text);
            detail = "panel_rows=" + std::to_string(panel.dates.size());
        } else if (args.case_name == "analysis_prepare") {
            fincore::factor_analysis_config config;
            config.periods = {"1D", "5D"};
            config.by_group = false;
            fincore::factor_analysis_model model;
            milliseconds = measure(
                args.warmup, args.repeat,
                [&]() {
                    return fincore::analyze_factor(input.prepared, config);
                },
                model);
            digest = model.result_fingerprint;
            detail = "positions=" + std::to_string(model.factor_positions.size());
        } else if (args.case_name == "research_assembly") {
            auto inverse = input.factor;
            for (double& value : inverse.values) {
                value = -value;
            }
            std::vector<fincore::named_factor_panel> panels{
                {"F", input.factor}, {"G", inverse}};
            auto spec = research_spec(input);
            spec.combinations = {{"combo", {{"F", 0.5}, {"G", -0.5}}}};
            fincore::research_panel_result result;
            milliseconds = measure(
                args.warmup, args.repeat,
                [&]() {
                    return fincore::evaluate_factor_research_panel(
                        panels, input.prices, spec);
                },
                result);
            std::string text = result.status;
            for (const auto& factor : result.factors) {
                for (const double value :
                     factor.result.information_coefficient.values) {
                    text += double_bits(value);
                }
            }
            digest = digest_bytes(text);
            detail = "factors=" + std::to_string(result.factors.size());
        } else if (args.case_name == "research_export") {
            const auto spec = research_spec(input);
            const auto result = fincore::evaluate_factor_research(
                input.factor, input.prices, spec);
            std::filesystem::path target;
            if (track_enabled) {
                tracked_live_before = bench_tracker::current().live;
                bench_tracker::reset_peak();
            }
            const auto begin = std::chrono::steady_clock::now();
            for (int index = 0; index < args.repeat; ++index) {
                target = std::filesystem::temp_directory_path() /
                         ("fincore-iter7-export-" +
                          std::to_string(static_cast<long long>(::getpid())) +
                          "-" + std::to_string(index));
                std::filesystem::remove_all(target);
                static_cast<void>(fincore::export_factor_research(
                    result, target.string()));
            }
            const auto end = std::chrono::steady_clock::now();
            if (track_enabled) {
                const auto tracked = bench_tracker::current();
                tracked_peak = tracked.peak;
                tracked_live_after = tracked.live;
            }
            milliseconds =
                std::chrono::duration<double, std::milli>(end - begin).count() /
                static_cast<double>(std::max(args.repeat, 1));
            std::string text;
            std::uint64_t bytes = 0;
            for (const auto& entry :
                 std::filesystem::recursive_directory_iterator(target)) {
                if (entry.is_regular_file()) {
                    std::ifstream stream(entry.path(), std::ios::binary);
                    const std::string contents(
                        (std::istreambuf_iterator<char>(stream)),
                        std::istreambuf_iterator<char>());
                    bytes += contents.size();
                    text += contents;
                }
            }
            digest = digest_bytes(text);
            detail = "bytes=" + std::to_string(bytes);
            std::filesystem::remove_all(target);
        } else if (args.case_name == "positions") {
            std::vector<fincore::factor_weight_row> result;
            milliseconds = measure(
                args.warmup, args.repeat,
                [&]() {
                    auto combined = fincore::factor_positions(
                        input.prepared, "1D");
                    auto five = fincore::factor_positions(input.prepared, "5D");
                    auto ten = fincore::factor_positions(input.prepared, "10D");
                    combined.insert(combined.end(), five.begin(), five.end());
                    combined.insert(combined.end(), ten.begin(), ten.end());
                    return combined;
                },
                result);
            std::string text;
            for (const auto& row : result) {
                text += std::to_string(row.date);
                text += row.asset;
                text += double_bits(row.weight);
            }
            digest = digest_bytes(text);
            detail = "rows=" + std::to_string(result.size());
        } else if (args.case_name == "fingerprint") {
            fincore::factor_analysis_config config;
            config.periods = {"1D"};
            config.include_portfolio_inputs = false;
            fincore::factor_analysis_model model;
            milliseconds = measure(
                args.warmup, args.repeat,
                [&]() {
                    return fincore::analyze_factor(input.prepared, config);
                },
                model);
            digest = model.result_fingerprint;
            detail = "fingerprint_inclusive=1";
        } else if (args.case_name == "fingerprint_stage") {
            fincore::factor_analysis_config config;
            config.periods = {"1D"};
            config.include_portfolio_inputs = false;
            std::string fingerprint;
            milliseconds = measure(
                args.warmup, args.repeat,
                [&]() {
                    return fincore::detail::factor_fingerprint_for_benchmark(
                        input.prepared, config);
                },
                fingerprint);
            digest = fingerprint;
            detail = "fingerprint_stage_only=1";
        } else {
            throw std::runtime_error{"unknown case: " + args.case_name};
        }

        std::cout << "{\"case\":\"" << json_escape(args.case_name)
                  << "\",\"label\":\"" << json_escape(args.label)
                  << "\",\"assets\":" << args.assets
                  << ",\"dates\":" << args.dates
                  << ",\"warmup\":" << args.warmup
                  << ",\"repeat\":" << args.repeat
                  << ",\"preprocess\":\"" << json_escape(args.preprocess)
                  << "\",\"ms\":" << milliseconds
                  << ",\"maxrss_bytes\":" << max_rss_bytes()
                  << ",\"digest\":\"" << digest << "\""
                  << ",\"detail\":\"" << json_escape(detail) << "\"";
        if (track_enabled) {
            std::cout << ",\"track\":{\"live_before\":" << tracked_live_before
                      << ",\"peak\":" << tracked_peak
                      << ",\"live_after\":" << tracked_live_after << "}";
        }
        std::cout << "}\n";
        return 0;
    } catch (const std::exception& error) {
        std::cerr << "benchmark error: " << error.what() << '\n';
        return 1;
    }
}
