#include <fincore/factor/performance.hpp>

#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <limits>
#include <map>
#include <numeric>
#include <set>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr double nan = std::numeric_limits<double>::quiet_NaN();

using bucket_key = std::pair<timestamp, std::string>;

std::vector<timestamp> unique_dates(const prepared_factor_data& data) {
    std::set<timestamp> dates;
    for (const auto& row : data.rows) {
        dates.insert(row.date);
    }
    return {dates.begin(), dates.end()};
}

std::vector<double> average_ranks(std::span<const double> values) {
    const std::size_t count = values.size();
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), 0U);
    std::stable_sort(order.begin(), order.end(),
                     [&values](const std::size_t left,
                               const std::size_t right) {
                         return values[left] < values[right];
                     });
    std::vector<double> ranks(count, 0.0);
    std::size_t position = 0U;
    while (position < count) {
        std::size_t end = position + 1U;
        while (end < count &&
               values[order[end]] == values[order[position]]) {
            ++end;
        }
        const double rank = 0.5 * (static_cast<double>(position + 1U) +
                                   static_cast<double>(end));
        for (std::size_t index = position; index < end; ++index) {
            ranks[order[index]] = rank;
        }
        position = end;
    }
    return ranks;
}

double pearson(std::span<const double> left,
               std::span<const double> right) {
    if (left.size() != right.size() || left.size() < 2U) {
        return nan;
    }
    const double mean_left =
        std::accumulate(left.begin(), left.end(), 0.0) /
        static_cast<double>(left.size());
    const double mean_right =
        std::accumulate(right.begin(), right.end(), 0.0) /
        static_cast<double>(right.size());
    double covariance = 0.0;
    double variance_left = 0.0;
    double variance_right = 0.0;
    for (std::size_t index = 0; index < left.size(); ++index) {
        const double first = left[index] - mean_left;
        const double second = right[index] - mean_right;
        covariance += first * second;
        variance_left += first * first;
        variance_right += second * second;
    }
    if (!(variance_left > 0.0) || !(variance_right > 0.0)) {
        return nan;
    }
    return covariance / std::sqrt(variance_left * variance_right);
}

double spearman(std::span<const double> left,
                std::span<const double> right) {
    const auto left_ranks = average_ranks(left);
    const auto right_ranks = average_ranks(right);
    return pearson(left_ranks, right_ranks);
}

struct demeaning {
    std::vector<std::vector<double>> values;  // labels x rows
};

demeaning demean_returns(const prepared_factor_data& data,
                         const bool by_group) {
    const std::size_t count = data.rows.size();
    const std::size_t labels = data.forward_labels.size();
    demeaning result;
    result.values.assign(labels, std::vector<double>(count, nan));
    for (std::size_t label = 0; label < labels; ++label) {
        for (std::size_t row = 0; row < count; ++row) {
            result.values[label][row] = data.rows[row].forward_returns[label];
        }
    }
    std::map<bucket_key, std::vector<std::size_t>> buckets;
    for (std::size_t row = 0; row < count; ++row) {
        const std::string group = by_group ? data.rows[row].group : "";
        if (by_group && !data.rows[row].has_group) {
            throw factor_data_error{
                "factor_data must contain a group column when group_adjust=True"};
        }
        buckets[{data.rows[row].date, group}].push_back(row);
    }
    for (const auto& bucket : buckets) {
        for (std::size_t label = 0; label < labels; ++label) {
            double total = 0.0;
            std::size_t finite = 0U;
            for (const std::size_t row : bucket.second) {
                if (std::isfinite(result.values[label][row])) {
                    total += result.values[label][row];
                    ++finite;
                }
            }
            if (finite == 0U) {
                continue;
            }
            const double mean = total / static_cast<double>(finite);
            for (const std::size_t row : bucket.second) {
                if (std::isfinite(result.values[label][row])) {
                    result.values[label][row] -= mean;
                }
            }
        }
    }
    return result;
}

std::vector<std::vector<double>> forward_matrix(
    const prepared_factor_data& data) {
    const std::size_t labels = data.forward_labels.size();
    std::vector<std::vector<double>> values(
        labels, std::vector<double>(data.rows.size(), nan));
    for (std::size_t label = 0; label < labels; ++label) {
        for (std::size_t row = 0; row < data.rows.size(); ++row) {
            values[label][row] = data.rows[row].forward_returns[label];
        }
    }
    return values;
}

struct weights_bucket {
    double values;
    bool valid;
};

std::vector<double> to_weights(std::vector<double> values,
                               const bool demeaned,
                               const bool equal_weight) {
    if (equal_weight) {
        if (demeaned) {
            std::vector<double> finite;
            for (const double value : values) {
                if (std::isfinite(value)) {
                    finite.push_back(value);
                }
            }
            if (!finite.empty()) {
                std::sort(finite.begin(), finite.end());
                const std::size_t count = finite.size();
                const double median =
                    count % 2U == 1U
                        ? finite[count / 2U]
                        : 0.5 * (finite[count / 2U - 1U] +
                                 finite[count / 2U]);
                for (double& value : values) {
                    value -= median;
                }
            }
        }
        double negative_count = 0.0;
        double positive_count = 0.0;
        for (const double value : values) {
            if (value < 0.0) {
                negative_count += 1.0;
            } else if (value > 0.0) {
                positive_count += 1.0;
            }
        }
        for (double& value : values) {
            if (value < 0.0) {
                value = -1.0 / negative_count;
            } else if (value > 0.0) {
                value = 1.0 / positive_count;
            }
        }
    } else if (demeaned) {
        double total = 0.0;
        std::size_t count = 0U;
        for (const double value : values) {
            if (std::isfinite(value)) {
                total += value;
                ++count;
            }
        }
        if (count > 0U) {
            const double mean = total / static_cast<double>(count);
            for (double& value : values) {
                value -= mean;
            }
        }
    }
    double gross = 0.0;
    for (const double value : values) {
        if (std::isfinite(value)) {
            gross += std::abs(value);
        }
    }
    if (!(gross > 0.0) || !std::isfinite(gross)) {
        return std::vector<double>(values.size(), nan);
    }
    for (double& value : values) {
        value /= gross;
    }
    return values;
}

