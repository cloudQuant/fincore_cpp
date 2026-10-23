include(FetchContent)

FetchContent_Declare(
    googletest
    GIT_REPOSITORY https://github.com/google/googletest.git
    GIT_TAG v1.15.2
    GIT_SHALLOW TRUE)
FetchContent_MakeAvailable(googletest)

add_executable(fincore_tests
    unit/build_contract_test.cpp
    unit/calendar_test.cpp
    unit/dataframe_test.cpp
    unit/basic_test.cpp
    unit/bayesian_cone_test.cpp
    unit/returns_test.cpp
    unit/drawdown_test.cpp
    unit/consecutive_test.cpp
    unit/ratios_test.cpp
    unit/extended_ratios_test.cpp
    unit/stats_test.cpp
    unit/risk_test.cpp
    unit/extended_risk_test.cpp
    unit/alpha_beta_test.cpp
    unit/extended_alpha_beta_test.cpp
    unit/annual_alpha_beta_test.cpp
    unit/timing_test.cpp
    unit/yearly_test.cpp
    unit/metrics_umbrella_test.cpp
    unit/metrics_bayesian_cone_umbrella_test.cpp
    unit/analysis_context_test.cpp
    unit/rolling_engine_test.cpp
    unit/rolling_metrics_test.cpp
    unit/perf_stats_test.cpp
    unit/performance_returns_test.cpp
    unit/performance_cashflows_test.cpp
    unit/performance_inference_test.cpp
    unit/performance_disclosures_test.cpp
    unit/perf_stats_public_header_test.cpp
    unit/positions_test.cpp
    unit/transactions_test.cpp
    unit/capacity_test.cpp
    unit/optimization_numerics_test.cpp
    unit/optimization_portfolio_test.cpp
    unit/risk_garch_test.cpp
    unit/risk_evt_test.cpp
    unit/risk_backtesting_test.cpp
    unit/risk_walk_forward_test.cpp
    unit/risk_report_test.cpp
    unit/risk_models_test.cpp
    unit/round_trips_test.cpp
    unit/perf_attrib_test.cpp
    unit/brinson_test.cpp
    unit/factor_regression_test.cpp
    unit/factor_exposures_test.cpp
    unit/factor_attribution_test.cpp
    unit/factor_prediction_test.cpp
    unit/style_analysis_test.cpp
    unit/style_performance_test.cpp
    unit/style_tilts_test.cpp
    unit/style_regression_attribution_test.cpp
    unit/attribution_umbrella_test.cpp
    unit/attribution_factor_regression_umbrella_test.cpp
    unit/attribution_factor_exposures_umbrella_test.cpp
    unit/attribution_factor_attribution_umbrella_test.cpp
    unit/attribution_factor_prediction_umbrella_test.cpp
    unit/attribution_style_analysis_umbrella_test.cpp
    unit/attribution_style_performance_umbrella_test.cpp
    unit/attribution_style_tilts_umbrella_test.cpp
    unit/attribution_style_regression_attribution_umbrella_test.cpp
    unit/stress_scenarios_test.cpp
    unit/correlation_breakdown_test.cpp
    unit/simulation_parameters_test.cpp
    unit/bootstrap_distribution_test.cpp
    unit/bootstrap_confidence_interval_test.cpp
    unit/bootstrap_summary_test.cpp
    unit/gbm_paths_test.cpp
    unit/simulation_gbm_paths_umbrella_test.cpp
    unit/latin_hypercube_sampling_test.cpp
    unit/simulation_latin_hypercube_umbrella_test.cpp
    unit/antithetic_variates_test.cpp
    unit/simulation_antithetic_variates_umbrella_test.cpp
    unit/simulation_result_test.cpp
    unit/simulation_result_umbrella_test.cpp
    unit/simulation_gbm_from_returns_umbrella_test.cpp
    unit/simulation_bootstrap_umbrella_test.cpp
    unit/simulation_bootstrap_confidence_interval_umbrella_test.cpp
    unit/simulation_bootstrap_summary_umbrella_test.cpp
    unit/simulation_umbrella_test.cpp
    unit/simulation_correlation_umbrella_test.cpp
    unit/simulation_parameters_umbrella_test.cpp
    unit/monte_carlo_simulation_test.cpp
    unit/monte_carlo_simulation_umbrella_test.cpp)
target_link_libraries(fincore_tests PRIVATE
    fincore::fincore
    GTest::gtest_main)

include(GoogleTest)
gtest_discover_tests(fincore_tests)

if (TARGET fincore_implot)
    add_test(
        NAME ImPlotAdapterCompile
        COMMAND "${CMAKE_COMMAND}"
            "-DBINARY_DIR=${CMAKE_BINARY_DIR}"
            -P "${CMAKE_CURRENT_SOURCE_DIR}/compile/implot_adapter_compile_test.cmake")
endif()
