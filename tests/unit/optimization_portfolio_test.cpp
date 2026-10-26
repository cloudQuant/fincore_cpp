#include <fincore/optimization/frontier.hpp>
#include <fincore/optimization/objectives.hpp>
#include <fincore/optimization/risk_parity.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <limits>
#include <string>
#include <vector>

namespace {

fincore::dataframe optimization_fixture() {
    std::vector<fincore::timestamp> index;
    std::vector<std::vector<double>> columns(4);
    for (int position = 0; position < 300; ++position) {
        index.push_back(static_cast<fincore::timestamp>(position + 1));
        for (int asset = 0; asset < 4; ++asset) {
            columns[static_cast<std::size_t>(asset)].push_back(
                0.0004 * asset +
                0.01 * std::sin((position + 1) * (0.7 + 0.1 * asset)) +
                0.003 *
                    std::cos((position + 1) * (0.13 + 0.07 * asset)));
        }
    }
    fincore::dataframe frame;
    frame.load_index(std::move(index));
    static const char* names[] = {"A", "B", "C", "D"};
    for (int asset = 0; asset < 4; ++asset) {
        frame.load_column<double>(
            names[asset], std::move(columns[static_cast<std::size_t>(asset)]),
            hmdf::nan_policy::dont_pad_with_nans);
    }
    return frame;
}

void expect_weights(const std::vector<double>& actual,
                    const std::vector<double>& expected,
                    const double tolerance) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < actual.size(); ++index) {
        EXPECT_NEAR(actual[index], expected[index], tolerance)
            << "weight " << index;
    }
}

TEST(OptimizationPortfolio, MinVarianceMatchesPythonOracle) {
    fincore::portfolio_optimization_config config;
    config.objective = fincore::portfolio_objective::min_variance;
    const auto result = fincore::optimize_portfolio(optimization_fixture(),
                                                    config);
    expect_weights(result.weights,
                   {0.2449111175220411, 0.25453194116959454,
                    0.25452347467870895, 0.24603346662965547},
                   1e-5);
    EXPECT_NEAR(result.expected_return, 0.16332275676201832, 5e-7);
    EXPECT_NEAR(result.volatility, 0.05793482049938796, 5e-7);
    EXPECT_NEAR(result.sharpe, 2.8190776350768827, 5e-6);
    EXPECT_EQ(result.asset_names.size(), 4U);
    EXPECT_EQ(result.asset_names.front(), "A");
    EXPECT_EQ(result.asset_names.back(), "D");
    EXPECT_EQ(result.objective, fincore::portfolio_objective::min_variance);
}

TEST(OptimizationPortfolio, MaxSharpeMatchesPythonOracle) {
    fincore::portfolio_optimization_config config;
    config.objective = fincore::portfolio_objective::max_sharpe;
    config.risk_free_rate = 0.02;
    config.max_weight = 0.7;
    const auto result = fincore::optimize_portfolio(optimization_fixture(),
                                                    config);
    expect_weights(result.weights,
                   {0.0342053696311322, 0.1541496893519629,
                    0.3115427697687592, 0.5001021712481456},
                   1e-4);
    EXPECT_NEAR(result.expected_return, 0.23448614828750652, 1e-6);
    EXPECT_NEAR(result.volatility, 0.0708731181623167, 1e-6);
    EXPECT_NEAR(result.sharpe, 3.0263399417009, 1e-6);
}

