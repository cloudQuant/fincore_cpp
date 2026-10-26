#include <fincore/runtime/operations_portfolio.hpp>

#include <fincore/dataframe.hpp>
#include <fincore/runtime/runtime.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <stdexcept>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace {

using fincore::dataframe;
using fincore::operation_catalog;
using fincore::operation_spec;
using fincore::runtime_inputs;
using fincore::runtime_value;
using fincore::timestamp;

constexpr timestamp day = 86'400'000'000'000LL;

dataframe positions_fixture() {
    dataframe positions;
    positions.load_index(std::vector<timestamp>{
        fincore::utc_timestamp(2022, 1, 3),
        fincore::utc_timestamp(2022, 1, 4),
        fincore::utc_timestamp(2022, 1, 5)});
    positions.load_column<double>(
        "A", {100.0, 100.0, 90.0}, hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "B", {-40.0, -30.0, -20.0},
        hmdf::nan_policy::dont_pad_with_nans);
    positions.load_column<double>(
        "cash", {10.0, 20.0, 25.0},
        hmdf::nan_policy::dont_pad_with_nans);
    return positions;
}

dataframe returns_fixture(std::size_t count, const double base,
                          const double step) {
    std::vector<timestamp> index;
    std::vector<double> values;
    index.reserve(count);
    values.reserve(count);
    for (std::size_t position = 0U; position < count; ++position) {
        index.push_back(static_cast<timestamp>(position));
        values.push_back(base + step * static_cast<double>(position));
    }
    return fincore::make_series(std::move(index), "returns",
                                std::move(values));
}

dataframe transactions_frame_fixture() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    dataframe transactions;
    transactions.load_index(
        std::vector<timestamp>{start, start, start + day});
    transactions.load_column<std::string>(
        "symbol", {"A", "B", "A"}, hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>(
        "amount", {10.0, -5.0, -2.0},
        hmdf::nan_policy::dont_pad_with_nans);
    transactions.load_column<double>(
        "price", {5.0, 20.0, 6.0},
        hmdf::nan_policy::dont_pad_with_nans);
    return transactions;
}

std::vector<fincore::portfolio_transaction> transaction_vector_fixture() {
    const auto start = fincore::utc_timestamp(2022, 1, 3);
    return {{start, "A", 10.0, 5.0},
            {start, "B", -5.0, 20.0},
            {start + day, "A", -2.0, 6.0}};
}

dataframe exceptions_fixture() {
    dataframe frame;
    frame.load_index(std::vector<timestamp>{0, 1, 2, 3, 4, 5});
    frame.load_column<bool>(
        "exceptions", {false, true, false, false, true, false},
        hmdf::nan_policy::dont_pad_with_nans);
    return frame;
}

const operation_spec& find_spec(const std::vector<operation_spec>& specs,
                                const std::string& id) {
    for (const auto& spec : specs) {
        if (spec.operation_id == id) {
            return spec;
        }
    }
    throw std::runtime_error("operation not declared: " + id);
}

std::string domain_of(const std::string& id) {
    return id.substr(0U, id.find('.'));
}

}  // namespace