std::vector<double> compute_weights(const prepared_factor_data& data,
                                    const bool demeaned,
                                    const bool group_adjust,
                                    const bool equal_weight) {
    const std::size_t count = data.rows.size();
    std::vector<double> weights(count, nan);
    std::map<bucket_key, std::vector<std::size_t>> buckets;
    for (std::size_t row = 0; row < count; ++row) {
        const std::string group = group_adjust ? data.rows[row].group : "";
        if (group_adjust && !data.rows[row].has_group) {
            throw factor_data_error{
                "factor_data must contain a group column when group_adjust=True"};
        }
        buckets[{data.rows[row].date, group}].push_back(row);
    }
    for (const auto& bucket : buckets) {
        std::vector<double> values;
        values.reserve(bucket.second.size());
        for (const std::size_t row : bucket.second) {
            values.push_back(data.rows[row].factor);
        }
        const auto bucket_weights =
            to_weights(values, demeaned, equal_weight);
        for (std::size_t index = 0; index < bucket.second.size(); ++index) {
            weights[bucket.second[index]] = bucket_weights[index];
        }
    }
    if (group_adjust) {
        // Re-normalize across groups per date.
        std::map<timestamp, std::vector<std::size_t>> by_date;
        for (std::size_t row = 0; row < count; ++row) {
            by_date[data.rows[row].date].push_back(row);
        }
        for (const auto& entry : by_date) {
            std::vector<double> values;
            values.reserve(entry.second.size());
            for (const std::size_t row : entry.second) {
                values.push_back(weights[row]);
            }
            const auto normalized = to_weights(values, false, false);
            for (std::size_t index = 0; index < entry.second.size();
                 ++index) {
                weights[entry.second[index]] = normalized[index];
            }
        }
    }
    return weights;
}

double student_t_two_sided_p_value(const double statistic, const double df) {
    if (std::isnan(statistic) || !(df > 0.0)) {
        return nan;
    }
    if (std::isinf(statistic)) {
        return 0.0;
    }
    const double x = df / (df + statistic * statistic);
    // Regularized incomplete beta I_x(df/2, 1/2).
    const auto log_beta = std::lgamma(df / 2.0) +
                          std::lgamma(0.5) - std::lgamma(df / 2.0 + 0.5);
    const auto beta_continued = [](const double a, const double b,
                                   const double value) {
        constexpr int max_iterations = 200;
        constexpr double epsilon = 3e-14;
        constexpr double tiny = 1e-300;
        const double qab = a + b;
        const double qap = a + 1.0;
        const double qam = a - 1.0;
        double c = 1.0;
        double d = 1.0 - qab * value / qap;
        if (std::abs(d) < tiny) {
            d = tiny;
        }
        d = 1.0 / d;
        double h = d;
        for (int iteration = 1; iteration <= max_iterations; ++iteration) {
            const int doubled = 2 * iteration;
            double aa = iteration * (b - iteration) * value /
                        ((qam + doubled) * (a + doubled));
            d = 1.0 + aa * d;
            if (std::abs(d) < tiny) {
                d = tiny;
            }
            c = 1.0 + aa / c;
            if (std::abs(c) < tiny) {
                c = tiny;
            }
            d = 1.0 / d;
            h *= d * c;
            aa = -(a + iteration) * (qab + iteration) * value /
                 ((a + doubled) * (qap + doubled));
            d = 1.0 + aa * d;
            if (std::abs(d) < tiny) {
                d = tiny;
            }
            c = 1.0 + aa / c;
            if (std::abs(c) < tiny) {
                c = tiny;
            }
            d = 1.0 / d;
            const double delta = d * c;
            h *= delta;
            if (std::abs(delta - 1.0) < epsilon) {
                break;
            }
        }
        return h;
    };
    const double a = df / 2.0;
    const double b = 0.5;
    double beta_value = 0.0;
    if (x <= 0.0) {
        beta_value = 0.0;
    } else if (x >= 1.0) {
        beta_value = 1.0;
    } else if (x < (a + 1.0) / (a + b + 2.0)) {
        beta_value = std::exp(a * std::log(x) + b * std::log1p(-x) -
                              log_beta) *
                     beta_continued(a, b, x) / a;
    } else {
        beta_value = 1.0 - std::exp(a * std::log(x) + b * std::log1p(-x) -
                                    log_beta) *
                              beta_continued(b, a, 1.0 - x) / b;
    }
    return std::clamp(beta_value, 0.0, 1.0);
}

}  // namespace

double student_t_two_sided_p(const double statistic, const double df) {
    return student_t_two_sided_p_value(statistic, df);
}

factor_ic_table factor_information_coefficient(
    const prepared_factor_data& factor_data, const bool group_adjust,
    const bool by_group) {
    const auto dates = unique_dates(factor_data);
    const std::size_t labels = factor_data.forward_labels.size();
    factor_ic_table table;
    table.dates = dates;
    table.labels = factor_data.forward_labels;
    table.values.assign(dates.size() * labels, nan);
    auto forwards = group_adjust
                        ? demean_returns(factor_data, true).values
                        : forward_matrix(factor_data);
    std::map<bucket_key, std::vector<std::size_t>> buckets;
    for (std::size_t row = 0; row < factor_data.rows.size(); ++row) {
        const std::string group =
            by_group ? factor_data.rows[row].group : "";
        if (by_group && !factor_data.rows[row].has_group) {
            throw factor_data_error{
                "factor_data must contain a group column when by_group=True"};
        }
        buckets[{factor_data.rows[row].date, group}].push_back(row);
    }
    std::map<timestamp, std::size_t> date_positions;
    for (std::size_t position = 0; position < dates.size(); ++position) {
        date_positions[dates[position]] = position;
    }
    // When by_group is true, aggregate rows across groups per date by
    // averaging their per-group ICs; otherwise a date has one bucket.
    std::vector<double> sums(dates.size() * labels, 0.0);
    std::vector<std::size_t> counts(dates.size() * labels, 0U);
    for (const auto& bucket : buckets) {
        std::vector<double> factors;
        factors.reserve(bucket.second.size());
        for (const std::size_t row : bucket.second) {
            factors.push_back(factor_data.rows[row].factor);
        }
        const std::size_t position = date_positions[bucket.first.first];
        for (std::size_t label = 0; label < labels; ++label) {
            std::vector<double> left;
            std::vector<double> right;
            for (std::size_t index = 0; index < bucket.second.size();
                 ++index) {
                const std::size_t row = bucket.second[index];
                if (std::isfinite(factors[index]) &&
                    std::isfinite(forwards[label][row])) {
                    left.push_back(factors[index]);
                    right.push_back(forwards[label][row]);
                }
            }
            if (left.size() < 2U) {
                continue;
            }
            const double value = spearman(left, right);
            if (std::isfinite(value)) {
                sums[position * labels + label] += value;
                ++counts[position * labels + label];
            }
        }
    }
    for (std::size_t index = 0; index < sums.size(); ++index) {
        if (counts[index] > 0U) {
            table.values[index] =
                sums[index] / static_cast<double>(counts[index]);
        }
    }
    return table;
}

