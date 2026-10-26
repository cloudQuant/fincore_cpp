#include <fincore/attribution/brinson.hpp>
#include <fincore/attribution/fama_french.hpp>
#include <fincore/attribution/fama_french_patch.hpp>
#include <fincore/attribution/style.hpp>
#include <fincore/calendar.hpp>
#include <fincore/errors.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cmath>
#include <cstddef>
#include <map>
#include <optional>
#include <string>
#include <string_view>
#include <type_traits>
#include <vector>

namespace {

using fincore::timestamp;

fincore::dataframe make_frame(std::vector<timestamp> index,
                              const std::vector<std::string>& columns,
                              const std::vector<std::vector<double>>& values) {
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    for (std::size_t column = 0U; column < columns.size(); ++column) {
        frame.load_column<double>(columns[column].c_str(), values[column],
                                  hmdf::nan_policy::dont_pad_with_nans);
    }
    return frame;
}

fincore::dataframe returns_frame() {
    const auto start = fincore::utc_timestamp(2024, 1, 2);
    const auto day = 86'400'000'000'000LL;
    return make_frame({start, start + day, start + 2 * day, start + 3 * day,
                       start + 4 * day, start + 5 * day},
                      {"returns"},
                      {{.010, .020, -.005, .015, .004, -.002}});
}

fincore::dataframe factor_frame() {
    const auto start = fincore::utc_timestamp(2024, 1, 2);
    const auto day = 86'400'000'000'000LL;
    return make_frame({start, start + day, start + 2 * day, start + 3 * day,
                       start + 4 * day, start + 5 * day},
                      {"MKT", "SMB", "HML"},
                      {{.008, .012, -.006, .010, .002, .000},
                       {.002, -.001, .003, .001, -.002, .001},
                       {-.001, .002, .001, -.003, .002, .000}});
}

TEST(AttributionFacades, FactorNameConstantsMatchPinnedSource) {
    EXPECT_EQ(fincore::FF3_FACTORS.size(), 3U);
    EXPECT_EQ(fincore::FF4MOM_FACTORS.size(), 4U);
    EXPECT_EQ(fincore::FF5_FACTORS.size(), 5U);
    EXPECT_EQ(fincore::FF3_FACTORS[0], "MKT");
    EXPECT_EQ(fincore::FF3_FACTORS[1], "SMB");
    EXPECT_EQ(fincore::FF3_FACTORS[2], "HML");
    EXPECT_EQ(fincore::FF4MOM_FACTORS[3], "MOM");
    EXPECT_EQ(fincore::FF5_FACTORS[3], "RMW");
    EXPECT_EQ(fincore::FF5_FACTORS[4], "CMA");
    EXPECT_TRUE((std::is_same_v<fincore::FamaFrenchModelPatch,
                                fincore::FamaFrenchModel>));
}

TEST(AttributionFacades, InjectedFamaFrenchProviderDetachesAndValidates) {
    auto owned = returns_frame();
    std::string seen_start;
    std::string seen_end;
    std::string seen_library;
    const fincore::fama_french_provider provider =
        [&](std::string_view start, std::string_view end,
            std::string_view library) {
            seen_start = std::string{start};
            seen_end = std::string{end};
            seen_library = std::string{library};
            return owned;
        };

    auto fetched = fincore::fetch_ff_factors("2024-01-01", "2024-12-31",
                                             "french", provider);
    EXPECT_EQ(seen_start, "2024-01-01");
    EXPECT_EQ(seen_end, "2024-12-31");
    EXPECT_EQ(seen_library, "french");
    // Reloading an existing name overwrites that column, so the original values
    // below are reachable only if the fetch detached them; an aliased provider
    // buffer would read 9.0 here.
    owned.load_column<double>("returns", {9.0, 9.0, 9.0, 9.0, 9.0, 9.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    const auto& values = fetched.get_column<double>("returns");
    ASSERT_EQ(values.size(), 6U);
    EXPECT_DOUBLE_EQ(values[0], .010);
    EXPECT_DOUBLE_EQ(values[5], -.002);

    const fincore::fama_french_provider empty_provider =
        [](std::string_view, std::string_view, std::string_view) {
            return fincore::dataframe{};
        };
    EXPECT_THROW(fincore::fetch_ff_factors("2024-01-01", "2024-12-31",
                                           "french", empty_provider),
                 fincore::fincore_error);
    EXPECT_THROW(fincore::fetch_ff_factors(
                     "2024-01-01", "2024-12-31", "french", {}),
                 fincore::fincore_error);

    const auto convenience = fincore::fetch_ff_factors(
        "2024-01-01", "2024-12-31", provider);
    EXPECT_EQ(convenience.get_index(), fetched.get_index());
}

TEST(AttributionFacades, InjectedStyleProviderAndResultProjections) {
    auto owned = returns_frame();
    const fincore::style_factor_provider provider =
        [&](const std::vector<std::string>& tickers,
            const std::optional<std::vector<std::string>>& factors,
            std::string_view library) {
            EXPECT_EQ(tickers, (std::vector<std::string>{"AAA", "BBB"}));
            EXPECT_FALSE(factors.has_value());
            EXPECT_EQ(library, "us");
            return owned;
        };
    auto fetched = fincore::fetch_style_factors({"AAA", "BBB"}, std::nullopt,
                                                "us", provider);
    owned.load_column<double>("returns", {1.0, 1.0, 1.0, 1.0, 1.0, 1.0},
                              hmdf::nan_policy::dont_pad_with_nans);
    EXPECT_DOUBLE_EQ(fetched.get_column<double>("returns")[0], .010);
    EXPECT_THROW(fincore::fetch_style_factors({}, std::nullopt, "us", provider),
                 fincore::fincore_error);

    auto exposures = make_frame({0, 1}, {"equal_weight"}, {{1.0, 1.0}});
    exposures.load_column<std::string>("asset", {"AAA", "BBB"},
                                       hmdf::nan_policy::dont_pad_with_nans);
    auto with_style_column = make_frame({0, 1}, {"return"}, {{.01, -.02}});
    with_style_column.load_column<std::string>("style", {"equal_weight", "value"},
                                               hmdf::nan_policy::dont_pad_with_nans);
    const auto overall = make_frame({0, 1}, {"returns"}, {{.01, -.02}});

    fincore::StyleResult result{exposures, with_style_column, overall};
    const auto summary = result.style_summary();
    ASSERT_EQ(summary.size(), 2U);
    EXPECT_DOUBLE_EQ(summary.at("equal_weight"), .01);
    EXPECT_DOUBLE_EQ(summary.at("value"), -.02);
    const auto dictionary = result.to_dict();
    EXPECT_EQ(dictionary.size(), 3U);
    EXPECT_EQ(dictionary.at("exposures").get_index(), exposures.get_index());
    EXPECT_EQ(dictionary.at("overall_returns").get_column<double>("returns")[0],
              .01);

    const auto analysis = fincore::style_analysis_result{
        exposures, with_style_column, overall};
    fincore::StyleResult from_analysis{analysis};
    EXPECT_EQ(from_analysis.style_summary().size(), 2U);
}

TEST(AttributionFacades, BrinsonFacadeDelegatesToKernelWithEqualWeights) {
    const auto returns = make_frame(
        {1704153600000000000LL, 1704240000000000000LL}, {"tech", "health"},
        {{.10, .03}, {.05, -.01}});
    const auto equal_weights = make_frame(
        {1704153600000000000LL, 1704240000000000000LL}, {"tech", "health"},
        {{.5, .5}, {.5, .5}});

    const fincore::BrinsonAttribution facade;
    const auto actual = facade.calculate(returns);
    const std::array<std::string_view, 2U> sectors{"tech", "health"};
    // The expectation reruns the same kernel with the documented defaults, so
    // this pins only the delegation: a kernel change moves both sides together.
    const auto expected = fincore::brinson_results(
        returns, returns, equal_weights, equal_weights, sectors);
    ASSERT_EQ(actual.get_index(), expected.get_index());
    for (const std::string_view column : {"allocation", "selection",
                                          "interaction", "total",
                                          "portfolio_return",
                                          "benchmark_return"}) {
        const auto left = actual.get_column<double>(std::string{column}.c_str());
        const auto right =
            expected.get_column<double>(std::string{column}.c_str());
        ASSERT_EQ(left.size(), right.size());
        for (std::size_t row = 0U; row < left.size(); ++row) {
            EXPECT_DOUBLE_EQ(left[row], right[row]);
        }
    }

    const fincore::BrinsonAttribution mapped(
        {{"equity", {"tech"}}, {"defensive", {"health"}}});
    const auto mapped_result = mapped.calculate(returns, std::nullopt,
                                                std::nullopt, "brinson_hood");
    ASSERT_EQ(mapped_result.get_columns_info<double>().size(), 6U);
    EXPECT_THROW(static_cast<void>(facade.calculate(
                     returns, std::nullopt, std::nullopt, "unknown")),
                 fincore::fincore_error);
}

TEST(AttributionFacades, FamaFrenchModelFitsPredictsAndAttributes) {
    const auto returns = returns_frame();
    const auto factors = factor_frame();

    fincore::FamaFrenchModel model{"3factor"};
    ASSERT_EQ(model.factors().size(), 3U);
    const auto fit = model.fit(returns, factors, "returns");
    EXPECT_TRUE(model.alpha().has_value());
    ASSERT_EQ(model.betas().size(), 3U);
    EXPECT_NEAR(fit.alpha, model.alpha().value(), 1e-15);
    EXPECT_TRUE(std::isfinite(fit.r_squared));
    EXPECT_GE(fit.r_squared, 0.0);
    EXPECT_LE(fit.r_squared, 1.0);
    EXPECT_EQ(fit.residuals.get_index(), returns.get_index());

    const auto predicted = model.predict(factors);
    const auto& fitted = fit.fitted_returns.get_column<double>("fitted_returns");
    const auto& values = predicted.get_column<double>("fitted_returns");
    ASSERT_EQ(values.size(), fitted.size());
    for (std::size_t row = 0U; row < values.size(); ++row) {
        EXPECT_NEAR(values[row], fitted[row], 1e-12);
    }

    const auto attribution = model.attribution_decomposition(returns, factors);
    EXPECT_TRUE(attribution.contains("alpha"));
    EXPECT_TRUE(attribution.contains("MKT_attribution"));
    EXPECT_TRUE(attribution.contains("specific_return"));
    EXPECT_TRUE(attribution.contains("common_return"));
    EXPECT_TRUE(attribution.contains("unexplained"));

    const auto exposures =
        model.get_factor_exposures(returns, factors, std::size_t{3U});
    ASSERT_EQ(exposures.get_columns_info<double>().size(), 4U);

    fincore::FamaFrenchModelPatch patch{"4factor_mom"};
    EXPECT_EQ(patch.model_type(), "4factor_mom");
    EXPECT_EQ(patch.factors().size(), 4U);
    EXPECT_THROW(fincore::FamaFrenchModel{"bad"}, fincore::fincore_error);
}

}  // namespace