TEST(PortfolioRuntimeOperations, DeclaresCompleteUniqueNativeCatalog) {
    const auto specs = fincore::portfolio_risk_simulation_operations();
    ASSERT_EQ(specs.size(), 60U);

    std::set<std::string> ids;
    std::map<std::string, std::size_t> domains;
    for (const auto& spec : specs) {
        EXPECT_TRUE(ids.insert(spec.operation_id).second)
            << spec.operation_id;
        EXPECT_EQ(spec.capability_id.rfind("fincore.", 0U), 0U);
        EXPECT_EQ(spec.domain, domain_of(spec.operation_id));
        EXPECT_TRUE(spec.callable);
        EXPECT_FALSE(spec.rng_policy.empty());
        EXPECT_EQ(spec.provenance.at("source_operation_id"),
                  spec.operation_id);
        EXPECT_FALSE(spec.provenance.at("kernel_symbol").empty());
        EXPECT_EQ(spec.provenance.at("input_schema").front(), '{');
        EXPECT_EQ(spec.provenance.at("input_schema").back(), '}');
        ++domains[spec.domain];
    }
    EXPECT_EQ(domains["portfolio"], 30U);
    EXPECT_EQ(domains["risk"], 15U);
    EXPECT_EQ(domains["simulation"], 15U);

    EXPECT_EQ(ids.count("portfolio.transactions.make_transaction_frame"), 1U);
    EXPECT_EQ(ids.count("portfolio.transactions.map_transaction"), 1U);

    const auto& annualize = find_spec(specs, "simulation.base.annualize");
    EXPECT_TRUE(annualize.deterministic);
    EXPECT_EQ(annualize.rng_policy, "none");

    // A fixed seed makes these reproducible, yet they stay non-deterministic:
    // deterministic and rng_policy are separate declarations, checked separately.
    for (const auto& id : {"simulation.bootstrap.bootstrap",
                           "simulation.bootstrap.bootstrap_ci",
                           "simulation.bootstrap.bootstrap_summary",
                           "simulation.paths.gbm_from_returns",
                           "simulation.paths.geometric_brownian_motion",
                           "simulation.paths.latin_hypercube_sampling",
                           "simulation.monte_carlo.MonteCarlo.from_parameters"}) {
        const auto& spec = find_spec(specs, id);
        EXPECT_FALSE(spec.deterministic) << id;
        EXPECT_EQ(spec.rng_policy, "seeded_mt19937") << id;
    }
    EXPECT_TRUE(find_spec(specs, "simulation.paths.antithetic_variates")
                    .deterministic);
    EXPECT_TRUE(find_spec(specs, "simulation.scenarios.stress_test")
                    .deterministic);
}

TEST(PortfolioRuntimeOperations, AliasesReuseCanonicalNativeKernels) {
    const auto specs = fincore::portfolio_risk_simulation_operations();
    const auto kernel = [&specs](const std::string& id) {
        return find_spec(specs, id).provenance.at("kernel_symbol");
    };
    // Aliases are tied together only by sharing a kernel_symbol; the catalogue has
    // no separate alias field, so equal symbols are the whole relation.
    EXPECT_EQ(kernel("portfolio.positions.extract_pos"),
              "stack_positions");
    EXPECT_EQ(kernel("portfolio.positions.stack_positions"),
              "stack_positions");
    EXPECT_EQ(kernel("portfolio.round_trips.groupby_consecutive"),
              "groupby_consecutive_transactions");
    EXPECT_EQ(kernel("simulation.base.compute_statistics"),
              "analyze_simulation_paths");
    EXPECT_EQ(kernel("simulation.base.estimate_parameters"),
              "estimate_simulation_parameters");
    EXPECT_EQ(kernel("simulation.bootstrap.bootstrap"),
              "bootstrap_distribution");
    EXPECT_EQ(kernel("simulation.bootstrap.bootstrap_ci"),
              "bootstrap_confidence_interval");
}

TEST(PortfolioRuntimeOperations, CatalogDigestIsDeterministic) {
    const operation_catalog first{
        fincore::portfolio_risk_simulation_operations()};
    const operation_catalog second{
        fincore::portfolio_risk_simulation_operations()};
    EXPECT_EQ(first.digest(), second.digest());
    EXPECT_EQ(first.operation_ids(), second.operation_ids());
    EXPECT_EQ(first.operations().size(), 60U);
}

TEST(PortfolioRuntimeOperations, DispatchesPortfolioPositionOperations) {
    const auto specs = fincore::portfolio_risk_simulation_operations();
    runtime_inputs inputs;
    inputs.emplace("positions", runtime_value(positions_fixture()));

    const auto allocation =
        find_spec(specs, "portfolio.positions.get_percent_alloc")
            .callable(inputs);
    const auto* allocation_frame = std::get_if<dataframe>(&allocation.data);
    ASSERT_NE(allocation_frame, nullptr);
    EXPECT_TRUE(allocation_frame->has_column("A"));
    EXPECT_TRUE(allocation_frame->has_column("cash"));
    EXPECT_EQ(allocation_frame->get_index().size(), 3U);

    const auto stacked =
        find_spec(specs, "portfolio.positions.extract_pos")
            .callable(inputs);
    const auto* stacked_frame = std::get_if<dataframe>(&stacked.data);
    ASSERT_NE(stacked_frame, nullptr);
    EXPECT_EQ(stacked_frame->get_index().size(), 6U);
    ASSERT_EQ(stacked_frame->get_columns_info<std::string>().size(), 1U);

    const auto top =
        find_spec(specs, "portfolio.positions.get_top_long_short_abs")
            .callable(inputs);
    const auto* top_frame = std::get_if<dataframe>(&top.data);
    ASSERT_NE(top_frame, nullptr);
    EXPECT_TRUE(top_frame->has_column("ranking"));
    EXPECT_TRUE(top_frame->has_column("symbol"));
    EXPECT_TRUE(top_frame->has_column("value"));
}