TEST(OptimizationPortfolio, TargetReturnAndTargetRiskMatchOracle) {
    fincore::portfolio_optimization_config config;
    config.objective = fincore::portfolio_objective::target_return;
    config.target_return = 0.15;
    config.max_weight = 0.7;
    const auto target_return =
        fincore::optimize_portfolio(optimization_fixture(), config);
    expect_weights(target_return.weights,
                   {0.28435786369316385, 0.2733254168638967,
                    0.2438482724258347, 0.19846844701710478},
                   5e-3);
    EXPECT_NEAR(target_return.expected_return, 0.14999999999999505, 1e-6);
    EXPECT_NEAR(target_return.volatility, 0.058436766019624525, 1e-5);
    double total = 0.0;
    for (const double weight : target_return.weights) {
        total += weight;
    }
    EXPECT_NEAR(total, 1.0, 1e-8);

    config = {};
    config.objective = fincore::portfolio_objective::target_risk;
    config.target_volatility = 0.06;
    config.max_weight = 0.7;
    const auto target_risk =
        fincore::optimize_portfolio(optimization_fixture(), config);
    expect_weights(target_risk.weights,
                   {0.16436060916751138, 0.2161572599144954,
                    0.2763210875385537, 0.3431610433794395},
                   5e-4);
    EXPECT_NEAR(target_risk.expected_return, 0.1905276990129139, 1e-6);
    EXPECT_NEAR(target_risk.volatility, 0.060000000000001885, 1e-7);
    EXPECT_NEAR(target_risk.sharpe, 3.175461650215132, 1e-6);

    config.target_volatility = 0.10;
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(
                     optimization_fixture(), config)),
                 fincore::optimization_error);
}

TEST(OptimizationPortfolio, SectorAndMinWeightConstraints) {
    fincore::portfolio_optimization_config config;
    config.objective = fincore::portfolio_objective::min_variance;
    config.max_weight = 0.8;
    config.sector_constraints = {{"x", 0.2, 0.6}, {"y", 0.2, 0.6}};
    config.sector_map = {{"A", "x"}, {"B", "x"}, {"C", "y"}, {"D", "y"}};
    const auto sector = fincore::optimize_portfolio(optimization_fixture(),
                                                    config);
    expect_weights(sector.weights,
                   {0.2449111175220411, 0.25453194116959454,
                    0.25452347467870895, 0.24603346662965547},
                   5e-3);
    EXPECT_NEAR(sector.volatility, 0.05793482049938796, 1e-5);

    config = {};
    config.objective = fincore::portfolio_objective::min_variance;
    config.max_weight = 0.7;
    config.min_weight = 0.1;
    const auto bounded = fincore::optimize_portfolio(optimization_fixture(),
                                                     config);
    expect_weights(bounded.weights,
                   {0.2449111175220411, 0.25453194116959454,
                    0.25452347467870895, 0.24603346662965547},
                   1e-5);

    config = {};
    config.objective = fincore::portfolio_objective::min_variance;
    config.short_allowed = true;
    config.max_weight = 0.8;
    const auto shorted = fincore::optimize_portfolio(optimization_fixture(),
                                                     config);
    expect_weights(shorted.weights,
                   {0.2449111175220411, 0.25453194116959454,
                    0.25452347467870895, 0.24603346662965547},
                   1e-5);
}