factor_ic_summary mean_information_coefficient(
    const prepared_factor_data& factor_data, const bool group_adjust,
    const bool by_group) {
    const auto table =
        factor_information_coefficient(factor_data, group_adjust, by_group);
    factor_ic_summary summary;
    summary.labels = table.labels;
    const std::size_t labels = table.labels.size();
    summary.means.assign(labels, nan);
    for (std::size_t label = 0; label < labels; ++label) {
        double total = 0.0;
        std::size_t count = 0U;
        for (std::size_t date = 0; date < table.dates.size(); ++date) {
            const double value = table.values[date * labels + label];
            if (std::isfinite(value)) {
                total += value;
                ++count;
            }
        }
        if (count > 0U) {
            summary.means[label] = total / static_cast<double>(count);
        }
    }
    return summary;
}

std::vector<factor_weight_row> factor_weights(
    const prepared_factor_data& factor_data, const bool demeaned,
    const bool group_adjust, const bool equal_weight) {
    const auto weights = compute_weights(factor_data, demeaned, group_adjust,
                                         equal_weight);
    std::vector<factor_weight_row> rows;
    rows.reserve(factor_data.rows.size());
    for (std::size_t row = 0; row < factor_data.rows.size(); ++row) {
        rows.push_back({factor_data.rows[row].date,
                        factor_data.rows[row].asset, weights[row]});
    }
    return rows;
}

factor_return_table factor_returns(
    const prepared_factor_data& factor_data, const bool demeaned,
    const bool group_adjust, const bool equal_weight) {
    const auto weights = compute_weights(factor_data, demeaned, group_adjust,
                                         equal_weight);
    const auto dates = unique_dates(factor_data);
    const std::size_t labels = factor_data.forward_labels.size();
    factor_return_table table;
    table.dates = dates;
    table.labels = factor_data.forward_labels;
    table.values.assign(dates.size() * labels, 0.0);
    std::map<timestamp, std::size_t> date_positions;
    for (std::size_t position = 0; position < dates.size(); ++position) {
        date_positions[dates[position]] = position;
    }
    std::vector<std::vector<bool>> seen(dates.size(),
                                        std::vector<bool>(labels, false));
    for (std::size_t row = 0; row < factor_data.rows.size(); ++row) {
        const std::size_t position =
            date_positions[factor_data.rows[row].date];
        for (std::size_t label = 0; label < labels; ++label) {
            const double value = factor_data.rows[row].forward_returns[label];
            if (!std::isfinite(value) || !std::isfinite(weights[row])) {
                continue;
            }
            table.values[position * labels + label] +=
                value * weights[row];
            seen[position][label] = true;
        }
    }
    static_cast<void>(seen);
    return table;
}

std::vector<factor_alpha_beta_row> factor_alpha_beta(
    const prepared_factor_data& factor_data, const bool demeaned,
    const bool group_adjust, const bool equal_weight) {
    const auto portfolio =
        factor_returns(factor_data, demeaned, group_adjust, equal_weight);
    const auto dates = unique_dates(factor_data);
    const std::size_t labels = factor_data.forward_labels.size();
    // Universe mean per date and label.
    std::vector<std::vector<double>> universe(
        dates.size(), std::vector<double>(labels, nan));
    std::map<timestamp, std::size_t> date_positions;
    for (std::size_t position = 0; position < dates.size(); ++position) {
        date_positions[dates[position]] = position;
    }
    std::vector<std::vector<double>> totals(
        dates.size(), std::vector<double>(labels, 0.0));
    std::vector<std::vector<std::size_t>> counts(
        dates.size(), std::vector<std::size_t>(labels, 0U));
    for (const auto& row : factor_data.rows) {
        const std::size_t position = date_positions[row.date];
        for (std::size_t label = 0; label < labels; ++label) {
            if (std::isfinite(row.forward_returns[label])) {
                totals[position][label] += row.forward_returns[label];
                ++counts[position][label];
            }
        }
    }
    for (std::size_t date = 0; date < dates.size(); ++date) {
        for (std::size_t label = 0; label < labels; ++label) {
            if (counts[date][label] > 0U) {
                universe[date][label] =
                    totals[date][label] /
                    static_cast<double>(counts[date][label]);
            }
        }
    }
    std::vector<factor_alpha_beta_row> results;
    for (std::size_t label = 0; label < labels; ++label) {
        std::vector<double> market;
        std::vector<double> values;
        for (std::size_t date = 0; date < dates.size(); ++date) {
            const double x = universe[date][label];
            const double y = portfolio.values[date * labels + label];
            if (std::isfinite(x) && std::isfinite(y)) {
                market.push_back(x);
                values.push_back(y);
            }
        }
        factor_alpha_beta_row row{factor_data.forward_labels[label], nan, nan};
        if (market.size() >= 2U) {
            const double mean_x =
                std::accumulate(market.begin(), market.end(), 0.0) /
                static_cast<double>(market.size());
            const double mean_y =
                std::accumulate(values.begin(), values.end(), 0.0) /
                static_cast<double>(values.size());
            double covariance = 0.0;
            double variance = 0.0;
            for (std::size_t index = 0; index < market.size(); ++index) {
                const double x = market[index] - mean_x;
                covariance += x * (values[index] - mean_y);
                variance += x * x;
            }
            if (variance > 0.0) {
                row.beta = covariance / variance;
                const double intercept = mean_y - row.beta * mean_x;
                int period = 0;
                for (const char character :
                     factor_data.forward_labels[label]) {
                    if (character >= '0' && character <= '9') {
                        period = period * 10 + (character - '0');
                    } else {
                        break;
                    }
                }
                if (period > 0) {
                    const double annualization =
                        252.0 / static_cast<double>(period);
                    row.annual_alpha =
                        std::pow(1.0 + intercept, annualization) - 1.0;
                }
            }
        }
        results.push_back(std::move(row));
    }
    return results;
}

