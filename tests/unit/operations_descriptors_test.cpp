#include <fincore/operations.hpp>
#include <fincore/version.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstddef>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>

namespace {

void expect_domain(const std::span<const fincore::operation_descriptor> operations,
                   const std::string_view domain, const std::size_t expected) {
    ASSERT_EQ(operations.size(), expected);
    for (const auto& operation : operations) {
        EXPECT_EQ(operation.domain, domain);
        const auto dot = operation.operation_id.find('.');
        ASSERT_NE(dot, std::string_view::npos);
        EXPECT_EQ(operation.domain, operation.operation_id.substr(0U, dot));
        EXPECT_EQ(operation.capability_id, operation.operation_id);
        EXPECT_EQ(operation.provenance_owner, domain);
        EXPECT_FALSE(operation.source_module.empty());
        EXPECT_FALSE(operation.operation_id.empty());
        EXPECT_FALSE(operation.rng_policy.empty());
    }
}

TEST(OperationsDescriptors, DomainAccessorsExposeStableSourceCounts) {
    expect_domain(fincore::attribution_operations(), "attribution", 17U);
    expect_domain(fincore::optimization_operations(), "optimization", 3U);
    expect_domain(fincore::performance_operations(), "performance", 9U);
    expect_domain(fincore::portfolio_operations(), "portfolio", 30U);
    expect_domain(fincore::report_operations(), "report", 3U);
    expect_domain(fincore::risk_operations(), "risk", 20U);
    expect_domain(fincore::simulation_operations(), "simulation", 15U);
    expect_domain(fincore::data_operations(), "data", 4U);
    expect_domain(fincore::factor_analysis_operations(), "factor_analysis",
                  41U);
    expect_domain(fincore::metrics_operations(), "metrics", 131U);
}

TEST(OperationsDescriptors, AllOperationIdsAreUniqueAndComplete) {
    const auto all = fincore::all_operations();
    ASSERT_EQ(all.size(), 273U);
    std::unordered_set<std::string> ids;
    ids.reserve(all.size());
    for (const auto& operation : all) {
        EXPECT_TRUE(ids.insert(std::string{operation.operation_id}).second)
            << operation.operation_id;
        EXPECT_EQ(operation.capability_id, operation.operation_id);
    }
    EXPECT_EQ(ids.size(), 273U);

    // Every check above is order-free, so these position pins are the only ones
    // holding the domain tables in source order rather than accepting the same
    // set of ids in any permutation.
    EXPECT_EQ(fincore::attribution_operations().front().operation_id,
              "attribution.brinson.brinson_attribution");
    EXPECT_EQ(fincore::metrics_operations().back().operation_id,
              "metrics.yearly.sharpe_ratio_by_year");
    EXPECT_EQ(fincore::simulation_operations()[4].operation_id,
              "simulation.bootstrap.bootstrap");
    EXPECT_FALSE(fincore::simulation_operations()[4].deterministic);
    EXPECT_EQ(fincore::simulation_operations()[4].rng_policy,
              "explicit_seed");
    EXPECT_TRUE(fincore::attribution_operations()[0].deterministic);
    EXPECT_EQ(fincore::attribution_operations()[0].rng_policy, "none");

    // The 273 size is already pinned above, so the pointer comparison is the
    // load-bearing line here: it shows both calls view one buffer that outlives
    // them, which is what lets a consumer keep a returned span.
    const auto second = fincore::all_operations();
    EXPECT_EQ(second.data(), all.data());
    EXPECT_EQ(second.size(), all.size());
}

TEST(OperationsDescriptors, ExposesPinnedPythonVersionIdentity) {
    EXPECT_EQ(fincore::version(), "0.5.2.dev0");
    EXPECT_STREQ(fincore::pinned_version_string, "0.5.2.dev0");
    EXPECT_EQ(fincore::pinned_version_major, 0);
    EXPECT_EQ(fincore::pinned_version_minor, 5);
    EXPECT_EQ(fincore::pinned_version_patch, 2);
}

TEST(OperationsDescriptors, EveryDomainPartitionMatchesAllOperations) {
    std::size_t total = 0U;
    for (const auto domain :
         {fincore::attribution_operations(), fincore::optimization_operations(),
          fincore::performance_operations(), fincore::portfolio_operations(),
          fincore::report_operations(), fincore::risk_operations(),
          fincore::simulation_operations(), fincore::data_operations(),
          fincore::factor_analysis_operations(),
          fincore::metrics_operations()}) {
        total += domain.size();
    }
    EXPECT_EQ(total, 273U);
}

}  // namespace
