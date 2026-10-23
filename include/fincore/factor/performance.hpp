#pragma once

#include <fincore/factor/data.hpp>

#include <cstddef>
#include <span>
#include <string>
#include <vector>

namespace fincore {

struct factor_ic_table {
    std::vector<timestamp> dates;
    std::vector<std::string> labels;
    std::vector<double> values;  // dates x labels, NaN missing
};

struct factor_ic_summary {
    std::vector<std::string> labels;
    std::vector<double> means;
};

struct factor_weight_row {
    timestamp date;
    std::string asset;
    double weight;
};

struct factor_return_table {
    std::vector<timestamp> dates;
    std::vector<std::string> labels;
    std::vector<double> values;  // dates x labels
};

struct factor_alpha_beta_row {
    std::string label;
    double annual_alpha;
    double beta;
};

struct quantile_return_table {
    std::vector<int> quantiles;
    std::vector<std::string> labels;
    std::vector<double> means;           // quantiles x labels
    std::vector<double> standard_errors; // quantiles x labels
};

struct factor_series_result {
    std::vector<timestamp> dates;
    std::vector<double> values;
};

factor_ic_table factor_information_coefficient(
    const prepared_factor_data& factor_data, bool group_adjust = false,
    bool by_group = false);

factor_ic_summary mean_information_coefficient(
    const prepared_factor_data& factor_data, bool group_adjust = false,
    bool by_group = false);

std::vector<factor_weight_row> factor_weights(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool equal_weight = false);

factor_return_table factor_returns(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool equal_weight = false);

std::vector<factor_alpha_beta_row> factor_alpha_beta(
    const prepared_factor_data& factor_data, bool demeaned = true,
    bool group_adjust = false, bool equal_weight = false);

quantile_return_table mean_return_by_quantile(
    const prepared_factor_data& factor_data, bool by_date = false,
    bool by_group = false, bool demeaned = true,
    bool group_adjust = false);

factor_series_result quantile_turnover(
    const prepared_factor_data& factor_data, int quantile,
    int period = 1);

factor_series_result factor_rank_autocorrelation(
    const prepared_factor_data& factor_data, int period = 1);

double ic_mean(std::span<const double> values);
double ic_t_stat(std::span<const double> values);
struct ic_interval {
    double lower;
    double upper;
};
ic_interval ic_confidence_interval(std::span<const double> values,
                                   double z = 1.96);

struct fdr_result {
    std::vector<double> p_values;
    std::vector<double> adjusted_p_values;
    std::vector<bool> rejected;
    double alpha = 0.05;
};

fdr_result benjamini_hochberg(std::span<const double> p_values,
                              double alpha = 0.05);

struct factor_hypothesis {
    std::string label;
    std::size_t n_observations = 0U;
    double mean_ic = 0.0;
    double t_stat = 0.0;
    double p_value = 0.0;
    double adjusted_p_value = 0.0;
    bool rejected = false;
    bool testable = false;
};

struct ic_inference_result {
    double alpha = 0.05;
    std::vector<factor_hypothesis> hypotheses;
};

ic_inference_result information_coefficient_inference(
    const factor_ic_table& information, double alpha = 0.05);

}  // namespace fincore