quantile_return_table mean_return_by_quantile(
    const prepared_factor_data& factor_data, const bool by_date,
    const bool by_group, const bool demeaned, const bool group_adjust) {
    if (by_date) {
        throw factor_data_error{
            "by_date=true is not available in the native quantile table"};
    }
    if (by_group) {
        throw factor_data_error{
            "by_group=true is not available in the native quantile table"};
    }
    const std::size_t labels = factor_data.forward_labels.size();
    auto forwards = group_adjust
                        ? demean_returns(factor_data, true).values
                        : (demeaned ? demean_returns(factor_data, false).values
                                    : forward_matrix(factor_data));
    std::set<int> quantile_values;
    for (const auto& row : factor_data.rows) {
        quantile_values.insert(row.quantile);
    }
    std::map<std::tuple<int, timestamp>,
             std::map<std::size_t, std::vector<double>>>
        per_date;
    for (std::size_t row = 0; row < factor_data.rows.size(); ++row) {
        for (std::size_t label = 0; label < labels; ++label) {
            if (std::isfinite(forwards[label][row])) {
                per_date[{factor_data.rows[row].quantile,
                          factor_data.rows[row].date}][label]
                    .push_back(forwards[label][row]);
            }
        }
    }
    quantile_return_table table;
    table.quantiles.assign(quantile_values.begin(), quantile_values.end());
    table.labels = factor_data.forward_labels;
    const std::size_t quantile_count = table.quantiles.size();
    table.means.assign(quantile_count * labels, nan);
    table.standard_errors.assign(quantile_count * labels, nan);
    for (std::size_t quantile_index = 0;
         quantile_index < quantile_count; ++quantile_index) {
        const int quantile = table.quantiles[quantile_index];
        for (std::size_t label = 0; label < labels; ++label) {
            std::vector<double> date_means;
            for (const auto& entry : per_date) {
                if (std::get<0>(entry.first) != quantile) {
                    continue;
                }
                const auto found = entry.second.find(label);
                if (found == entry.second.end() ||
                    found->second.empty()) {
                    continue;
                }
                const double total =
                    std::accumulate(found->second.begin(),
                                    found->second.end(), 0.0);
                date_means.push_back(
                    total / static_cast<double>(found->second.size()));
            }
            if (date_means.empty()) {
                continue;
            }
            const double mean =
                std::accumulate(date_means.begin(), date_means.end(), 0.0) /
                static_cast<double>(date_means.size());
            table.means[quantile_index * labels + label] = mean;
            if (date_means.size() > 1U) {
                double variance = 0.0;
                for (const double value : date_means) {
                    variance += (value - mean) * (value - mean);
                }
                variance /= static_cast<double>(date_means.size() - 1U);
                table.standard_errors[quantile_index * labels + label] =
                    std::sqrt(variance) /
                    std::sqrt(static_cast<double>(date_means.size()));
            }
        }
    }
    return table;
}

quantile_date_return_table mean_return_by_quantile_by_date(
    const prepared_factor_data& factor_data, const bool demeaned,
    const bool group_adjust) {
    const std::size_t labels = factor_data.forward_labels.size();
    auto forwards = group_adjust
                        ? demean_returns(factor_data, true).values
                        : (demeaned ? demean_returns(factor_data, false).values
                                    : forward_matrix(factor_data));
    quantile_date_return_table table;
    table.dates = unique_dates(factor_data);
    table.labels = factor_data.forward_labels;
    std::set<int> quantile_values;
    for (const auto& row : factor_data.rows) {
        quantile_values.insert(row.quantile);
    }
    table.quantiles.assign(quantile_values.begin(), quantile_values.end());
    const std::size_t date_count = table.dates.size();
    const std::size_t quantile_count = table.quantiles.size();
    table.means.assign(date_count * quantile_count * labels, nan);
    table.standard_errors.assign(date_count * quantile_count * labels, nan);

    std::map<timestamp, std::size_t> date_index;
    for (std::size_t index = 0; index < date_count; ++index) {
        date_index[table.dates[index]] = index;
    }
    std::map<int, std::size_t> quantile_index;
    for (std::size_t index = 0; index < quantile_count; ++index) {
        quantile_index[table.quantiles[index]] = index;
    }
    using cell = std::tuple<std::size_t, std::size_t, std::size_t>;
    std::map<cell, std::vector<double>> buckets;
    for (std::size_t row = 0; row < factor_data.rows.size(); ++row) {
        const auto date = date_index.find(factor_data.rows[row].date);
        const auto quantile =
            quantile_index.find(factor_data.rows[row].quantile);
        if (date == date_index.end() || quantile == quantile_index.end()) {
            continue;
        }
        for (std::size_t label = 0; label < labels; ++label) {
            if (std::isfinite(forwards[label][row])) {
                buckets[{date->second, quantile->second, label}]
                    .push_back(forwards[label][row]);
            }
        }
    }
    for (const auto& entry : buckets) {
        const auto& [date, quantile, label] = entry.first;
        const auto& values = entry.second;
        if (values.empty()) {
            continue;
        }
        const double mean =
            std::accumulate(values.begin(), values.end(), 0.0) /
            static_cast<double>(values.size());
        const std::size_t position =
            (date * quantile_count + quantile) * labels + label;
        table.means[position] = mean;
        if (values.size() > 1U) {
            double variance = 0.0;
            for (const double value : values) {
                variance += (value - mean) * (value - mean);
            }
            variance /= static_cast<double>(values.size() - 1U);
            table.standard_errors[position] =
                std::sqrt(variance) /
                std::sqrt(static_cast<double>(values.size()));
        }
    }
    return table;
}

