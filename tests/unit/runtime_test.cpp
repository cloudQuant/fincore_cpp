#include <fincore/calendar.hpp>
#include <fincore/detail/digest.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/rolling.hpp>
#include <fincore/metrics/stats.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/portfolio.hpp>
#include <fincore/report/risk.hpp>
#include <fincore/report/xlsx.hpp>
#include <fincore/risk/backtesting.hpp>
#include <fincore/risk/report.hpp>
#include <fincore/risk/walk_forward.hpp>
#include <fincore/runtime/runtime.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cmath>
#include <complex>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <functional>
#include <map>
#include <memory>
#include <sstream>
#include <span>
#include <string>
#include <vector>

namespace {

fincore::dataframe returns_fixture() {
    return fincore::make_series({1, 2, 3}, "returns",
                                {0.01, 0.02, -0.01});
}

fincore::dataframe benchmark_fixture() {
    return fincore::make_series({1, 2, 3}, "benchmark",
                                {0.005, 0.01, -0.005});
}

fincore::dataframe portfolio_returns_fixture() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    constexpr auto day = 86'400'000'000'000LL;
    return fincore::make_series(
        {start, start + day, start + 2 * day, start + 3 * day,
         start + 4 * day},
        "returns", {0.01, 0.02, -0.01, 0.03, 0.0});
}

fincore::dataframe portfolio_benchmark_fixture() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    constexpr auto day = 86'400'000'000'000LL;
    return fincore::make_series(
        {start, start + day, start + 2 * day, start + 3 * day,
         start + 4 * day},
        "benchmark", {0.005, 0.01, -0.005, 0.02, 0.0});
}

fincore::dataframe portfolio_positions_fixture() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    constexpr auto day = 86'400'000'000'000LL;
    fincore::dataframe positions;
    positions.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day,
        start + 4 * day});
    positions.load_column<double>(
        "A", {100.0, 100.0, 90.0, 110.0, 110.0},
        hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "cash", {10.0, 10.0, 20.0, 15.0, 15.0},
        hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

std::vector<fincore::portfolio_transaction> portfolio_transactions_fixture() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    constexpr auto day = 86'400'000'000'000LL;
    return {{start, "A", 10.0, 5.0},
            {start, "B", -5.0, 20.0},
            {start + day, "A", -2.0, 6.0}};
}

fincore::dataframe risk_returns_fixture(
    int count = 12, const double first_return_adjustment = 0.0) {
    const auto start = fincore::utc_timestamp(2021, 1, 1);
    constexpr auto day = 86'400'000'000'000LL;
    std::vector<fincore::timestamp> index;
    std::vector<double> values;
    for (int position = 0; position < count; ++position) {
        index.push_back(start + position * day);
        values.push_back(0.01 * std::sin(position * 0.7) +
                         (position == 0 ? first_return_adjustment : 0.0));
    }
    return fincore::make_series(index, "returns", values);
}

fincore::risk_model_spec runtime_risk_spec() {
    fincore::risk_model_spec spec;
    spec.window = 5U;
    spec.refit_cadence = 2U;
    spec.confidence_level = 0.90;
    return spec;
}

fincore::risk_backtest_result runtime_backtest_result() {
    fincore::risk_backtest_result result{};
    result.method = fincore::risk_backtest_method::var;
    result.confidence_level = 0.90;
    result.observations = 7U;
    result.exceptions = 1U;
    result.expected_exceptions = 0.7;
    result.exception_rate = 1.0 / 7.0;
    result.kupiec_lr = 0.5;
    result.kupiec_pvalue = 0.4;
    result.christoffersen_lr = 0.3;
    result.christoffersen_pvalue = 0.6;
    result.inputs_digest = "backtest-inputs";
    result.diagnostics = {{"transition_count", 2.0}};
    result.status = fincore::risk_backtest_status::pass;
    return result;
}

fincore::operation_catalog instrumented_builtin_catalog(
    const std::shared_ptr<std::map<std::string, int>>& counts) {
    const auto base = fincore::builtin_catalog();
    auto operations = base.operations();
    for (auto& operation : operations) {
        if (operation.operation_id !=
                "risk.diagnostics.walk_forward_var" &&
            operation.operation_id !=
                "risk.report.build_risk_validation_report" &&
            operation.operation_id != "report.risk.build_risk_report" &&
            operation.operation_id !=
                "report.portfolio.build_portfolio_report") {
            continue;
        }
        auto original = operation.callable;
        const auto operation_id = operation.operation_id;
        operation.callable = [counts, original = std::move(original),
                              operation_id](
                                 const fincore::runtime_inputs& inputs) {
            ++(*counts)[operation_id];
            return original(inputs);
        };
    }
    return fincore::operation_catalog(std::move(operations));
}

class temporary_directory {
  public:
    temporary_directory() {
        static std::atomic<std::uint64_t> sequence{0U};
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        for (int attempt = 0; attempt < 100; ++attempt) {
            path_ = std::filesystem::temp_directory_path() /
                    ("fincore-runtime-test-" + std::to_string(stamp) + "-" +
                     std::to_string(sequence.fetch_add(1U)));
            if (std::filesystem::create_directory(path_)) {
                path_ = std::filesystem::canonical(path_);
                return;
            }
        }
        throw std::runtime_error{"could not create temporary test directory"};
    }

    ~temporary_directory() {
        std::error_code error;
        std::filesystem::remove_all(path_, error);
    }

    const std::filesystem::path& path() const { return path_; }

  private:
    std::filesystem::path path_;
};

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error{"could not open expected artifact"};
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    return bytes.str();
}

std::string raw_sha256(const std::string& bytes) {
    return fincore::detail::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(bytes.data()), bytes.size()));
}

std::string manifest_file_record(const std::string& manifest,
                                 const std::string& path) {
    const auto path_field = "\"path\": \"" + path + "\"";
    const auto field_position = manifest.find(path_field);
    if (field_position == std::string::npos) {
        return {};
    }
    const auto record_start = manifest.rfind("    {", field_position);
    const auto record_end = manifest.find("\n    }", field_position);
    if (record_start == std::string::npos || record_end == std::string::npos) {
        return {};
    }
    return manifest.substr(record_start, record_end + 6U - record_start);
}

fincore::operation_catalog demo_catalog(std::shared_ptr<int> counter) {
    std::vector<fincore::operation_spec> operations;
    operations.push_back(
        {"demo.double", "demo.double", "demo",
         [counter](const fincore::runtime_inputs& inputs) {
             ++*counter;
             const auto found = inputs.find("value");
             const auto* value =
                 found == inputs.end()
                     ? nullptr
                     : std::get_if<double>(&found->second.data);
             if (value == nullptr) {
                 throw fincore::input_error{"value must be a number"};
             }
             return fincore::runtime_value(*value * 2.0);
         }});
    operations.push_back(
        {"demo.text", "demo.text", "demo",
         [](const fincore::runtime_inputs&) {
             return fincore::runtime_value(std::string("ok"));
         }});
    return fincore::operation_catalog(std::move(operations));
}

TEST(Runtime, RunsPlansAndBatchesInOrder) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    const auto single = fincore::run(
        "demo.double", {{"value", fincore::runtime_value(2.5)}},
        {{"mode", fincore::runtime_value(std::string("test"))}}, catalog);
    const auto* value = std::get_if<double>(&single.value.data);
    ASSERT_NE(value, nullptr);
    EXPECT_DOUBLE_EQ(*value, 5.0);
    EXPECT_EQ(single.metadata.at("operation_id"), "demo.double");
    EXPECT_EQ(single.metadata.at("config_recorded"), "true");
    EXPECT_EQ(single.metadata.at("digest_version"), "2");
    EXPECT_EQ(single.metadata.at("cache"), "disabled");
    EXPECT_EQ(single.metadata.at("input_digest").size(), 64U);
    EXPECT_EQ(single.metadata.at("catalog_digest"), catalog.digest());

    const std::vector<fincore::operation_request> requests{
        {"demo.double", {{"value", fincore::runtime_value(1.0)}}},
        {"demo.text", {}}};
    const auto plan = fincore::plan(requests, catalog);
    EXPECT_EQ(plan.catalog_digest, catalog.digest());
    const auto results = fincore::batch(plan, catalog);
    ASSERT_EQ(results.size(), 2U);
    const auto* first = std::get_if<double>(&results[0].value.data);
    ASSERT_NE(first, nullptr);
    EXPECT_DOUBLE_EQ(*first, 2.0);
    const auto* second = std::get_if<std::string>(&results[1].value.data);
    ASSERT_NE(second, nullptr);
    EXPECT_EQ(*second, "ok");
    EXPECT_EQ(*counter, 2);
}