TEST(PortfolioRuntimeOperations, DispatchesPortfolioTransactionOperations) {
    const auto specs = fincore::portfolio_risk_simulation_operations();
    runtime_inputs inputs;
    inputs.emplace("transactions",
                   runtime_value(transaction_vector_fixture()));

    const auto volume =
        find_spec(specs, "portfolio.transactions.get_txn_vol")
            .callable(inputs);
    const auto* volume_frame = std::get_if<dataframe>(&volume.data);
    ASSERT_NE(volume_frame, nullptr);
    EXPECT_TRUE(volume_frame->has_column("txn_volume"));
    EXPECT_TRUE(volume_frame->has_column("txn_shares"));

    runtime_inputs round_trip_inputs;
    round_trip_inputs.emplace("transactions",
                              runtime_value(transactions_frame_fixture()));
    const auto grouped =
        find_spec(specs, "portfolio.round_trips.groupby_consecutive")
            .callable(round_trip_inputs);
    const auto* grouped_frame = std::get_if<dataframe>(&grouped.data);
    ASSERT_NE(grouped_frame, nullptr);
    EXPECT_TRUE(grouped_frame->has_column("symbol"));
    EXPECT_TRUE(grouped_frame->has_column("amount"));
    EXPECT_TRUE(grouped_frame->has_column("price"));
}

TEST(PortfolioRuntimeOperations, DispatchesRiskOperations) {
    const auto specs = fincore::portfolio_risk_simulation_operations();

    runtime_inputs exception_inputs;
    exception_inputs.emplace("exceptions",
                             runtime_value(exceptions_fixture()));
    const auto christoffersen =
        find_spec(specs, "risk.backtesting.christoffersen_lr")
            .callable(exception_inputs);
    const auto* statistic = std::get_if<double>(&christoffersen.data);
    ASSERT_NE(statistic, nullptr);
    EXPECT_TRUE(std::isfinite(*statistic));

    runtime_inputs estimate_inputs;
    estimate_inputs.emplace(
        "returns", runtime_value(returns_fixture(20U, 0.01, 0.002)));
    const auto estimate =
        find_spec(specs, "risk.models.forecast_var")
            .callable(estimate_inputs);
    const auto* estimate_frame = std::get_if<dataframe>(&estimate.data);
    ASSERT_NE(estimate_frame, nullptr);
    ASSERT_TRUE(estimate_frame->has_column("estimate"));
    EXPECT_TRUE(std::isfinite(
        estimate_frame->get_column<double>("estimate")[0]));

    runtime_inputs hill_inputs;
    hill_inputs.emplace(
        "data", runtime_value(returns_fixture(100U, 0.01, 0.001)));
    const auto hill =
        find_spec(specs, "risk.evt.hill_estimator").callable(hill_inputs);
    const auto* hill_frame = std::get_if<dataframe>(&hill.data);
    ASSERT_NE(hill_frame, nullptr);
    EXPECT_TRUE(hill_frame->has_column("xi"));
    EXPECT_TRUE(hill_frame->has_column("tail_observation"));
}