quantile_spread_result mean_return_spread(
    const quantile_return_table& table, const int upper_quantile,
    const int lower_quantile) {
    const auto upper = std::find(table.quantiles.begin(),
                                 table.quantiles.end(), upper_quantile);
    const auto lower = std::find(table.quantiles.begin(),
                                 table.quantiles.end(), lower_quantile);
    if (upper == table.quantiles.end() || lower == table.quantiles.end()) {
        throw factor_data_error{
            "spread quantiles must both be present in the quantile table"};
    }
    const std::size_t upper_index =
        static_cast<std::size_t>(upper - table.quantiles.begin());
    const std::size_t lower_index =
        static_cast<std::size_t>(lower - table.quantiles.begin());
    const std::size_t labels = table.labels.size();
    quantile_spread_result result;
    result.labels = table.labels;
    result.spreads.assign(labels, nan);
    result.standard_errors.assign(labels, nan);
    for (std::size_t label = 0; label < labels; ++label) {
        const double upper_mean = table.means[upper_index * labels + label];
        const double lower_mean = table.means[lower_index * labels + label];
        result.spreads[label] = upper_mean - lower_mean;
        const double upper_error =
            table.standard_errors[upper_index * labels + label];
        const double lower_error =
            table.standard_errors[lower_index * labels + label];
        if (std::isfinite(upper_error) && std::isfinite(lower_error)) {
            result.standard_errors[label] =
                std::sqrt(upper_error * upper_error +
                          lower_error * lower_error);
        }
    }
    return result;
}

factor_series_result quantile_turnover(
    const prepared_factor_data& factor_data, const int quantile,
    const int period) {
    if (period <= 0) {
        throw factor_data_error{"period must be a positive integer"};
    }
    const auto dates = unique_dates(factor_data);
    std::map<timestamp, std::set<std::string>> names;
    for (const auto& row : factor_data.rows) {
        if (row.quantile == quantile) {
            names[row.date].insert(row.asset);
        }
    }
    factor_series_result result;
    result.dates = dates;
    result.values.assign(dates.size(), nan);
    for (std::size_t position = static_cast<std::size_t>(period);
         position < dates.size(); ++position) {
        const auto current = names.find(dates[position]);
        if (current == names.end() || current->second.empty()) {
            continue;
        }
        const auto previous = names.find(dates[position -
                                               static_cast<std::size_t>(period)]);
        std::size_t entering = 0U;
        for (const auto& asset : current->second) {
            if (previous == names.end() ||
                previous->second.find(asset) == previous->second.end()) {
                ++entering;
            }
        }
        result.values[position] =
            static_cast<double>(entering) /
            static_cast<double>(current->second.size());
    }
    return result;
}

factor_series_result factor_rank_autocorrelation(
    const prepared_factor_data& factor_data, const int period) {
    if (period <= 0) {
        throw factor_data_error{"period must be a positive integer"};
    }
    const auto dates = unique_dates(factor_data);
    std::map<timestamp, std::map<std::string, double>> ranks;
    std::map<timestamp, std::vector<std::string>> assets;
    for (const auto& date : dates) {
        std::vector<std::string> names;
        std::vector<double> values;
        for (const auto& row : factor_data.rows) {
            if (row.date == date && std::isfinite(row.factor)) {
                names.push_back(row.asset);
                values.push_back(row.factor);
            }
        }
        const auto ranked = average_ranks(values);
        for (std::size_t index = 0; index < names.size(); ++index) {
            ranks[date][names[index]] = ranked[index];
        }
        assets[date] = std::move(names);
    }
    factor_series_result result;
    result.dates = dates;
    result.values.assign(dates.size(), nan);
    for (std::size_t position = static_cast<std::size_t>(period);
         position < dates.size(); ++position) {
        const auto& current_date = dates[position];
        const auto& previous_date =
            dates[position - static_cast<std::size_t>(period)];
        std::vector<double> left;
        std::vector<double> right;
        for (const auto& asset : assets[current_date]) {
            const auto previous = ranks[previous_date].find(asset);
            if (previous == ranks[previous_date].end()) {
                continue;
            }
            left.push_back(ranks[current_date][asset]);
            right.push_back(previous->second);
        }
        result.values[position] = pearson(left, right);
    }
    return result;
}

double ic_mean(const std::span<const double> values) {
    double total = 0.0;
    std::size_t count = 0U;
    for (const double value : values) {
        if (std::isnan(value)) {
            continue;
        }
        if (!std::isfinite(value)) {
            throw factor_data_error{
                "IC observations must not contain infinite values"};
        }
        total += value;
        ++count;
    }
    return count == 0U ? nan : total / static_cast<double>(count);
}

double ic_t_stat(const std::span<const double> values) {
    std::vector<double> finite;
    for (const double value : values) {
        if (std::isnan(value)) {
            continue;
        }
        if (!std::isfinite(value)) {
            throw factor_data_error{
                "IC observations must not contain infinite values"};
        }
        finite.push_back(value);
    }
    if (finite.size() < 2U) {
        return nan;
    }
    const double mean =
        std::accumulate(finite.begin(), finite.end(), 0.0) /
        static_cast<double>(finite.size());
    double variance = 0.0;
    for (const double value : finite) {
        variance += (value - mean) * (value - mean);
    }
    variance /= static_cast<double>(finite.size() - 1U);
    const double standard_error =
        std::sqrt(variance) / std::sqrt(static_cast<double>(finite.size()));
    if (standard_error < 1e-15) {
        if (mean > 0.0) {
            return std::numeric_limits<double>::infinity();
        }
        if (mean < 0.0) {
            return -std::numeric_limits<double>::infinity();
        }
        return 0.0;
    }
    return mean / standard_error;
}

ic_interval ic_confidence_interval(const std::span<const double> values,
                                   const double z) {
    if (!(z > 0.0) || !std::isfinite(z)) {
        throw factor_data_error{
            "z must be a finite positive multiplier"};
    }
    std::vector<double> finite;
    for (const double value : values) {
        if (std::isnan(value)) {
            continue;
        }
        if (!std::isfinite(value)) {
            throw factor_data_error{
                "IC observations must not contain infinite values"};
        }
        finite.push_back(value);
    }
    if (finite.size() < 2U) {
        return {nan, nan};
    }
    const double mean =
        std::accumulate(finite.begin(), finite.end(), 0.0) /
        static_cast<double>(finite.size());
    double variance = 0.0;
    for (const double value : finite) {
        variance += (value - mean) * (value - mean);
    }
    variance /= static_cast<double>(finite.size() - 1U);
    const double standard_error =
        std::sqrt(variance) / std::sqrt(static_cast<double>(finite.size()));
    return {mean - z * standard_error, mean + z * standard_error};
}