TEST(OptimizationPortfolio, FrontierMatchesPythonOracle) {
    fincore::frontier_config config;
    config.n_points = 10;
    config.risk_free_rate = 0.02;
    config.max_weight = 0.7;
    const auto result =
        fincore::efficient_frontier(optimization_fixture(), config);
    ASSERT_EQ(result.frontier_returns.size(), 10U);
    ASSERT_EQ(result.frontier_weights.size(), 10U);
    EXPECT_DOUBLE_EQ(result.periods_per_year, 252.0);
    EXPECT_NEAR(result.min_variance.expected_return,
                0.16332275676201832, 5e-7);
    EXPECT_NEAR(result.min_variance.volatility,
                0.05793482049938796, 5e-7);
    EXPECT_NEAR(result.max_sharpe.expected_return, 0.23448614828750652,
                1e-6);
    EXPECT_NEAR(result.max_sharpe.volatility, 0.0708731181623167, 1e-6);
    EXPECT_NEAR(result.max_sharpe_ratio, 3.0263399417009, 1e-6);

    const std::vector<double> expected_returns{
        0.16332275676201832, 0.17595600520570476, 0.18858925364970883,
        0.20122250209397566, 0.2138557505376538,  0.22648899898249766,
        0.23912224742607238, 0.25175549587013674, 0.264388744314103,
        0.2770219927580827};
    const std::vector<double> expected_volatilities{
        0.05793482049938796, 0.0583863346833283,  0.05972041470636603,
        0.06188000477690715, 0.0647825999262175,  0.06833358412076472,
        0.07243766553281822, 0.07707403396496358, 0.08261408321831713,
        0.08898211825400286};
    for (std::size_t point = 0; point < 10U; ++point) {
        EXPECT_NEAR(result.frontier_returns[point],
                    expected_returns[point], 5e-7)
            << "point " << point;
        EXPECT_NEAR(result.frontier_volatilities[point],
                    expected_volatilities[point], 5e-7)
            << "point " << point;
        EXPECT_NEAR(result.frontier_sharpe[point],
                    (expected_returns[point] - 0.02) /
                        expected_volatilities[point],
                    5e-5);
        EXPECT_NEAR(result.frontier_diagnostics[point].sum_residual, 0.0,
                    1e-8);
        EXPECT_NEAR(result.frontier_diagnostics[point].upper_bound_residual,
                    0.0, 1e-8);
    }
    expect_weights(result.frontier_weights[0],
                   {0.2449111175220411, 0.25453194116959454,
                    0.25452347467870895, 0.24603346662965547},
                   1e-5);
    expect_weights(result.frontier_weights[1],
                   {0.20750656021140507, 0.23671046395479137,
                    0.2646458123564207, 0.2911371634773829},
                   1e-3);
    expect_weights(result.frontier_weights[4],
                   {0.09529037606281332, 0.18324923752871883,
                    0.2950129686583157, 0.42644741775015216},
                   1e-3);
    expect_weights(result.frontier_weights[9],
                   {0.0, 0.0, 0.30000000000000004, 0.7}, 1e-9);
    EXPECT_EQ(result.frontier_diagnostics.front().solver,
              fincore::frontier_solver::min_variance);
    EXPECT_EQ(result.frontier_diagnostics.back().solver,
              fincore::frontier_solver::bounded_linear_max_return);

    fincore::frontier_config short_config;
    short_config.n_points = 5;
    short_config.risk_free_rate = 0.02;
    short_config.short_allowed = true;
    short_config.max_weight = 1.0;
    const auto shorted =
        fincore::efficient_frontier(optimization_fixture(), short_config);
    ASSERT_EQ(shorted.frontier_weights.size(), 5U);
    expect_weights(shorted.frontier_weights.back(),
                   {-1.0, 0.0, 1.0, 1.0}, 1e-9);
    EXPECT_NEAR(shorted.frontier_returns.back(), 0.46572836077640317, 1e-6);
    EXPECT_NEAR(shorted.frontier_volatilities.back(), 0.20163113253722942,
                1e-6);
}

TEST(OptimizationPortfolio, RiskParityMatchesPythonOracle) {
    const auto result = fincore::risk_parity(optimization_fixture());
    expect_weights(result.weights,
                   {0.24739839216870257, 0.2523350851557695,
                    0.2523098511408693, 0.24795667153465864},
                   1e-6);
    expect_weights(result.risk_contributions,
                   {0.014484303352295854, 0.014484303344705018,
                    0.014484303350040997, 0.014484303351729282},
                   1e-9);
    EXPECT_NEAR(result.volatility, 0.05793721339877115, 1e-8);

    const auto custom = fincore::risk_parity(
        optimization_fixture(), std::vector<double>{0.1, 0.2, 0.3, 0.4});
    expect_weights(custom.weights,
                   {0.16086432195279138, 0.23220151467183564,
                    0.2841967290791839, 0.3227374342961891},
                   1e-6);
    expect_weights(custom.risk_contributions,
                   {0.005959405342942881, 0.011918809578876153,
                    0.017878212277923255, 0.023837617723194725},
                   1e-8);
    EXPECT_NEAR(custom.volatility, 0.05959404492293703, 1e-8);
}

