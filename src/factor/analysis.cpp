#include <fincore/factor/analysis.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/factor/exceptions.hpp>

#include <cstdint>
#include <cstring>
#include <set>
#include <string>
#include <vector>

namespace fincore {
namespace {

std::string fingerprint_of(const prepared_factor_data& data,
                           const factor_analysis_config& config) {
    std::vector<std::uint8_t> payload;
    const auto append_u64 = [&payload](const std::uint64_t value) {
        for (int shift = 56; shift >= 0; shift -= 8) {
            payload.push_back(
                static_cast<std::uint8_t>((value >> shift) & 0xffU));
        }
    };
    const auto append_double = [&append_u64](const double value) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, &value, sizeof(bits));
        append_u64(bits);
    };
    const auto append_text = [&payload](const std::string& text) {
        for (const char character : text) {
            payload.push_back(static_cast<std::uint8_t>(character));
        }
        payload.push_back(0U);
    };
    append_u64(config.long_short ? 1U : 0U);
    append_u64(config.group_neutral ? 1U : 0U);
    append_u64(config.equal_weight ? 1U : 0U);
    append_u64(data.forward_labels.size());
    for (const auto& label : data.forward_labels) {
        append_text(label);
    }
    append_u64(data.rows.size());
    for (const auto& row : data.rows) {
        append_u64(static_cast<std::uint64_t>(row.date));
        append_text(row.asset);
        append_double(row.factor);
        append_u64(static_cast<std::uint64_t>(row.quantile));
        append_text(row.has_group ? row.group : std::string{});
        for (const double value : row.forward_returns) {
            append_double(value);
        }
    }
    return detail::sha256_hex(payload);
}

}  // namespace

factor_analysis_model analyze_factor(
    const prepared_factor_data& factor_data,
    const factor_analysis_config& config) {
    if (factor_data.rows.empty()) {
        throw factor_data_error{
            "factor_data must contain at least one prepared observation"};
    }
    factor_analysis_model model;
    model.factor_data = factor_data;
    model.forward_periods = factor_data.forward_labels;
    model.information_coefficient =
        factor_information_coefficient(factor_data);
    model.mean_information_coefficient =
        mean_information_coefficient(factor_data);
    model.factor_returns = factor_returns(
        factor_data, config.long_short, config.group_neutral,
        config.equal_weight);
    model.factor_weights = factor_weights(
        factor_data, config.long_short, config.group_neutral,
        config.equal_weight);
    model.mean_returns_by_quantile = mean_return_by_quantile(
        factor_data, false, false, config.long_short,
        config.group_neutral);
    model.alpha_beta = factor_alpha_beta(
        factor_data, config.long_short, config.group_neutral,
        config.equal_weight);
    std::set<int> quantiles;
    for (const auto& row : factor_data.rows) {
        quantiles.insert(row.quantile);
    }
    for (const int quantile : quantiles) {
        model.quantile_turnover.push_back(
            quantile_turnover(factor_data, quantile, 1));
    }
    model.rank_autocorrelation =
        factor_rank_autocorrelation(factor_data, 1);
    model.result_fingerprint = fingerprint_of(factor_data, config);
    return model;
}

}  // namespace fincore