fdr_result benjamini_hochberg(const std::span<const double> p_values,
                              const double alpha) {
    if (!std::isfinite(alpha) || !(alpha > 0.0 && alpha <= 1.0)) {
        throw factor_data_error{
            "alpha must be a finite probability in (0, 1]"};
    }
    fdr_result result;
    result.alpha = alpha;
    result.p_values.assign(p_values.begin(), p_values.end());
    for (const double value : p_values) {
        if (!std::isfinite(value) || value < 0.0 || value > 1.0) {
            throw factor_data_error{
                "p_values must be finite probabilities in [0, 1]"};
        }
    }
    const std::size_t count = p_values.size();
    result.adjusted_p_values.assign(count, nan);
    result.rejected.assign(count, false);
    if (count == 0U) {
        return result;
    }
    std::vector<std::size_t> order(count);
    std::iota(order.begin(), order.end(), 0U);
    std::stable_sort(order.begin(), order.end(),
                     [&p_values](const std::size_t left,
                                 const std::size_t right) {
                         return p_values[left] < p_values[right];
                     });
    std::vector<double> sorted(count, 0.0);
    for (std::size_t index = 0; index < count; ++index) {
        sorted[index] = p_values[order[index]];
    }
    std::vector<double> adjusted(count, 0.0);
    double running = 1.0;
    for (std::size_t position = count; position > 0U; --position) {
        const std::size_t index = position - 1U;
        const double value =
            static_cast<double>(count) * sorted[index] /
            static_cast<double>(position);
        running = std::min(running, value);
        adjusted[index] = std::clamp(running, 0.0, 1.0);
    }
    std::size_t last_rejected = 0U;
    bool any = false;
    for (std::size_t index = 0; index < count; ++index) {
        const double threshold =
            alpha * static_cast<double>(index + 1U) /
            static_cast<double>(count);
        if (sorted[index] <= threshold) {
            last_rejected = index;
            any = true;
        }
    }
    for (std::size_t index = 0; index < count; ++index) {
        result.adjusted_p_values[order[index]] = adjusted[index];
        result.rejected[order[index]] = any && index <= last_rejected;
    }
    return result;
}

ic_inference_result information_coefficient_inference(
    const factor_ic_table& information, const double alpha) {
    if (!std::isfinite(alpha) || !(alpha > 0.0 && alpha <= 1.0)) {
        throw factor_data_error{
            "alpha must be a finite probability in (0, 1]"};
    }
    ic_inference_result result;
    result.alpha = alpha;
    const std::size_t labels = information.labels.size();
    std::vector<double> testable_p;
    std::vector<std::size_t> testable_index;
    for (std::size_t label = 0; label < labels; ++label) {
        std::vector<double> values;
        for (std::size_t date = 0; date < information.dates.size(); ++date) {
            const double value = information.values[date * labels + label];
            if (std::isfinite(value)) {
                values.push_back(value);
            }
        }
        factor_hypothesis hypothesis;
        hypothesis.label = information.labels[label];
        hypothesis.n_observations = values.size();
        hypothesis.testable = values.size() >= 2U;
        if (!hypothesis.testable) {
            hypothesis.mean_ic = ic_mean(values);
            hypothesis.t_stat = nan;
            hypothesis.p_value = nan;
            hypothesis.adjusted_p_value = nan;
            result.hypotheses.push_back(hypothesis);
            continue;
        }
        hypothesis.mean_ic = ic_mean(values);
        hypothesis.t_stat = ic_t_stat(values);
        hypothesis.p_value = student_t_two_sided_p_value(
            hypothesis.t_stat,
            static_cast<double>(values.size() - 1U));
        testable_p.push_back(hypothesis.p_value);
        testable_index.push_back(label);
        result.hypotheses.push_back(hypothesis);
    }
    if (!testable_p.empty()) {
        const auto correction = benjamini_hochberg(testable_p, alpha);
        for (std::size_t index = 0; index < testable_index.size(); ++index) {
            auto& hypothesis = result.hypotheses[testable_index[index]];
            hypothesis.adjusted_p_value =
                correction.adjusted_p_values[index];
            hypothesis.rejected = correction.rejected[index];
        }
    }
    return result;
}

namespace {

std::vector<std::string> asset_columns(const dataframe& frame) {
    std::vector<std::string> names;
    for (const auto& info : frame.get_columns_info<double>()) {
        if (std::get<2>(info) == typeid(double)) {
            names.emplace_back(std::get<0>(info).c_str());
        }
    }
    return names;
}

double sample_standard_error(const std::vector<double>& values) {
    if (values.size() < 2U) {
        return nan;
    }
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                        static_cast<double>(values.size());
    double variance = 0.0;
    for (const double value : values) {
        variance += (value - mean) * (value - mean);
    }
    variance /= static_cast<double>(values.size() - 1U);
    return std::sqrt(variance) / std::sqrt(static_cast<double>(values.size()));
}

double newey_west_standard_error(const std::vector<double>& values,
                                 const std::size_t lags) {
    if (values.size() < 2U) {
        return nan;
    }
    const double mean = std::accumulate(values.begin(), values.end(), 0.0) /
                        static_cast<double>(values.size());
    std::vector<double> centered(values.size(), 0.0);
    for (std::size_t index = 0; index < values.size(); ++index) {
        centered[index] = values[index] - mean;
    }
    double long_run = 0.0;
    for (const double value : centered) {
        long_run += value * value;
    }
    for (std::size_t lag = 1; lag <= lags; ++lag) {
        const double weight =
            1.0 - static_cast<double>(lag) /
                      static_cast<double>(lags + 1U);
        double cross = 0.0;
        for (std::size_t index = lag; index < centered.size(); ++index) {
            cross += centered[index] * centered[index - lag];
        }
        long_run += 2.0 * weight * cross;
    }
    const double variance =
        long_run / static_cast<double>(values.size() *
                                       values.size());
    if (variance < 0.0) {
        if (std::abs(variance) <=
            std::numeric_limits<double>::epsilon()) {
            return 0.0;
        }
        throw factor_data_error{
            "Newey-West variance must not be negative"};
    }
    return std::sqrt(variance);
}

double mean_t_stat(const double mean, const double standard_error) {
    if (!std::isfinite(standard_error)) {
        return nan;
    }
    if (standard_error > 1e-15) {
        return mean / standard_error;
    }
    if (mean > 0.0) {
        return std::numeric_limits<double>::infinity();
    }
    if (mean < 0.0) {
        return -std::numeric_limits<double>::infinity();
    }
    return 0.0;
}

fama_macbeth_estimate make_estimate(const std::vector<double>& values,
                                    const bool newey_west,
                                    const std::size_t lags) {
    fama_macbeth_estimate estimate;
    if (values.empty()) {
        estimate.mean = nan;
        estimate.std_error = nan;
        estimate.t_stat = nan;
        return estimate;
    }
    estimate.mean =
        std::accumulate(values.begin(), values.end(), 0.0) /
        static_cast<double>(values.size());
    estimate.std_error =
        newey_west ? newey_west_standard_error(values, lags)
                   : sample_standard_error(values);
    estimate.t_stat = mean_t_stat(estimate.mean, estimate.std_error);
    return estimate;
}

}  // namespace