TEST(Runtime, ValidatesCatalogAndResolution) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    EXPECT_THROW(static_cast<void>(catalog.resolve("missing")),
                 fincore::operation_resolution_error);
    EXPECT_THROW(
        static_cast<void>(fincore::operation_catalog(
            {{"a", "a", "demo", [](const fincore::runtime_inputs&) {
                  return fincore::runtime_value(1.0);
              }},
             {"a", "b", "demo", [](const fincore::runtime_inputs&) {
                  return fincore::runtime_value(1.0);
              }}})),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::operation_catalog(
            {{"", "a", "demo", [](const fincore::runtime_inputs&) {
                  return fincore::runtime_value(1.0);
              }}})),
        fincore::input_error);

    std::make_shared<int>(0);
    const auto other = fincore::operation_catalog(
        {{"other.op", "other.op", "demo",
          [](const fincore::runtime_inputs&) {
              return fincore::runtime_value(1.0);
          }}});
    const std::vector<fincore::operation_request> requests{
        {"demo.double", {{"value", fincore::runtime_value(1.0)}}}};
    const auto execution_plan = fincore::plan(requests, catalog);
    EXPECT_THROW(static_cast<void>(fincore::batch(execution_plan, other)),
                 fincore::input_error);
}

TEST(Runtime, RejectsEmptyOperationInputAndConfigNames) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    EXPECT_THROW(
        static_cast<void>(fincore::operation_request(
            "demo.text", {{"", fincore::runtime_value(1.0)}})),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::run(
            "demo.text", {{"", fincore::runtime_value(1.0)}}, {}, catalog)),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::run(
            "demo.text", {}, {{"", fincore::runtime_value(1.0)}}, catalog)),
        fincore::input_error);
    EXPECT_EQ(*counter, 0);
}

TEST(Runtime, SessionCachesAndClears) {
    auto counter = std::make_shared<int>(0);
    const auto catalog = demo_catalog(counter);
    fincore::analysis_session session(catalog);
    const fincore::runtime_inputs inputs{
        {"value", fincore::runtime_value(3.0)}};
    const auto first = session.run("demo.double", inputs);
    EXPECT_EQ(first.metadata.at("cache"), "miss");
    EXPECT_EQ(*counter, 1);
    const auto second = session.run("demo.double", inputs);
    EXPECT_EQ(second.metadata.at("cache"), "hit");
    EXPECT_EQ(*counter, 1);
    session.clear();
    const auto third = session.run("demo.double", inputs);
    EXPECT_EQ(third.metadata.at("cache"), "miss");
    EXPECT_EQ(*counter, 2);
}

TEST(Runtime, SessionBypassesNondeterministicCacheAndClosesCleanly) {
    auto deterministic_counter = std::make_shared<int>(0);
    auto sequence = std::make_shared<int>(0);
    auto operations = demo_catalog(deterministic_counter).operations();
    operations.push_back(
        {"demo.sequence", "demo.sequence", "demo",
         [sequence](const fincore::runtime_inputs&) {
             return fincore::runtime_value(++*sequence);
         },
         false});
    const fincore::operation_catalog catalog(std::move(operations));
    fincore::analysis_session session(catalog);
    const auto catalog_digest = session.catalog_digest();
    EXPECT_FALSE(session.closed());
    EXPECT_EQ(session.cache_entries(), 0U);

    const auto first_sequence = session.run("demo.sequence", {});
    const auto second_sequence = session.run("demo.sequence", {});
    const auto* first_sequence_value =
        std::get_if<std::int64_t>(&first_sequence.value.data);
    const auto* second_sequence_value =
        std::get_if<std::int64_t>(&second_sequence.value.data);
    EXPECT_EQ(first_sequence.metadata.at("cache"), "disabled");
    EXPECT_EQ(second_sequence.metadata.at("cache"), "disabled");
    ASSERT_NE(first_sequence_value, nullptr);
    ASSERT_NE(second_sequence_value, nullptr);
    EXPECT_EQ(*first_sequence_value, 1);
    EXPECT_EQ(*second_sequence_value, 2);
    EXPECT_EQ(*sequence, 2);
    EXPECT_EQ(session.cache_entries(), 0U);

    const fincore::runtime_inputs deterministic_inputs{
        {"value", fincore::runtime_value(3.0)}};
    const auto deterministic_first =
        session.run("demo.double", deterministic_inputs);
    const auto deterministic_second =
        session.run("demo.double", deterministic_inputs);
    EXPECT_EQ(deterministic_first.metadata.at("cache"), "miss");
    EXPECT_EQ(deterministic_second.metadata.at("cache"), "hit");
    EXPECT_EQ(*deterministic_counter, 1);
    EXPECT_EQ(session.cache_entries(), 1U);

    session.close();
    session.close();
    EXPECT_TRUE(session.closed());
    EXPECT_EQ(session.cache_entries(), 0U);
    EXPECT_EQ(session.catalog_digest(), catalog_digest);
    session.clear();
    EXPECT_TRUE(session.closed());
    EXPECT_THROW(session.run("demo.sequence", {}), std::runtime_error);
    EXPECT_THROW(session.run("demo.double", deterministic_inputs),
                 std::runtime_error);
    EXPECT_EQ(*sequence, 2);
    EXPECT_EQ(*deterministic_counter, 1);
}

TEST(Runtime, SessionOwnsCatalogSnapshotAndAcceptsTemporaryCatalog) {
    auto original_counter = std::make_shared<int>(0);
    auto replacement_counter = std::make_shared<int>(0);
    auto caller_catalog = demo_catalog(original_counter);
    fincore::analysis_session session(caller_catalog);
    caller_catalog = demo_catalog(replacement_counter);

    const auto result = session.run(
        "demo.double", {{"value", fincore::runtime_value(4.0)}});
    const auto* value = std::get_if<double>(&result.value.data);
    ASSERT_NE(value, nullptr);
    EXPECT_DOUBLE_EQ(*value, 8.0);
    EXPECT_EQ(*original_counter, 1);
    EXPECT_EQ(*replacement_counter, 0);

    auto temporary_counter = std::make_shared<int>(0);
    fincore::analysis_session temporary_session(
        demo_catalog(temporary_counter));
    const auto temporary_result = temporary_session.run(
        "demo.double", {{"value", fincore::runtime_value(2.0)}});
    const auto* temporary_value =
        std::get_if<double>(&temporary_result.value.data);
    ASSERT_NE(temporary_value, nullptr);
    EXPECT_DOUBLE_EQ(*temporary_value, 4.0);
    EXPECT_EQ(*temporary_counter, 1);
}

TEST(Runtime, SnapshotsAndValueDigestsAreStable) {
    const auto frame = returns_fixture();
    const fincore::analysis_snapshot first({{"returns", fincore::runtime_value(frame)}});
    const fincore::analysis_snapshot second({{"returns", fincore::runtime_value(frame)}});
    EXPECT_EQ(first.digest(), second.digest());
    EXPECT_EQ(first.digest().size(), 64U);
    const auto changed = fincore::make_series({1, 2, 3}, "returns",
                                              {0.01, 0.02, 0.03});
    EXPECT_NE(fincore::runtime_value_digest(fincore::runtime_value(frame)),
              fincore::runtime_value_digest(fincore::runtime_value(changed)));
    const auto relabeled = fincore::make_series({1, 2, 3}, "renamed",
                                                {0.01, 0.02, -0.01});
    EXPECT_NE(fincore::runtime_value_digest(fincore::runtime_value(frame)),
              fincore::runtime_value_digest(
                  fincore::runtime_value(relabeled)));
    const auto reindexed = fincore::make_series({1, 2, 4}, "returns",
                                                {0.01, 0.02, -0.01});
    EXPECT_NE(fincore::runtime_value_digest(fincore::runtime_value(frame)),
              fincore::runtime_value_digest(
                  fincore::runtime_value(reindexed)));
    EXPECT_NE(fincore::runtime_value_digest(fincore::runtime_value(1)),
              fincore::runtime_value_digest(fincore::runtime_value(1.0)));

    const fincore::analysis_snapshot first_key(
        {{"returns", fincore::runtime_value(frame)}});
    const fincore::analysis_snapshot changed_key(
        {{"renamed", fincore::runtime_value(frame)}});
    EXPECT_NE(first_key.digest(), changed_key.digest());
}