TEST(OptimizationPortfolio, MinVarianceHandlesSingularCovarianceFeasibly) {
    fincore::dataframe frame;
    frame.load_index(std::vector<fincore::timestamp>{1, 2, 3, 4});
    const std::vector<double> common_returns{0.01, -0.02, 0.03, 0.0};
    frame.load_column<double>("A", common_returns,
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("B", common_returns,
                              hmdf::nan_policy::dont_pad_with_nans);
    frame.load_column<double>("C", common_returns,
                              hmdf::nan_policy::dont_pad_with_nans);

    fincore::portfolio_optimization_config config;
    config.objective = fincore::portfolio_objective::min_variance;
    const auto result = fincore::optimize_portfolio(frame, config);

    expect_weights(result.weights, {1.0 / 3.0, 1.0 / 3.0, 1.0 / 3.0},
                   1e-10);
    double total = 0.0;
    for (const double weight : result.weights) {
        total += weight;
        EXPECT_GE(weight, 0.0);
        EXPECT_LE(weight, 1.0);
    }
    EXPECT_NEAR(total, 1.0, 1e-12);
    EXPECT_NEAR(result.expected_return, 0.005 * 252.0, 1e-12);
    EXPECT_NEAR(result.volatility,
                std::sqrt((0.0013 / 3.0) * 252.0), 1e-12);
}

TEST(OptimizationPortfolio, RejectsInvalidInputs) {
    const auto frame = optimization_fixture();
    fincore::portfolio_optimization_config config;
    config.objective =
        static_cast<fincore::portfolio_objective>(99);
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(frame, config)),
                 fincore::input_error);

    config = {};
    config.objective = fincore::portfolio_objective::target_return;
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(frame, config)),
                 fincore::input_error);
    config.objective = fincore::portfolio_objective::target_risk;
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(frame, config)),
                 fincore::input_error);
    config = {};
    config.max_weight = 0.0;
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(frame, config)),
                 fincore::input_error);
    config.max_weight = 0.5;
    config.min_weight = 0.6;
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(frame, config)),
                 fincore::input_error);
    config = {};
    config.sector_constraints = {{"x", 0.1, 0.5}};
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(frame, config)),
                 fincore::input_error);

    EXPECT_THROW(
        static_cast<void>(fincore::efficient_frontier(
            frame, fincore::frontier_config{1U, 0.0, false, 1.0, 252.0})),
        fincore::input_error);
    EXPECT_THROW(
        static_cast<void>(fincore::efficient_frontier(
            fincore::make_series(std::vector<fincore::timestamp>{1, 2, 3},
                                 "A", std::vector<double>{0.01, -0.02, 0.03}),
            fincore::frontier_config{})),
        fincore::input_error);
    fincore::frontier_config infeasible;
    infeasible.max_weight = 0.2;
    EXPECT_THROW(static_cast<void>(
                     fincore::efficient_frontier(frame, infeasible)),
                 fincore::optimization_error);

    EXPECT_THROW(static_cast<void>(fincore::risk_parity(
                     frame, std::vector<double>{0.5, 0.5})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::risk_parity(
                     frame, std::vector<double>{-0.1, 0.4, 0.4, 0.3})),
                 fincore::input_error);
    EXPECT_THROW(static_cast<void>(fincore::risk_parity(frame, std::nullopt, 0)),
                 fincore::input_error);

    const auto with_nan = fincore::make_series(
        std::vector<fincore::timestamp>{1, 2, 3}, "A",
        std::vector<double>{0.01, std::numeric_limits<double>::quiet_NaN(),
                            0.03});
    EXPECT_THROW(static_cast<void>(fincore::optimize_portfolio(with_nan)),
                 fincore::input_error);
}

}  // namespace