fama_macbeth_result fama_macbeth(const dataframe& returns,
                                 const dataframe& exposures,
                                 const fama_macbeth_covariance covariance,
                                 const std::size_t newey_west_lags) {
    const auto assets = asset_columns(returns);
    if (returns.get_index().empty() || assets.size() < 2U) {
        throw factor_data_error{
            "returns must contain at least two asset columns"};
    }
    if (exposures.get_index().empty()) {
        throw factor_data_error{"exposures must contain at least one row"};
    }
    if (std::adjacent_find(returns.get_index().begin(),
                           returns.get_index().end()) !=
        returns.get_index().end()) {
        throw factor_data_error{
            "returns and exposures indices must not contain duplicates"};
    }
    if (exposures.get_index().size() > 1U &&
        std::adjacent_find(exposures.get_index().begin(),
                           exposures.get_index().end()) !=
            exposures.get_index().end()) {
        throw factor_data_error{
            "returns and exposures indices must not contain duplicates"};
    }
    const auto exposure_assets = asset_columns(exposures);
    for (const auto& asset : assets) {
        if (std::find(exposure_assets.begin(), exposure_assets.end(),
                      asset) == exposure_assets.end()) {
            throw factor_data_error{
                "exposures are missing return asset columns"};
        }
    }
    if (covariance == fama_macbeth_covariance::newey_west &&
        !std::is_sorted(returns.get_index().begin(),
                        returns.get_index().end())) {
        throw factor_data_error{
            "newey-west covariance requires chronological returns index"};
    }
    std::map<timestamp, std::size_t> exposure_rows;
    for (std::size_t row = 0; row < exposures.get_index().size(); ++row) {
        exposure_rows[exposures.get_index()[row]] = row;
    }
    const bool static_exposure = exposures.get_index().size() == 1U;

    std::vector<double> slopes;
    std::vector<double> intercepts;
    const auto& return_index = returns.get_index();
    for (std::size_t position = 0; position < return_index.size();
         ++position) {
        const timestamp date = return_index[position];
        std::size_t exposure_row = 0U;
        if (!static_exposure) {
            const auto found = exposure_rows.find(date);
            if (found == exposure_rows.end()) {
                continue;
            }
            exposure_row = found->second;
        }
        std::vector<double> y_values;
        std::vector<double> x_values;
        for (const auto& asset : assets) {
            const auto& y = returns.get_column<double>(asset.c_str());
            const auto& x = exposures.get_column<double>(asset.c_str());
            const double y_value = y[position];
            const double x_value = x[exposure_row];
            if (std::isfinite(y_value) && std::isfinite(x_value)) {
                y_values.push_back(y_value);
                x_values.push_back(x_value);
            }
        }
        if (y_values.size() < 2U) {
            continue;
        }
        const double x_mean =
            std::accumulate(x_values.begin(), x_values.end(), 0.0) /
            static_cast<double>(x_values.size());
        const double y_mean =
            std::accumulate(y_values.begin(), y_values.end(), 0.0) /
            static_cast<double>(y_values.size());
        double xx = 0.0;
        double xy = 0.0;
        for (std::size_t index = 0; index < x_values.size(); ++index) {
            const double dx = x_values[index] - x_mean;
            xx += dx * dx;
            xy += dx * (y_values[index] - y_mean);
        }
        if (std::sqrt(xx / static_cast<double>(x_values.size())) < 1e-15) {
            continue;
        }
        const double slope = xy / xx;
        slopes.push_back(slope);
        intercepts.push_back(y_mean - slope * x_mean);
    }

    if (covariance == fama_macbeth_covariance::newey_west &&
        newey_west_lags >= slopes.size()) {
        throw factor_data_error{
            "newey_west_lags must be smaller than the fitted cross-section "
            "count"};
    }
    fama_macbeth_result result;
    result.n_cross_sections = slopes.size();
    result.covariance = covariance;
    result.newey_west_lags = newey_west_lags;
    const bool use_newey_west =
        covariance == fama_macbeth_covariance::newey_west;
    result.intercept =
        make_estimate(intercepts, use_newey_west, newey_west_lags);
    result.exposure = make_estimate(slopes, use_newey_west, newey_west_lags);
    return result;
}


namespace {

std::vector<std::string> factor_assets_at(const factor_series& factor,
                                          const timestamp date) {
    std::vector<std::string> assets;
    for (std::size_t row = 0; row < factor.dates.size(); ++row) {
        if (factor.dates[row] != date) {
            continue;
        }
        if (std::find(assets.begin(), assets.end(), factor.assets[row]) ==
            assets.end()) {
            assets.push_back(factor.assets[row]);
        }
    }
    return assets;
}

std::vector<double> wealth_series(const std::vector<double>& returns) {
    std::vector<double> wealth(returns.size(), 1.0);
    double growth = 1.0;
    for (std::size_t row = 0; row < returns.size(); ++row) {
        growth *= 1.0 + (std::isfinite(returns[row]) ? returns[row] : 0.0);
        wealth[row] = growth;
    }
    return wealth;
}

}  // namespace