TEST(PortfolioRuntimeOperations, DispatchesSimulationOperations) {
    const auto specs = fincore::portfolio_risk_simulation_operations();

    runtime_inputs annualize_inputs;
    annualize_inputs.emplace("daily_value", runtime_value(0.1));
    annualize_inputs.emplace("period", runtime_value(std::string{"monthly"}));
    const auto annualized =
        find_spec(specs, "simulation.base.annualize")
            .callable(annualize_inputs);
    const auto* annualized_value = std::get_if<double>(&annualized.data);
    ASSERT_NE(annualized_value, nullptr);
    EXPECT_NEAR(*annualized_value, 0.1 * std::sqrt(12.0), 1e-12);

    runtime_inputs parameter_inputs;
    parameter_inputs.emplace(
        "returns", runtime_value(returns_fixture(20U, 0.01, 0.002)));
    const auto parameters =
        find_spec(specs, "simulation.base.estimate_parameters")
            .callable(parameter_inputs);
    const auto* parameter_frame = std::get_if<dataframe>(&parameters.data);
    ASSERT_NE(parameter_frame, nullptr);
    EXPECT_TRUE(parameter_frame->has_column("drift"));
    EXPECT_TRUE(parameter_frame->has_column("volatility"));

    runtime_inputs lhs_inputs;
    lhs_inputs.emplace("n_samples", runtime_value(std::int64_t{8}));
    lhs_inputs.emplace("n_dimensions", runtime_value(std::int64_t{3}));
    lhs_inputs.emplace("seed", runtime_value(std::int64_t{42}));
    const auto samples =
        find_spec(specs, "simulation.paths.latin_hypercube_sampling")
            .callable(lhs_inputs);
    const auto* sample_frame = std::get_if<dataframe>(&samples.data);
    ASSERT_NE(sample_frame, nullptr);
    EXPECT_EQ(sample_frame->shape().first, 8U);
    EXPECT_EQ(sample_frame->shape().second, 3U);

    runtime_inputs bootstrap_inputs;
    bootstrap_inputs.emplace(
        "returns", runtime_value(returns_fixture(20U, 0.01, 0.002)));
    bootstrap_inputs.emplace("n_samples", runtime_value(std::int64_t{200}));
    bootstrap_inputs.emplace("seed", runtime_value(std::int64_t{7}));
    const auto interval =
        find_spec(specs, "simulation.bootstrap.bootstrap_ci")
            .callable(bootstrap_inputs);
    const auto* interval_frame = std::get_if<dataframe>(&interval.data);
    ASSERT_NE(interval_frame, nullptr);
    ASSERT_TRUE(interval_frame->has_column("lower"));
    ASSERT_TRUE(interval_frame->has_column("upper"));
    EXPECT_TRUE(std::isfinite(
        interval_frame->get_column<double>("lower")[0]));
    EXPECT_TRUE(std::isfinite(
        interval_frame->get_column<double>("upper")[0]));

    runtime_inputs stress_inputs;
    stress_inputs.emplace(
        "returns", runtime_value(returns_fixture(20U, 0.01, 0.002)));
    const auto stressed =
        find_spec(specs, "simulation.scenarios.stress_test")
            .callable(stress_inputs);
    const auto* stress_frame = std::get_if<dataframe>(&stressed.data);
    ASSERT_NE(stress_frame, nullptr);
    EXPECT_TRUE(stress_frame->has_column("scenario"));
    EXPECT_TRUE(stress_frame->has_column("stressed_return"));
    EXPECT_EQ(stress_frame->get_index().size(), 80U);
}

TEST(PortfolioRuntimeOperations, DispatchesTransactionMappingOperations) {
    const auto specs = fincore::portfolio_risk_simulation_operations();

    runtime_inputs frame_inputs;
    frame_inputs.emplace(
        "transactions", runtime_value(transaction_vector_fixture()));
    const auto frame_result =
        find_spec(specs, "portfolio.transactions.make_transaction_frame")
            .callable(frame_inputs);
    const auto* frame = std::get_if<dataframe>(&frame_result.data);
    ASSERT_NE(frame, nullptr);
    ASSERT_EQ(frame->get_index().size(), 3U);
    EXPECT_TRUE(frame->has_column("symbol"));
    EXPECT_DOUBLE_EQ(frame->get_column<double>("amount")[0], 10.0);
    EXPECT_DOUBLE_EQ(frame->get_column<double>("price")[1], 20.0);

    runtime_inputs single_inputs;
    single_inputs.emplace(
        "transaction",
        runtime_value(std::vector<fincore::portfolio_transaction>{
            transaction_vector_fixture().front()}));
    const auto mapped_result =
        find_spec(specs, "portfolio.transactions.map_transaction")
            .callable(single_inputs);
    const auto* mapped = std::get_if<std::vector<fincore::portfolio_transaction>>(
        &mapped_result.data);
    ASSERT_NE(mapped, nullptr);
    ASSERT_EQ(mapped->size(), 1U);
    EXPECT_EQ(mapped->front().symbol, "A");
    EXPECT_DOUBLE_EQ(mapped->front().amount, 10.0);
}