TEST(Runtime, DataFrameDigestsCoverSupportedTypesAndRejectOthers) {
    const auto make_frame = [](const bool flag, const int count,
                               const std::string& label) {
        fincore::dataframe frame;
        frame.load_index(std::vector<fincore::timestamp>{1});
        frame.load_column<bool>("flag", {flag},
                                hmdf::nan_policy::dont_pad_with_nans);
        frame.load_column<int>("count", {count},
                               hmdf::nan_policy::dont_pad_with_nans);
        frame.load_column<std::string>(
            "label", {label}, hmdf::nan_policy::dont_pad_with_nans);
        return frame;
    };
    const auto baseline = make_frame(false, 4, "alpha");
    const auto changed_flag = make_frame(true, 4, "alpha");
    const auto changed_count = make_frame(false, 5, "alpha");
    const auto changed_label = make_frame(false, 4, "beta");
    const auto digest = [](const fincore::dataframe& frame) {
        return fincore::runtime_value_digest(fincore::runtime_value(frame));
    };
    EXPECT_NE(digest(baseline), digest(changed_flag));
    EXPECT_NE(digest(baseline), digest(changed_count));
    EXPECT_NE(digest(baseline), digest(changed_label));

    fincore::dataframe float_baseline;
    float_baseline.load_index(std::vector<fincore::timestamp>{1});
    float_baseline.load_column<float>(
        "score", {1.25F}, hmdf::nan_policy::dont_pad_with_nans);
    auto changed_float = float_baseline;
    changed_float.get_column<float>("score")[0] = 1.5F;
    EXPECT_NE(digest(float_baseline), digest(changed_float));

    fincore::dataframe unsupported;
    unsupported.load_index(std::vector<fincore::timestamp>{1});
    unsupported.load_column<std::complex<double>>(
        "complex", {std::complex<double>{1.0, 2.0}},
        hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_THROW(
        fincore::runtime_value_digest(fincore::runtime_value(unsupported)),
        fincore::input_error);
}

TEST(Runtime, CatalogDigestFramesDelimitersAndRejectsRepeatedModes) {
    const auto callable = [](const fincore::runtime_inputs&) {
        return fincore::runtime_value(1.0);
    };
    const fincore::operation_spec first{
        "digest.operation", "digest.capability", "demo", callable, true,
        "none", std::nullopt, std::nullopt,
        {{"alpha", "beta=gamma"}}};
    const fincore::operation_spec second{
        "digest.operation", "digest.capability", "demo", callable, true,
        "none", std::nullopt, std::nullopt,
        {{"alpha=beta", "gamma"}}};
    EXPECT_NE(fincore::operation_catalog({first}).digest(),
              fincore::operation_catalog({second}).digest());

    const auto mode_operation = [&callable](const std::string& id,
                                            const std::string& mode) {
        return fincore::operation_spec{id,
                                       "shared.capability",
                                       "demo",
                                       callable,
                                       true,
                                       "none",
                                       mode,
                                       "approved",
                                       {}};
    };
    EXPECT_THROW(
        fincore::operation_catalog(
            {mode_operation("mode.a", "A"), mode_operation("mode.b", "B"),
             mode_operation("mode.b-again", "B")}),
        fincore::input_error);
    EXPECT_NO_THROW(static_cast<void>(fincore::operation_catalog(
        {mode_operation("mode.a", "A"), mode_operation("mode.b", "B"),
         mode_operation("mode.c", "C")})));
}

TEST(Runtime, DigestsCoverRiskAndTransactionFields) {
    const auto spec_digest = [](const fincore::risk_model_spec& spec) {
        return fincore::runtime_value_digest(fincore::runtime_value(spec));
    };
    const auto spec = runtime_risk_spec();
    const auto assert_spec_changed = [&](const auto& mutate) {
        auto changed = spec;
        mutate(changed);
        EXPECT_NE(spec_digest(spec), spec_digest(changed));
    };
    assert_spec_changed([](auto& value) {
        value.forecast_target = fincore::risk_forecast_target::es;
    });
    assert_spec_changed([](auto& value) { value.confidence_level += 0.01; });
    assert_spec_changed([](auto& value) { ++value.horizon; });
    assert_spec_changed([](auto& value) { value.distribution = "historical"; });
    assert_spec_changed([](auto& value) {
        value.tail = fincore::risk_tail::upper;
    });
    assert_spec_changed([](auto& value) {
        value.sign_convention = "losses_positive";
    });
    assert_spec_changed([](auto& value) { ++value.window; });
    assert_spec_changed([](auto& value) { ++value.refit_cadence; });
    assert_spec_changed([](auto& value) { value.model_version = "2.0"; });

    const auto backtest_digest = [](const fincore::risk_backtest_result& value) {
        fincore::walk_forward_var_result result{};
        result.backtest = value;
        return fincore::runtime_value_digest(fincore::runtime_value(result));
    };
    const auto backtest = runtime_backtest_result();
    const auto assert_backtest_changed = [&](const auto& mutate) {
        auto changed = backtest;
        mutate(changed);
        EXPECT_NE(backtest_digest(backtest), backtest_digest(changed));
    };
    assert_backtest_changed([](auto& value) {
        value.method = fincore::risk_backtest_method::es;
    });
    assert_backtest_changed([](auto& value) { value.confidence_level += 0.01; });
    assert_backtest_changed([](auto& value) { ++value.observations; });
    assert_backtest_changed([](auto& value) { ++value.exceptions; });
    assert_backtest_changed([](auto& value) { value.expected_exceptions += 1.0; });
    assert_backtest_changed([](auto& value) { value.exception_rate += 0.1; });
    assert_backtest_changed([](auto& value) { value.kupiec_lr += 1.0; });
    assert_backtest_changed([](auto& value) { value.kupiec_pvalue += 0.1; });
    assert_backtest_changed([](auto& value) { value.christoffersen_lr += 1.0; });
    assert_backtest_changed([](auto& value) {
        value.christoffersen_pvalue += 0.1;
    });
    assert_backtest_changed([](auto& value) { value.inputs_digest += "-changed"; });
    assert_backtest_changed([](auto& value) {
        value.diagnostics.clear();
        value.diagnostics.emplace("transition_count_changed", 2.0);
    });
    assert_backtest_changed([](auto& value) {
        value.diagnostics.begin()->second += 1.0;
    });
    assert_backtest_changed([](auto& value) {
        value.status = fincore::risk_backtest_status::fail;
    });

    fincore::walk_forward_var_result walk{};
    walk.spec = spec;
    walk.index = {1, 2};
    walk.forecast = {-0.02, -0.03};
    walk.realized = {-0.01, -0.04};
    walk.refit_timestamps = {1};
    walk.fits = {{1, -0.02, 0.001, 0.01, 5U}};
    walk.inputs_digest = "walk-inputs";
    walk.status = fincore::walk_forward_status::ok;
    walk.unsupported_reason = "reason";
    walk.n_observations = 7U;
    walk.backtest = backtest;
    walk.has_backtest = true;
    const auto walk_digest = [](const fincore::walk_forward_var_result& value) {
        return fincore::runtime_value_digest(fincore::runtime_value(value));
    };
    const auto assert_walk_changed = [&](const auto& mutate) {
        auto changed = walk;
        mutate(changed);
        EXPECT_NE(walk_digest(walk), walk_digest(changed));
    };
    assert_walk_changed([](auto& value) { value.spec.window += 1U; });
    assert_walk_changed([](auto& value) { value.index[0] += 1; });
    assert_walk_changed([](auto& value) { value.forecast[0] -= 0.01; });
    assert_walk_changed([](auto& value) { value.realized[0] -= 0.01; });
    assert_walk_changed([](auto& value) { value.refit_timestamps[0] += 1; });
    assert_walk_changed([](auto& value) { value.fits[0].date += 1; });
    assert_walk_changed([](auto& value) { value.fits[0].forecast -= 0.01; });
    assert_walk_changed([](auto& value) { value.fits[0].mean += 0.01; });
    assert_walk_changed([](auto& value) {
        value.fits[0].standard_deviation += 0.01;
    });
    assert_walk_changed([](auto& value) { ++value.fits[0].n_observations; });
    assert_walk_changed([](auto& value) { value.inputs_digest += "-changed"; });
    assert_walk_changed([](auto& value) {
        value.status = fincore::walk_forward_status::unsupported;
    });
    assert_walk_changed([](auto& value) { value.unsupported_reason += "!"; });
    assert_walk_changed([](auto& value) { ++value.n_observations; });
    assert_walk_changed([](auto& value) {
        value.backtest.diagnostics.clear();
        value.backtest.diagnostics.emplace("group_changed", 2.0);
    });
    assert_walk_changed([](auto& value) { value.has_backtest = false; });

    fincore::risk_validation_report report;
    report.status = fincore::walk_forward_status::ok;
    report.inputs_digest = "report-inputs";
    report.specification = spec;
    report.forecast_events = {{1, -0.02, -0.01, false, true}};
    report.refits = {{1, -0.02, 0.001, 0.01, 5U}};
    report.diagnostics = {"historical", "weibull", 5U, 2U, 7U, "pass", "", 12U};
    report.has_backtest = true;
    report.backtest = backtest;
    report.traffic_light_zone = "green";
    report.timestamp_index_name = "time";
    report.timestamp_timezone = "UTC";
    report.disclosure = "reference";
    report.schema_version = 2;
    const auto report_digest = [](const fincore::risk_validation_report& value) {
        return fincore::runtime_value_digest(fincore::runtime_value(value));
    };
    const auto assert_report_changed = [&](const auto& mutate) {
        auto changed = report;
        mutate(changed);
        EXPECT_NE(report_digest(report), report_digest(changed));
    };
    assert_report_changed([](auto& value) {
        value.status = fincore::walk_forward_status::unsupported;
    });
    assert_report_changed([](auto& value) { value.inputs_digest += "!"; });
    assert_report_changed([](auto& value) { value.specification.window += 1U; });
    assert_report_changed([](auto& value) {
        value.forecast_events[0].date += 1;
    });
    assert_report_changed([](auto& value) {
        value.forecast_events[0].forecast -= 0.01;
    });
    assert_report_changed([](auto& value) {
        value.forecast_events[0].realized -= 0.01;
    });
    assert_report_changed([](auto& value) {
        value.forecast_events[0].exception = true;
    });
    assert_report_changed([](auto& value) { value.forecast_events[0].refit = false; });
    assert_report_changed([](auto& value) { value.refits[0].date += 1; });
    assert_report_changed([](auto& value) { value.refits[0].forecast -= 0.01; });
    assert_report_changed([](auto& value) { value.refits[0].mean += 0.01; });
    assert_report_changed([](auto& value) {
        value.refits[0].standard_deviation += 0.01;
    });
    assert_report_changed([](auto& value) { ++value.refits[0].n_observations; });
    assert_report_changed([](auto& value) { value.diagnostics.method += "!"; });
    assert_report_changed([](auto& value) {
        value.diagnostics.quantile_method += "!";
    });
    assert_report_changed([](auto& value) { ++value.diagnostics.window; });
    assert_report_changed([](auto& value) { ++value.diagnostics.refit_cadence; });
    assert_report_changed([](auto& value) { ++value.diagnostics.forecast_count; });
    assert_report_changed([](auto& value) {
        value.diagnostics.backtest_status += "!";
    });
    assert_report_changed([](auto& value) { value.diagnostics.reason += "!"; });
    assert_report_changed([](auto& value) {
        ++value.diagnostics.n_observations;
    });
    assert_report_changed([](auto& value) { value.has_backtest = false; });
    assert_report_changed([](auto& value) {
        value.backtest.diagnostics.clear();
        value.backtest.diagnostics.emplace("different_group", 2.0);
    });
    assert_report_changed([](auto& value) { value.traffic_light_zone = "red"; });
    assert_report_changed([](auto& value) {
        value.timestamp_index_name = "date";
    });
    assert_report_changed([](auto& value) { value.timestamp_timezone = "local"; });
    assert_report_changed([](auto& value) { value.disclosure += "!"; });
    assert_report_changed([](auto& value) { ++value.schema_version; });

    const auto transactions = portfolio_transactions_fixture();
    const auto transaction_digest = [](const auto& values) {
        return fincore::runtime_value_digest(fincore::runtime_value(values));
    };
    auto changed_transactions = transactions;
    changed_transactions[0].date += 1;
    EXPECT_NE(transaction_digest(transactions),
              transaction_digest(changed_transactions));
    changed_transactions = transactions;
    changed_transactions[0].symbol += "!";
    EXPECT_NE(transaction_digest(transactions),
              transaction_digest(changed_transactions));
    changed_transactions = transactions;
    changed_transactions[0].amount += 1.0;
    EXPECT_NE(transaction_digest(transactions),
              transaction_digest(changed_transactions));
    changed_transactions = transactions;
    changed_transactions[0].price += 1.0;
    EXPECT_NE(transaction_digest(transactions),
              transaction_digest(changed_transactions));
    changed_transactions = transactions;
    changed_transactions.pop_back();
    EXPECT_NE(transaction_digest(transactions),
              transaction_digest(changed_transactions));
}

TEST(Runtime, PortfolioAndRiskPipelinePlansCachesRendersAndWritesArtifacts) {
    const auto counts = std::make_shared<std::map<std::string, int>>();
    const auto catalog = instrumented_builtin_catalog(counts);

    const auto risk_returns = risk_returns_fixture();
    const auto spec = runtime_risk_spec();
    const auto portfolio_returns = portfolio_returns_fixture();
    const auto portfolio_benchmark = portfolio_benchmark_fixture();
    const auto portfolio_positions = portfolio_positions_fixture();
    const auto portfolio_transactions = portfolio_transactions_fixture();
    const auto direct_walk = fincore::walk_forward_var(risk_returns, spec);
    const auto direct_validation = fincore::build_risk_validation_report(
        direct_walk, std::string{"risk_index"});
    const auto direct_risk_document = fincore::build_risk_report(
        direct_validation, "Runtime risk validation");
    const fincore::runtime_inputs walk_inputs{
        {"returns", risk_returns}, {"spec", spec}};
    fincore::runtime_inputs portfolio_inputs{
        {"returns", portfolio_returns},
        {"benchmark", portfolio_benchmark},
        {"positions", portfolio_positions},
        {"transactions", portfolio_transactions},
        {"rolling_window", 3},
        {"title", std::string{"Runtime portfolio"}}};
    const auto direct_portfolio_document = fincore::build_portfolio_report(
        portfolio_returns, &portfolio_benchmark, "Runtime portfolio",
        &portfolio_positions, 3U, portfolio_transactions);

    const std::vector<fincore::operation_request> requests{
        {"risk.diagnostics.walk_forward_var", walk_inputs,
         {{"provenance_only", std::string{"do not forward"}}}},
        {"report.portfolio.build_portfolio_report", portfolio_inputs,
         {{"provenance_only", std::string{"do not forward"}}}}};
    const auto execution_plan = fincore::plan(requests, catalog);
    const auto results = fincore::batch(execution_plan, catalog);
    ASSERT_EQ(results.size(), 2U);
    EXPECT_EQ(results[0].metadata.at("config_recorded"), "true");
    EXPECT_EQ(fincore::runtime_value_digest(results[0].value),
              fincore::runtime_value_digest(
                  fincore::runtime_value(direct_walk)));
    const auto* portfolio_value =
        std::get_if<fincore::report_document>(&results[1].value.data);
    ASSERT_NE(portfolio_value, nullptr);
    EXPECT_EQ(portfolio_value->to_json(), direct_portfolio_document.to_json());

    const auto validation_result = fincore::run(
        "risk.report.build_risk_validation_report",
        {{"result", results[0].value},
         {"index_name", std::string{"risk_index"}}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(validation_result.value),
              fincore::runtime_value_digest(
                  fincore::runtime_value(direct_validation)));
    const auto risk_document_result = fincore::run(
        "report.risk.build_risk_report",
        {{"report", validation_result.value},
         {"title", std::string{"Runtime risk validation"}}},
        {}, catalog);
    const auto* risk_document =
        std::get_if<fincore::report_document>(&risk_document_result.value.data);
    ASSERT_NE(risk_document, nullptr);
    EXPECT_EQ(risk_document->to_json(), direct_risk_document.to_json());

    EXPECT_EQ(counts->at("risk.diagnostics.walk_forward_var"), 1);
    EXPECT_EQ(counts->at("risk.report.build_risk_validation_report"), 1);
    EXPECT_EQ(counts->at("report.risk.build_risk_report"), 1);
    EXPECT_EQ(counts->at("report.portfolio.build_portfolio_report"), 1);

    fincore::analysis_session session(catalog);
    const auto walk_first = session.run(
        "risk.diagnostics.walk_forward_var", walk_inputs);
    const auto walk_second = session.run(
        "risk.diagnostics.walk_forward_var", walk_inputs);
    EXPECT_EQ(walk_first.metadata.at("cache"), "miss");
    EXPECT_EQ(walk_second.metadata.at("cache"), "hit");
    EXPECT_EQ((*counts)["risk.diagnostics.walk_forward_var"], 2);

    const fincore::runtime_inputs validation_inputs{
        {"result", walk_first.value}, {"index_name", std::string{"risk_index"}}};
    const auto validation_first = session.run(
        "risk.report.build_risk_validation_report", validation_inputs);
    const auto validation_second = session.run(
        "risk.report.build_risk_validation_report", validation_inputs);
    EXPECT_EQ(validation_first.metadata.at("cache"), "miss");
    EXPECT_EQ(validation_second.metadata.at("cache"), "hit");
    EXPECT_EQ((*counts)["risk.report.build_risk_validation_report"], 2);

    const fincore::runtime_inputs risk_document_inputs{
        {"report", validation_first.value},
        {"title", std::string{"Runtime risk validation"}}};
    auto risk_document_first = session.run(
        "report.risk.build_risk_report", risk_document_inputs);
    const auto risk_document_second = session.run(
        "report.risk.build_risk_report", risk_document_inputs);
    EXPECT_EQ(risk_document_first.metadata.at("cache"), "miss");
    EXPECT_EQ(risk_document_second.metadata.at("cache"), "hit");
    EXPECT_EQ((*counts)["report.risk.build_risk_report"], 2);

    const auto original_risk_document_digest =
        fincore::runtime_value_digest(risk_document_second.value);
    auto* mutable_risk_document = std::get_if<fincore::report_document>(
        &risk_document_first.value.data);
    ASSERT_NE(mutable_risk_document, nullptr);
    mutable_risk_document->title = "mutated caller risk result";
    const auto restored_risk_document = session.run(
        "report.risk.build_risk_report", risk_document_inputs);
    EXPECT_EQ(restored_risk_document.metadata.at("cache"), "hit");
    EXPECT_EQ(fincore::runtime_value_digest(risk_document_second.value),
              original_risk_document_digest);
    EXPECT_EQ(fincore::runtime_value_digest(restored_risk_document.value),
              original_risk_document_digest);

    auto changed_walk_inputs = walk_inputs;
    changed_walk_inputs["returns"] = risk_returns_fixture(12, 0.025);
    const auto changed_walk_first = session.run(
        "risk.diagnostics.walk_forward_var", changed_walk_inputs);
    const auto changed_walk_second = session.run(
        "risk.diagnostics.walk_forward_var", changed_walk_inputs);
    EXPECT_EQ(changed_walk_first.metadata.at("cache"), "miss");
    EXPECT_EQ(changed_walk_second.metadata.at("cache"), "hit");
    EXPECT_NE(changed_walk_first.metadata.at("input_digest"),
              walk_first.metadata.at("input_digest"));
    EXPECT_NE(fincore::runtime_value_digest(changed_walk_first.value),
              fincore::runtime_value_digest(walk_first.value));
    EXPECT_EQ(fincore::runtime_value_digest(changed_walk_second.value),
              fincore::runtime_value_digest(changed_walk_first.value));

    const fincore::runtime_inputs changed_validation_inputs{
        {"result", changed_walk_first.value},
        {"index_name", std::string{"risk_index"}}};
    const auto changed_validation_first = session.run(
        "risk.report.build_risk_validation_report", changed_validation_inputs);
    const auto changed_validation_second = session.run(
        "risk.report.build_risk_validation_report", changed_validation_inputs);
    EXPECT_EQ(changed_validation_first.metadata.at("cache"), "miss");
    EXPECT_EQ(changed_validation_second.metadata.at("cache"), "hit");
    EXPECT_NE(changed_validation_first.metadata.at("input_digest"),
              validation_first.metadata.at("input_digest"));
    EXPECT_NE(fincore::runtime_value_digest(changed_validation_first.value),
              fincore::runtime_value_digest(validation_first.value));
    EXPECT_EQ(fincore::runtime_value_digest(changed_validation_second.value),
              fincore::runtime_value_digest(changed_validation_first.value));

    const fincore::runtime_inputs changed_risk_document_inputs{
        {"report", changed_validation_first.value},
        {"title", std::string{"Runtime risk validation"}}};
    const auto changed_risk_document_first = session.run(
        "report.risk.build_risk_report", changed_risk_document_inputs);
    const auto changed_risk_document_second = session.run(
        "report.risk.build_risk_report", changed_risk_document_inputs);
    EXPECT_EQ(changed_risk_document_first.metadata.at("cache"), "miss");
    EXPECT_EQ(changed_risk_document_second.metadata.at("cache"), "hit");
    EXPECT_NE(changed_risk_document_first.metadata.at("input_digest"),
              risk_document_first.metadata.at("input_digest"));
    EXPECT_NE(fincore::runtime_value_digest(changed_risk_document_first.value),
              original_risk_document_digest);
    EXPECT_EQ(fincore::runtime_value_digest(changed_risk_document_second.value),
              fincore::runtime_value_digest(changed_risk_document_first.value));
    EXPECT_EQ(fincore::runtime_value_digest(risk_document_second.value),
              original_risk_document_digest);
    EXPECT_EQ((*counts)["risk.diagnostics.walk_forward_var"], 3);
    EXPECT_EQ((*counts)["risk.report.build_risk_validation_report"], 3);
    EXPECT_EQ((*counts)["report.risk.build_risk_report"], 3);

    auto portfolio_first = session.run(
        "report.portfolio.build_portfolio_report", portfolio_inputs);
    auto* mutable_cached_document =
        std::get_if<fincore::report_document>(&portfolio_first.value.data);
    ASSERT_NE(mutable_cached_document, nullptr);
    mutable_cached_document->title = "mutated caller result";
    const auto portfolio_second = session.run(
        "report.portfolio.build_portfolio_report", portfolio_inputs);
    EXPECT_EQ(portfolio_first.metadata.at("cache"), "miss");
    EXPECT_EQ(portfolio_second.metadata.at("cache"), "hit");
    const auto* independent_cached_document =
        std::get_if<fincore::report_document>(&portfolio_second.value.data);
    ASSERT_NE(independent_cached_document, nullptr);
    EXPECT_EQ(independent_cached_document->title, "Runtime portfolio");
    EXPECT_EQ((*counts)["report.portfolio.build_portfolio_report"], 2);

    portfolio_inputs["title"] = std::string{"Changed portfolio"};
    const auto changed_portfolio = session.run(
        "report.portfolio.build_portfolio_report", portfolio_inputs);
    EXPECT_EQ(changed_portfolio.metadata.at("cache"), "miss");
    EXPECT_EQ((*counts)["report.portfolio.build_portfolio_report"], 3);
    const auto* changed_document =
        std::get_if<fincore::report_document>(&changed_portfolio.value.data);
    ASSERT_NE(changed_document, nullptr);
    EXPECT_EQ(changed_document->title, "Changed portfolio");
    EXPECT_EQ(independent_cached_document->title, "Runtime portfolio");
    portfolio_inputs["title"] = std::string{"Runtime portfolio"};
    const auto restored_portfolio = session.run(
        "report.portfolio.build_portfolio_report", portfolio_inputs);
    EXPECT_EQ(restored_portfolio.metadata.at("cache"), "hit");
    EXPECT_EQ((*counts)["report.portfolio.build_portfolio_report"], 3);

    const auto* cached_risk_document = std::get_if<fincore::report_document>(
        &risk_document_second.value.data);
    ASSERT_NE(cached_risk_document, nullptr);
    const auto portfolio_html = fincore::render_html(*independent_cached_document);
    const auto portfolio_xlsx = fincore::render_xlsx(*independent_cached_document);
    const auto risk_html = fincore::render_html(*cached_risk_document);
    const auto risk_xlsx = fincore::render_xlsx(*cached_risk_document);
    EXPECT_EQ(portfolio_html, fincore::render_html(direct_portfolio_document));
    EXPECT_EQ(portfolio_xlsx, fincore::render_xlsx(direct_portfolio_document));
    EXPECT_EQ(risk_html, fincore::render_html(direct_risk_document));
    EXPECT_EQ(risk_xlsx, fincore::render_xlsx(direct_risk_document));
    EXPECT_EQ((*counts)["risk.diagnostics.walk_forward_var"], 3);
    EXPECT_EQ((*counts)["risk.report.build_risk_validation_report"], 3);
    EXPECT_EQ((*counts)["report.risk.build_risk_report"], 3);
    EXPECT_EQ((*counts)["report.portfolio.build_portfolio_report"], 3);

    fincore::artifact_bundle bundle;
    bundle.artifacts["portfolio/report.html"] = portfolio_html;
    bundle.artifacts["portfolio/report.xlsx"] = portfolio_xlsx;
    bundle.artifacts["risk/report.html"] = risk_html;
    bundle.artifacts["risk/report.xlsx"] = risk_xlsx;
    bundle.metadata["catalog_digest"] = catalog.digest();
    const auto portfolio_provenance = std::map<std::string, std::string>{
        {"catalog_digest", catalog.digest()},
        {"input_digest", portfolio_second.metadata.at("input_digest")},
        {"operation_id", "report.portfolio.build_portfolio_report"}};
    const auto risk_provenance = std::map<std::string, std::string>{
        {"catalog_digest", catalog.digest()},
        {"input_digest", risk_document_second.metadata.at("input_digest")},
        {"operation_id", "report.risk.build_risk_report"}};
    bundle.descriptors["portfolio/report.html"] =
        {"text/html", portfolio_provenance};
    bundle.descriptors["portfolio/report.xlsx"] =
        {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
         portfolio_provenance};
    bundle.descriptors["risk/report.html"] =
        {"text/html", risk_provenance};
    bundle.descriptors["risk/report.xlsx"] =
        {"application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
         risk_provenance};
    temporary_directory temporary;
    const auto output = temporary.path() / "artifacts";
    fincore::write_artifact_bundle(bundle, output);
    EXPECT_EQ(read_bytes(output / "portfolio/report.html"), portfolio_html);
    EXPECT_EQ(read_bytes(output / "portfolio/report.xlsx"), portfolio_xlsx);
    EXPECT_EQ(read_bytes(output / "risk/report.html"), risk_html);
    EXPECT_EQ(read_bytes(output / "risk/report.xlsx"), risk_xlsx);
    const auto manifest = read_bytes(output / ".fincore-manifest.json");
    EXPECT_NE(manifest.find("\"schema_version\": 2"), std::string::npos);
    EXPECT_NE(manifest.find("catalog_digest"), std::string::npos);
    EXPECT_NE(manifest.find("portfolio/report.xlsx"), std::string::npos);
    EXPECT_NE(manifest.find("risk/report.html"), std::string::npos);

    const auto assert_manifest_file = [&](const std::string& path,
                                          const std::string& media_type,
                                          const std::string& bytes,
                                          const std::string& input_digest) {
        const auto record = manifest_file_record(manifest, path);
        if (record.empty()) {
            ADD_FAILURE() << "manifest omitted descriptor for " << path;
            return;
        }
        const auto persisted_bytes = read_bytes(output / path);
        EXPECT_EQ(persisted_bytes, bytes);
        EXPECT_NE(record.find("\"media_type\": \"" + media_type + "\""),
                  std::string::npos);
        EXPECT_NE(record.find("\"size_bytes\": " +
                              std::to_string(persisted_bytes.size())),
                  std::string::npos);
        EXPECT_NE(record.find("\"sha256\": \"" +
                              raw_sha256(persisted_bytes) + "\""),
                  std::string::npos);
        EXPECT_NE(record.find("\"catalog_digest\": \"" + catalog.digest() +
                              "\""),
                  std::string::npos);
        EXPECT_NE(record.find("\"input_digest\": \"" + input_digest + "\""),
                  std::string::npos);
    };
    assert_manifest_file(
        "portfolio/report.html", "text/html", portfolio_html,
        portfolio_second.metadata.at("input_digest"));
    assert_manifest_file(
        "portfolio/report.xlsx",
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        portfolio_xlsx, portfolio_second.metadata.at("input_digest"));
    assert_manifest_file(
        "risk/report.html", "text/html", risk_html,
        risk_document_second.metadata.at("input_digest"));
    assert_manifest_file(
        "risk/report.xlsx",
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        risk_xlsx, risk_document_second.metadata.at("input_digest"));
    EXPECT_NE(portfolio_second.metadata.at("input_digest"),
              risk_document_second.metadata.at("input_digest"));
    EXPECT_NE(risk_document_second.metadata.at("input_digest"),
              results[0].metadata.at("input_digest"));

    const auto repeat_output = temporary.path() / "artifacts-repeat";
    fincore::write_artifact_bundle(bundle, repeat_output);
    EXPECT_EQ(read_bytes(repeat_output / ".fincore-manifest.json"), manifest);
}

TEST(Runtime, ArtifactManifestV2DefaultsAndEscapesDescriptorMetadata) {
    temporary_directory temporary;
    fincore::artifact_bundle legacy{
        {{"legacy.txt", "abc"}}, {{"batch", "legacy"}}};
    fincore::write_artifact_bundle(legacy, temporary.path() / "legacy");
    const auto legacy_manifest =
        read_bytes(temporary.path() / "legacy" / ".fincore-manifest.json");
    const auto legacy_record =
        manifest_file_record(legacy_manifest, "legacy.txt");
    EXPECT_NE(legacy_manifest.find("\"schema_version\": 2"),
              std::string::npos);
    EXPECT_NE(legacy_record.find("\"media_type\": \"application/octet-stream\""),
              std::string::npos);
    EXPECT_NE(legacy_record.find("\"size_bytes\": 3"), std::string::npos);
    EXPECT_NE(legacy_record.find(
                  "\"sha256\": "
                  "\"ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad\""),
              std::string::npos);
    EXPECT_NE(legacy_record.find("\"provenance\": {}"), std::string::npos);

    fincore::artifact_descriptor descriptor{
        "text/plain", {{"note\"field", "quote \" slash \\ line\nnext"}}};
    fincore::artifact_bundle owned;
    owned.artifacts["note.txt"] = "payload";
    owned.descriptors["note.txt"] = descriptor;
    descriptor.media_type = "application/changed";
    descriptor.provenance["note\"field"] = "changed";
    fincore::write_artifact_bundle(owned, temporary.path() / "owned");
    const auto owned_manifest =
        read_bytes(temporary.path() / "owned" / ".fincore-manifest.json");
    const auto owned_record = manifest_file_record(owned_manifest, "note.txt");
    EXPECT_NE(owned_record.find("\"media_type\": \"text/plain\""),
              std::string::npos);
    EXPECT_NE(owned_record.find("\"note\\\"field\""), std::string::npos);
    EXPECT_NE(owned_record.find("\"quote \\\" slash \\\\ line\\nnext\""),
              std::string::npos);
    EXPECT_EQ(owned_record.find("application/changed"), std::string::npos);
    EXPECT_EQ(owned_record.find("\"changed\""), std::string::npos);
}

TEST(Runtime, ArtifactBundleRejectsUnsafePathsAndCleansPartialWrites) {
    temporary_directory temporary;
    fincore::artifact_bundle unsafe;
    unsafe.artifacts["../escape.txt"] = "escape";
    const auto unsafe_output = temporary.path() / "unsafe-output";
    EXPECT_THROW(fincore::write_artifact_bundle(unsafe, unsafe_output),
                 fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(unsafe_output));
    unsafe.artifacts.clear();
    unsafe.artifacts[""] = "empty key";
    EXPECT_THROW(fincore::write_artifact_bundle(unsafe, unsafe_output),
                 fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(unsafe_output));

    unsafe.artifacts.clear();
    unsafe.artifacts["report.txt"] = "report";
    unsafe.descriptors["missing.txt"] = {"text/plain", {}};
    EXPECT_THROW(fincore::write_artifact_bundle(unsafe, unsafe_output),
                 fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(unsafe_output));
    unsafe.descriptors.clear();
    unsafe.descriptors["report.txt"] = {"", {}};
    EXPECT_THROW(fincore::write_artifact_bundle(unsafe, unsafe_output),
                 fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(unsafe_output));
    unsafe.descriptors["report.txt"] = {"text/plain\r\nX-Injected: yes", {}};
    EXPECT_THROW(fincore::write_artifact_bundle(unsafe, unsafe_output),
                 fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(unsafe_output));
    unsafe.descriptors["report.txt"] = {"text/plain/extra", {}};
    EXPECT_THROW(fincore::write_artifact_bundle(unsafe, unsafe_output),
                 fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(unsafe_output));

    const auto existing = temporary.path() / "existing";
    ASSERT_TRUE(std::filesystem::create_directory(existing));
    {
        std::ofstream sentinel(existing / "sentinel.txt", std::ios::binary);
        ASSERT_TRUE(sentinel.is_open());
        sentinel << "preserve";
    }
    fincore::artifact_bundle valid;
    valid.artifacts["report.html"] = "<html/>";
    EXPECT_THROW(fincore::write_artifact_bundle(valid, existing),
                 fincore::input_error);
    EXPECT_EQ(read_bytes(existing / "sentinel.txt"), "preserve");

    const auto partial = temporary.path() / "partial";
    fincore::artifact_bundle too_long;
    too_long.artifacts["a-valid.txt"] = "written first";
    too_long.artifacts["z" + std::string(300U, 'x')] = "cannot be written";
    EXPECT_THROW(fincore::write_artifact_bundle(too_long, partial),
                 std::exception);
    EXPECT_FALSE(std::filesystem::exists(partial));

    const auto real_parent = temporary.path() / "real-parent";
    ASSERT_TRUE(std::filesystem::create_directory(real_parent));
    const auto symlink_parent = temporary.path() / "symlink-parent";
#if defined(_WIN32)
    // Directory symlinks need Developer Mode; a junction is the unprivileged
    // reparse-point equivalent and must be rejected just the same.
    const auto junction = "cmd /c mklink /J \"" +
                          symlink_parent.string() + "\" \"" +
                          real_parent.string() + "\" >nul 2>&1";
    ASSERT_EQ(std::system(junction.c_str()), 0)
        << "cannot create a directory junction";
#else
    std::filesystem::create_directory_symlink(real_parent, symlink_parent);
#endif
    EXPECT_THROW(
        fincore::write_artifact_bundle(valid, symlink_parent / "output"),
        fincore::input_error);
    EXPECT_FALSE(std::filesystem::exists(real_parent / "output"));
}

TEST(Runtime, RejectsMalformedBuiltinInputsAndNegativeCounts) {
    const auto catalog = fincore::builtin_catalog();
    const auto frame = portfolio_returns_fixture();
    EXPECT_THROW(
        fincore::run("risk.diagnostics.walk_forward_var",
                     {{"returns", frame}}, {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.diagnostics.walk_forward_var",
                     {{"returns", fincore::runtime_value(1.0)},
                      {"spec", runtime_risk_spec()}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.diagnostics.walk_forward_var",
                     {{"returns", frame}, {"spec", std::string{"bad"}}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.report.build_risk_validation_report",
                     {{"result", frame}}, {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.report.build_risk_validation_report",
                     {{"result", fincore::walk_forward_var_result{}},
                      {"index_name", fincore::runtime_value(1)}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.risk.build_risk_report",
                     {{"report", fincore::walk_forward_var_result{}}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.risk.build_risk_report",
                     {{"report", fincore::risk_validation_report{}},
                      {"title", fincore::runtime_value(1)}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.portfolio.build_portfolio_report",
                     {{"returns", fincore::runtime_value(1.0)}}, {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.portfolio.build_portfolio_report",
                     {{"returns", frame}, {"benchmark", std::string{"bad"}}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.portfolio.build_portfolio_report",
                     {{"returns", frame}, {"positions", std::string{"bad"}}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.portfolio.build_portfolio_report",
                     {{"returns", frame}, {"transactions", frame}}, {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.portfolio.build_portfolio_report",
                     {{"returns", frame}, {"rolling_window", -1}}, {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("report.portfolio.build_portfolio_report",
                     {{"returns", frame}, {"title", fincore::runtime_value(1)}},
                     {}, catalog),
        fincore::input_error);

    EXPECT_THROW(
        fincore::run("risk.backtesting.kupiec_lr",
                     {{"observations", -1}, {"exceptions", 0},
                      {"confidence_level", 0.99}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.backtesting.kupiec_lr",
                     {{"observations", 10}, {"exceptions", -1},
                      {"confidence_level", 0.99}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.calibration.basel_traffic_light",
                     {{"exceptions", -1}, {"observations", 10},
                      {"confidence_level", 0.99}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("risk.calibration.expected_exception_count",
                     {{"observations", -1}, {"confidence_level", 0.99}},
                     {}, catalog),
        fincore::input_error);
    EXPECT_THROW(
        fincore::run("metrics.rolling.roll_sharpe_ratio",
                     {{"returns", frame}, {"window", -1}}, {}, catalog),
        fincore::input_error);
}

TEST(Runtime, BuiltinCatalogRunsAcceptedOperations) {
    const auto catalog = fincore::builtin_catalog();
    EXPECT_GE(catalog.operations().size(), 48U);
    EXPECT_EQ(catalog.operation_ids().size(), catalog.operations().size());
    const std::vector<std::string> expected_ids{
        "metrics.alpha_beta.alpha",
        "metrics.alpha_beta.beta",
        "metrics.drawdown.max_drawdown",
        "metrics.drawdown.max_drawdown_days",
        "metrics.drawdown.max_drawdown_months",
        "metrics.drawdown.max_drawdown_weeks",
        "metrics.ratios.adjusted_sharpe_ratio",
        "metrics.ratios.burke_ratio",
        "metrics.ratios.calmar_ratio",
        "metrics.ratios.conditional_sharpe_ratio",
        "metrics.ratios.information_ratio",
        "metrics.ratios.m_squared",
        "metrics.ratios.mar_ratio",
        "metrics.ratios.omega_ratio",
        "metrics.ratios.sharpe_ratio",
        "metrics.ratios.sortino_ratio",
        "metrics.ratios.sterling_ratio",
        "metrics.ratios.up_down_capture",
        "metrics.returns.cum_returns",
        "metrics.returns.cum_returns_final",
        "metrics.risk.annual_volatility",
        "metrics.risk.beta_fragility_heuristic",
        "metrics.risk.conditional_value_at_risk",
        "metrics.risk.downside_risk",
        "metrics.risk.residual_risk",
        "metrics.risk.tail_ratio",
        "metrics.risk.tracking_error",
        "metrics.rolling.roll_annual_volatility",
        "metrics.rolling.roll_beta",
        "metrics.rolling.roll_max_drawdown",
        "metrics.rolling.roll_sharpe_ratio",
        "metrics.rolling.roll_sortino_ratio",
        "metrics.rolling.rolling_beta",
        "metrics.stats.hurst_exponent",
        "metrics.stats.kurtosis",
        "metrics.stats.loss_rate",
        "metrics.stats.r_cubed_turtle",
        "metrics.stats.skewness",
        "metrics.stats.stutzer_index",
        "metrics.stats.win_rate",
        "metrics.yearly.annual_return",
        "report.portfolio.build_portfolio_report",
        "report.risk.build_risk_report",
        "risk.backtesting.kupiec_lr",
        "risk.calibration.basel_traffic_light",
        "risk.calibration.expected_exception_count",
        "risk.diagnostics.walk_forward_var",
        "risk.report.build_risk_validation_report"};
    const auto registered_ids = catalog.operation_ids();
    for (const auto& expected : expected_ids) {
        EXPECT_NE(std::find(registered_ids.begin(), registered_ids.end(),
                            expected),
                  registered_ids.end())
            << expected;
    }
    const auto frame = returns_fixture();
    const auto benchmark = benchmark_fixture();
    const fincore::runtime_inputs pair{{"returns", frame},
                                       {"benchmark", benchmark}};
    const auto single = [&catalog, &frame](const std::string& id,
                                           const double expected) {
        const auto result =
            fincore::run(id, {{"returns", fincore::runtime_value(frame)}},
                         {}, catalog);
        const auto* value = std::get_if<double>(&result.value.data);
        EXPECT_NE(value, nullptr);
        if (value != nullptr && std::isnan(expected)) {
            EXPECT_TRUE(std::isnan(*value)) << id;
        } else if (value != nullptr) {
            EXPECT_DOUBLE_EQ(*value, expected) << id;
        }
    };
    single("metrics.ratios.sharpe_ratio", fincore::sharpe_ratio(frame));
    single("metrics.ratios.sortino_ratio", fincore::sortino_ratio(frame));
    single("metrics.ratios.calmar_ratio", fincore::calmar_ratio(frame));
    single("metrics.ratios.mar_ratio", fincore::mar_ratio(frame));
    single("metrics.ratios.omega_ratio", fincore::omega_ratio(frame));
    single("metrics.drawdown.max_drawdown", fincore::max_drawdown(frame));
    single("metrics.risk.annual_volatility",
           fincore::annual_volatility(frame));
    single("metrics.risk.tail_ratio", fincore::tail_ratio(frame));
    single("metrics.risk.conditional_value_at_risk",
           fincore::conditional_value_at_risk(frame));
    single("metrics.yearly.annual_return", fincore::annual_return(frame));
    single("metrics.returns.cum_returns_final",
           fincore::cumulative_return(frame));
    single("metrics.ratios.adjusted_sharpe_ratio",
           fincore::adjusted_sharpe_ratio(frame));
    single("metrics.ratios.burke_ratio", fincore::burke_ratio(frame));
    single("metrics.ratios.conditional_sharpe_ratio",
           fincore::conditional_sharpe_ratio(frame));
    single("metrics.ratios.sterling_ratio", fincore::sterling_ratio(frame));
    single("metrics.drawdown.max_drawdown_days",
           fincore::max_drawdown_days(frame));
    single("metrics.drawdown.max_drawdown_weeks",
           fincore::max_drawdown_weeks(frame));
    single("metrics.drawdown.max_drawdown_months",
           fincore::max_drawdown_months(frame));
    single("metrics.risk.downside_risk", fincore::downside_risk(frame));
    single("metrics.stats.skewness", fincore::skewness(frame));
    single("metrics.stats.kurtosis", fincore::kurtosis(frame));
    single("metrics.stats.win_rate", fincore::win_rate(frame));
    single("metrics.stats.loss_rate", fincore::loss_rate(frame));
    single("metrics.stats.hurst_exponent",
           fincore::hurst_exponent(frame));
    single("metrics.stats.stutzer_index", fincore::stutzer_index(frame));
    single("metrics.stats.r_cubed_turtle", fincore::r_cubed_turtle(frame));

    const auto paired = [&catalog, &pair](const std::string& id,
                                          const double expected) {
        const auto result = fincore::run(id, pair, {}, catalog);
        const auto* value = std::get_if<double>(&result.value.data);
        EXPECT_NE(value, nullptr);
        if (value != nullptr && std::isnan(expected)) {
            EXPECT_TRUE(std::isnan(*value)) << id;
        } else if (value != nullptr) {
            EXPECT_DOUBLE_EQ(*value, expected) << id;
        }
    };
    paired("metrics.ratios.information_ratio",
           fincore::information_ratio(frame, benchmark));
    paired("metrics.ratios.up_down_capture",
           fincore::up_down_capture(frame, benchmark));
    paired("metrics.risk.tracking_error",
           fincore::tracking_error(frame, benchmark));
    paired("metrics.alpha_beta.alpha",
           fincore::alpha(frame, benchmark));
    paired("metrics.alpha_beta.beta", fincore::beta(frame, benchmark));
    paired("metrics.ratios.m_squared",
           fincore::m_squared(frame, benchmark));
    paired("metrics.risk.residual_risk",
           fincore::residual_risk(frame, benchmark));
    paired("metrics.risk.beta_fragility_heuristic",
           fincore::beta_fragility_heuristic(frame, benchmark));

    const auto cumulative = fincore::run(
        "metrics.returns.cum_returns",
        {{"returns", fincore::runtime_value(frame)}}, {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(cumulative.value),
              fincore::runtime_value_digest(
                  fincore::runtime_value(fincore::cumulative_returns(frame))));

    const auto kupiec = fincore::run(
        "risk.backtesting.kupiec_lr",
        {{"observations", fincore::runtime_value(250)},
         {"exceptions", fincore::runtime_value(0)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* lr = std::get_if<double>(&kupiec.value.data);
    ASSERT_NE(lr, nullptr);
    EXPECT_NEAR(*lr, 5.025167926750753, 1e-12);

    const auto rolling = fincore::run(
        "metrics.rolling.roll_sharpe_ratio",
        {{"returns", fincore::runtime_value(frame)},
         {"window", fincore::runtime_value(3)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(rolling.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::roll_sharpe_ratio(frame, 3))));
    const auto volatility = fincore::run(
        "metrics.rolling.roll_annual_volatility",
        {{"returns", fincore::runtime_value(frame)},
         {"window", fincore::runtime_value(2)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(volatility.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::roll_annual_volatility(frame, 2))));
    const auto rolling_beta_result = fincore::run(
        "metrics.rolling.rolling_beta",
        {{"returns", fincore::runtime_value(frame)},
         {"benchmark", fincore::runtime_value(benchmark)},
         {"rolling_window", fincore::runtime_value(2)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(rolling_beta_result.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::rolling_beta(frame, benchmark, 2))));
    const auto roll_beta_result = fincore::run(
        "metrics.rolling.roll_beta",
        {{"returns", fincore::runtime_value(frame)},
         {"benchmark", fincore::runtime_value(benchmark)},
         {"window", fincore::runtime_value(2)}},
        {}, catalog);
    EXPECT_EQ(fincore::runtime_value_digest(roll_beta_result.value),
              fincore::runtime_value_digest(fincore::runtime_value(
                  fincore::roll_beta(frame, benchmark, 2))));

    const auto expected = fincore::run(
        "risk.calibration.expected_exception_count",
        {{"observations", fincore::runtime_value(250)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* count = std::get_if<double>(&expected.value.data);
    ASSERT_NE(count, nullptr);
    EXPECT_DOUBLE_EQ(*count, fincore::expected_exception_count(250, 0.99));
    const auto basel = fincore::run(
        "risk.calibration.basel_traffic_light",
        {{"exceptions", fincore::runtime_value(0)},
         {"observations", fincore::runtime_value(250)},
         {"confidence_level", fincore::runtime_value(0.99)}},
        {}, catalog);
    const auto* zone = std::get_if<std::string>(&basel.value.data);
    ASSERT_NE(zone, nullptr);
    EXPECT_EQ(*zone, "green");

    EXPECT_THROW(
        static_cast<void>(fincore::run(
            "metrics.sharpe_ratio",
            {{"returns", fincore::runtime_value(frame)}}, {}, catalog)),
        fincore::operation_resolution_error);
    EXPECT_THROW(
        static_cast<void>(fincore::run(
            "metrics.ratios.information_ratio",
            {{"returns", fincore::runtime_value(frame)}}, {}, catalog)),
        fincore::input_error);

    fincore::artifact_bundle bundle;
    bundle.artifacts["report.html"] = "<html></html>";
    bundle.metadata["schema_version"] = "1";
    EXPECT_EQ(bundle.artifacts.at("report.html"), "<html></html>");
}

}  // namespace