event_window_result common_start_returns(
    const factor_series& factor, const dataframe& returns, const int before,
    const int after, const bool cumulative, const bool mean_by_date,
    const factor_series* demean_by) {
    if (before < 0 || after < 0) {
        throw factor_data_error{
            "before and after must be non-negative integers"};
    }
    const auto& index = returns.get_index();
    if (std::adjacent_find(index.begin(), index.end()) != index.end()) {
        throw factor_data_error{"returns index must be unique"};
    }
    const auto assets = asset_columns(returns);
    std::map<std::string, std::vector<double>> values;
    for (const auto& asset : assets) {
        const auto& column = returns.get_column<double>(asset.c_str());
        values[asset] =
            cumulative
                ? std::vector<double>(column.begin(), column.end())
                : wealth_series(std::vector<double>(column.begin(),
                                                    column.end()));
    }
    std::map<timestamp, std::size_t> positions;
    for (std::size_t row = 0; row < index.size(); ++row) {
        positions[index[row]] = row;
    }
    std::set<timestamp> event_dates_set;
    for (const auto date : factor.dates) {
        if (positions.contains(date)) {
            event_dates_set.insert(date);
        }
    }

    event_window_result result;
    result.before = before;
    result.after = after;
    result.event_dates.assign(event_dates_set.begin(), event_dates_set.end());
    std::set<int> offsets;
    std::set<std::string> labels;
    for (const timestamp date : result.event_dates) {
        const auto position = positions[date];
        const auto event_assets = factor_assets_at(factor, date);
        const auto demean_assets =
            demean_by != nullptr ? factor_assets_at(*demean_by, date)
                                 : std::vector<std::string>{};
        if (demean_by != nullptr && demean_assets.empty()) {
            throw factor_data_error{
                "demean_by has no assets at a factor event date"};
        }
        for (const auto& asset : event_assets) {
            if (!values.contains(asset)) {
                throw factor_data_error{
                    "returns do not contain factor assets"};
            }
            labels.insert(asset);
        }
        const std::size_t start =
            position > static_cast<std::size_t>(before)
                ? position - static_cast<std::size_t>(before)
                : 0U;
        const std::size_t stop = std::min(
            index.size(),
            position + static_cast<std::size_t>(after) + 1U);
        for (std::size_t row = start; row < stop; ++row) {
            const int offset =
                static_cast<int>(row) - static_cast<int>(position);
            offsets.insert(offset);
            double demean_mean = 0.0;
            std::size_t demean_count = 0U;
            if (demean_by != nullptr) {
                for (const auto& asset : demean_assets) {
                    const double value = values[asset][row];
                    if (std::isfinite(value)) {
                        demean_mean += value;
                        ++demean_count;
                    }
                }
                demean_mean /= static_cast<double>(demean_count);
            }
            if (mean_by_date) {
                double total = 0.0;
                std::size_t count = 0U;
                for (const auto& asset : event_assets) {
                    const double value = values[asset][row];
                    if (std::isfinite(value)) {
                        total += value;
                        ++count;
                    }
                }
                if (count > 0U) {
                    double mean = total / static_cast<double>(count);
                    if (demean_by != nullptr) {
                        mean -= demean_mean;
                    }
                    result.cells.push_back(
                        {offset, date, std::string{}, mean});
                }
            } else {
                for (const auto& asset : event_assets) {
                    const double value = values[asset][row];
                    if (std::isfinite(value)) {
                        result.cells.push_back(
                            {offset, date, asset,
                             demean_by != nullptr ? value - demean_mean
                                                  : value});
                    }
                }
            }
        }
    }
    result.offsets.assign(offsets.begin(), offsets.end());
    result.asset_labels.assign(labels.begin(), labels.end());
    return result;
}

std::vector<quantile_event_statistics>
average_cumulative_return_by_quantile(
    const prepared_factor_data& factor_data, const dataframe& returns,
    const int before, const int after, const bool demeaned) {
    if (before < 0 || after < 0) {
        throw factor_data_error{
            "before and after must be non-negative integers"};
    }
    std::set<int> quantiles;
    for (const auto& row : factor_data.rows) {
        quantiles.insert(row.quantile);
    }
    factor_series demean_source;
    for (const auto& row : factor_data.rows) {
        demean_source.dates.push_back(row.date);
        demean_source.assets.push_back(row.asset);
        demean_source.values.push_back(static_cast<double>(row.quantile));
    }
    std::vector<quantile_event_statistics> result;
    for (const int quantile : quantiles) {
        factor_series events;
        for (const auto& row : factor_data.rows) {
            if (row.quantile != quantile) {
                continue;
            }
            events.dates.push_back(row.date);
            events.assets.push_back(row.asset);
            events.values.push_back(static_cast<double>(quantile));
        }
        const auto window = common_start_returns(
            events, returns, before, after, true, true,
            demeaned ? &demean_source : nullptr);
        std::map<int, std::vector<double>> by_offset;
        for (const auto& cell : window.cells) {
            by_offset[cell.offset].push_back(cell.value);
        }
        quantile_event_statistics statistics;
        statistics.quantile = quantile;
        if (!by_offset.empty()) {
            const int first = by_offset.begin()->first;
            const int last = by_offset.rbegin()->first;
            for (int offset = first; offset <= last; ++offset) {
                statistics.offsets.push_back(offset);
                const auto found = by_offset.find(offset);
                if (found == by_offset.end() || found->second.empty()) {
                    statistics.means.push_back(nan);
                    statistics.standard_deviations.push_back(nan);
                    continue;
                }
                const auto& entries = found->second;
                const double mean =
                    std::accumulate(entries.begin(), entries.end(), 0.0) /
                    static_cast<double>(entries.size());
                statistics.means.push_back(mean);
                if (entries.size() > 1U) {
                    double variance = 0.0;
                    for (const double value : entries) {
                        variance += (value - mean) * (value - mean);
                    }
                    variance /= static_cast<double>(entries.size() - 1U);
                    statistics.standard_deviations.push_back(
                        std::sqrt(variance));
                } else {
                    statistics.standard_deviations.push_back(nan);
                }
            }
        }
        result.push_back(std::move(statistics));
    }
    return result;
}

}  // namespace fincore
