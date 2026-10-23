#include <fincore/runtime/runtime.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>
#include <fincore/risk/backtesting.hpp>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <span>
#include <typeinfo>
#include <utility>
#include <vector>

namespace fincore {
namespace {

std::string canonical_operation(const operation_spec& spec) {
    std::string text;
    text += spec.operation_id + "\n";
    text += spec.capability_id + "\n";
    text += spec.domain + "\n";
    text += spec.deterministic ? "1" : "0";
    text += "\n" + spec.rng_policy + "\n";
    text += spec.semantic_mode.value_or("") + "\n";
    text += spec.mode_approval.value_or("") + "\n";
    for (const auto& entry : spec.provenance) {
        text += entry.first + "=" + entry.second + "\n";
    }
    return text;
}

std::string digest_text(const std::string& text) {
    return detail::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

const double& require_number(const runtime_inputs& inputs,
                             const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<double>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a number: " + name};
    }
    return *value;
}

const std::int64_t& require_integer(const runtime_inputs& inputs,
                                    const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<std::int64_t>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be an integer: " + name};
    }
    return *value;
}

const dataframe& require_frame(const runtime_inputs& inputs,
                               const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<dataframe>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a dataframe: " + name};
    }
    return *value;
}

}  // namespace

std::string runtime_value_digest(const runtime_value& value) {
    std::string text;
    if (std::holds_alternative<std::monostate>(value.data)) {
        text = "null";
    } else if (const auto* item = std::get_if<bool>(&value.data)) {
        text = *item ? "b1" : "b0";
    } else if (const auto* item = std::get_if<std::int64_t>(&value.data)) {
        text = "i" + std::to_string(*item);
    } else if (const auto* item = std::get_if<double>(&value.data)) {
        std::uint64_t bits = 0;
        std::memcpy(&bits, item, sizeof(bits));
        text = "f" + std::to_string(bits);
    } else if (const auto* item = std::get_if<std::string>(&value.data)) {
        text = "s" + std::to_string(item->size()) + ":" + *item;
    } else if (const auto* frame = std::get_if<dataframe>(&value.data)) {
        text = "d" + std::to_string(frame->get_index().size()) + ":";
        for (const auto& info : frame->get_columns_info<double>()) {
            if (std::get<2>(info) != typeid(double)) {
                continue;
            }
            const std::string name{std::get<0>(info).c_str()};
            text += name + "=";
            const auto& column = frame->get_column<double>(name.c_str());
            for (const double entry : column) {
                std::uint64_t bits = 0;
                std::memcpy(&bits, &entry, sizeof(bits));
                text += std::to_string(bits) + ",";
            }
            text += ";";
        }
        for (const timestamp date : frame->get_index()) {
            text += std::to_string(date) + ",";
        }
    } else if (const auto* document =
                   std::get_if<report_document>(&value.data)) {
        text = "r" + document->to_json();
    }
    return digest_text(text);
}

operation_catalog::operation_catalog(std::vector<operation_spec> operations) {
    std::set<std::string> seen_ids;
    std::map<std::string, const operation_spec*> by_capability;
    for (auto& spec : operations) {
        if (spec.operation_id.empty() || spec.capability_id.empty() ||
            spec.domain.empty() || spec.rng_policy.empty()) {
            throw input_error{
                "operation identifiers and rng_policy must be non-empty"};
        }
        if (!spec.callable) {
            throw input_error{"operation callable must be set"};
        }
        if (spec.semantic_mode.has_value() !=
            spec.mode_approval.has_value()) {
            throw input_error{
                "semantic_mode and mode_approval must be provided together"};
        }
        if (!seen_ids.insert(spec.operation_id).second) {
            throw input_error{
                "duplicate operation_id: " + spec.operation_id};
        }
        const auto previous = by_capability.find(spec.capability_id);
        if (previous != by_capability.end()) {
            if (previous->second->semantic_mode.has_value() !=
                    spec.semantic_mode.has_value() ||
                previous->second->semantic_mode == spec.semantic_mode) {
                throw input_error{
                    "multiple operation IDs for one leaf capability require "
                    "distinct semantic modes: " +
                    spec.capability_id};
            }
        }
        by_capability.emplace(spec.capability_id, &spec);
    }
    std::stable_sort(operations.begin(), operations.end(),
                     [](const operation_spec& left,
                        const operation_spec& right) {
                         return left.operation_id < right.operation_id;
                     });
    operations_ = std::move(operations);
    for (std::size_t index = 0; index < operations_.size(); ++index) {
        by_id_[operations_[index].operation_id] = index;
    }
    std::string payload;
    for (const auto& spec : operations_) {
        payload += canonical_operation(spec) + "--\n";
    }
    digest_ = digest_text(payload);
}

const operation_spec& operation_catalog::resolve(
    const std::string& operation_id) const {
    const auto found = by_id_.find(operation_id);
    if (found == by_id_.end()) {
        throw operation_resolution_error(operation_id);
    }
    return operations_[found->second];
}

std::vector<std::string> operation_catalog::operation_ids() const {
    std::vector<std::string> ids;
    ids.reserve(operations_.size());
    for (const auto& spec : operations_) {
        ids.push_back(spec.operation_id);
    }
    return ids;
}

operation_request::operation_request(std::string operation_id,
                                     runtime_inputs inputs,
                                     runtime_config config)
    : operation_id(std::move(operation_id)),
      inputs(std::move(inputs)),
      config(std::move(config)) {
    if (this->operation_id.empty()) {
        throw input_error{"operation_id must be a non-empty string"};
    }
    std::string input_payload;
    for (const auto& entry : this->inputs) {
        input_payload += entry.first + ":" + runtime_value_digest(entry.second) +
                         ";";
    }
    input_digest = digest_text(input_payload);
    std::string config_payload;
    for (const auto& entry : this->config) {
        config_payload += entry.first + ":" + runtime_value_digest(entry.second) +
                          ";";
    }
    config_digest = digest_text(config_payload);
}

execution_plan plan(const std::vector<operation_request>& requests,
                    const operation_catalog& catalog) {
    for (const auto& request : requests) {
        catalog.resolve(request.operation_id);
    }
    return {catalog.digest(), requests};
}

runtime_result run(const std::string& operation_id,
                   const runtime_inputs& inputs,
                   const runtime_config& config,
                   const operation_catalog& catalog) {
    const auto& spec = catalog.resolve(operation_id);
    const operation_request snapshot(operation_id, inputs, config);
    const runtime_value value = spec.callable(snapshot.inputs);
    runtime_result result;
    result.value = value;
    result.metadata = {
        {"operation_id", spec.operation_id},
        {"capability_id", spec.capability_id},
        {"domain", spec.domain},
        {"input_digest", snapshot.input_digest},
        {"config_digest", snapshot.config_digest},
        {"catalog_digest", catalog.digest()},
        {"deterministic", spec.deterministic ? "true" : "false"},
        {"rng_policy", spec.rng_policy},
        {"config_recorded", snapshot.config.empty() ? "false" : "true"},
        {"cache", "disabled"}};
    return result;
}

std::vector<runtime_result> batch(const execution_plan& plan_value,
                                  const operation_catalog& catalog) {
    if (plan_value.catalog_digest != catalog.digest()) {
        throw input_error{
            "execution plan was created for another catalog snapshot"};
    }
    std::vector<runtime_result> results;
    results.reserve(plan_value.requests.size());
    for (const auto& request : plan_value.requests) {
        results.push_back(
            run(request.operation_id, request.inputs, request.config, catalog));
    }
    return results;
}

analysis_snapshot::analysis_snapshot(runtime_inputs inputs)
    : inputs_(std::move(inputs)) {
    std::string payload;
    for (const auto& entry : inputs_) {
        payload += entry.first + ":" + runtime_value_digest(entry.second) + ";";
    }
    digest_ = digest_text(payload);
}

analysis_session::analysis_session(const operation_catalog& catalog)
    : catalog_(catalog) {}

runtime_result analysis_session::run(const std::string& operation_id,
                                     const runtime_inputs& inputs,
                                     const runtime_config& config) {
    const operation_request request(operation_id, inputs, config);
    const std::string key = operation_id + "|" + request.input_digest + "|" +
                            request.config_digest;
    const auto cached = cache_.find(key);
    if (cached != cache_.end()) {
        runtime_result copy = cached->second;
        copy.metadata["cache"] = "hit";
        return copy;
    }
    runtime_result result = fincore::run(operation_id, inputs, config, catalog_);
    result.metadata["cache"] = "miss";
    cache_.emplace(key, result);
    return result;
}

void analysis_session::clear() { cache_.clear(); }

operation_catalog builtin_catalog() {
    std::vector<operation_spec> operations;
    const auto add = [&operations](std::string operation_id,
                                   std::string domain, std::string kernel,
                                   std::function<runtime_value(
                                       const runtime_inputs&)>
                                       callable) {
        operations.push_back(
            {operation_id, operation_id, domain, std::move(callable), true,
             "none", std::nullopt, std::nullopt,
             {{"owner", domain}, {"kernel_symbol", std::move(kernel)}}});
    };
    add("metrics.alpha_beta.alpha", "metrics", "alpha",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                alpha(require_frame(inputs, "returns"),
                      require_frame(inputs, "benchmark")));
        });
    add("metrics.alpha_beta.beta", "metrics", "beta",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                beta(require_frame(inputs, "returns"),
                     require_frame(inputs, "benchmark")));
        });
    add("metrics.drawdown.max_drawdown", "metrics", "max_drawdown",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                max_drawdown(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.calmar_ratio", "metrics", "calmar_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                calmar_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.information_ratio", "metrics", "information_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                information_ratio(require_frame(inputs, "returns"),
                                  require_frame(inputs, "benchmark")));
        });
    add("metrics.ratios.mar_ratio", "metrics", "mar_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                mar_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.omega_ratio", "metrics", "omega_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                omega_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.sharpe_ratio", "metrics", "sharpe_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                sharpe_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.sortino_ratio", "metrics", "sortino_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                sortino_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.up_down_capture", "metrics", "up_down_capture",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                up_down_capture(require_frame(inputs, "returns"),
                                require_frame(inputs, "benchmark")));
        });
    add("metrics.returns.cum_returns", "metrics", "cumulative_returns",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                cumulative_returns(require_frame(inputs, "returns")));
        });
    add("metrics.returns.cum_returns_final", "metrics", "cumulative_return",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                cumulative_return(require_frame(inputs, "returns")));
        });
    add("metrics.risk.annual_volatility", "metrics", "annual_volatility",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                annual_volatility(require_frame(inputs, "returns")));
        });
    add("metrics.risk.conditional_value_at_risk", "metrics",
        "conditional_value_at_risk",
        [](const runtime_inputs& inputs) {
            return runtime_value(conditional_value_at_risk(
                require_frame(inputs, "returns")));
        });
    add("metrics.risk.tail_ratio", "metrics", "tail_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                tail_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.risk.tracking_error", "metrics", "tracking_error",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                tracking_error(require_frame(inputs, "returns"),
                               require_frame(inputs, "benchmark")));
        });
    add("metrics.yearly.annual_return", "metrics", "annual_return",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                annual_return(require_frame(inputs, "returns")));
        });
    add("risk.backtesting.kupiec_lr", "risk", "kupiec_lr",
        [](const runtime_inputs& inputs) {
            const auto& observations = require_integer(inputs, "observations");
            const auto& exceptions = require_integer(inputs, "exceptions");
            const double confidence =
                require_number(inputs, "confidence_level");
            return runtime_value(kupiec_lr(
                static_cast<std::size_t>(observations),
                static_cast<std::size_t>(exceptions), confidence));
        });
    add("risk.calibration.basel_traffic_light", "risk",
        "basel_traffic_light",
        [](const runtime_inputs& inputs) {
            const auto& exceptions = require_integer(inputs, "exceptions");
            const auto& observations = require_integer(inputs, "observations");
            const double confidence =
                require_number(inputs, "confidence_level");
            return runtime_value(basel_traffic_light(
                static_cast<std::size_t>(exceptions),
                static_cast<std::size_t>(observations), confidence));
        });
    add("risk.calibration.expected_exception_count", "risk",
        "expected_exception_count",
        [](const runtime_inputs& inputs) {
            const auto& observations = require_integer(inputs, "observations");
            const double confidence =
                require_number(inputs, "confidence_level");
            return runtime_value(expected_exception_count(
                static_cast<std::size_t>(observations), confidence));
        });
    add("metrics.drawdown.max_drawdown_days", "metrics", "max_drawdown_days",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                max_drawdown_days(require_frame(inputs, "returns")));
        });
    add("metrics.drawdown.max_drawdown_months", "metrics",
        "max_drawdown_months",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                max_drawdown_months(require_frame(inputs, "returns")));
        });
    add("metrics.drawdown.max_drawdown_weeks", "metrics",
        "max_drawdown_weeks",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                max_drawdown_weeks(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.adjusted_sharpe_ratio", "metrics",
        "adjusted_sharpe_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                adjusted_sharpe_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.burke_ratio", "metrics", "burke_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                burke_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.ratios.conditional_sharpe_ratio", "metrics",
        "conditional_sharpe_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(conditional_sharpe_ratio(
                require_frame(inputs, "returns")));
        });
    add("metrics.ratios.m_squared", "metrics", "m_squared",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                m_squared(require_frame(inputs, "returns"),
                          require_frame(inputs, "benchmark")));
        });
    add("metrics.ratios.sterling_ratio", "metrics", "sterling_ratio",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                sterling_ratio(require_frame(inputs, "returns")));
        });
    add("metrics.risk.beta_fragility_heuristic", "metrics",
        "beta_fragility_heuristic",
        [](const runtime_inputs& inputs) {
            return runtime_value(beta_fragility_heuristic(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark")));
        });
    add("metrics.risk.downside_risk", "metrics", "downside_risk",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                downside_risk(require_frame(inputs, "returns")));
        });
    add("metrics.risk.residual_risk", "metrics", "residual_risk",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                residual_risk(require_frame(inputs, "returns"),
                              require_frame(inputs, "benchmark")));
        });
    add("metrics.stats.hurst_exponent", "metrics", "hurst_exponent",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                hurst_exponent(require_frame(inputs, "returns")));
        });
    add("metrics.stats.kurtosis", "metrics", "kurtosis",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                kurtosis(require_frame(inputs, "returns")));
        });
    add("metrics.stats.loss_rate", "metrics", "loss_rate",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                loss_rate(require_frame(inputs, "returns")));
        });
    add("metrics.stats.r_cubed_turtle", "metrics", "r_cubed_turtle",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                r_cubed_turtle(require_frame(inputs, "returns")));
        });
    add("metrics.stats.skewness", "metrics", "skewness",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                skewness(require_frame(inputs, "returns")));
        });
    add("metrics.stats.stutzer_index", "metrics", "stutzer_index",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                stutzer_index(require_frame(inputs, "returns")));
        });
    add("metrics.stats.win_rate", "metrics", "win_rate",
        [](const runtime_inputs& inputs) {
            return runtime_value(
                win_rate(require_frame(inputs, "returns")));
        });
    return operation_catalog(std::move(operations));
}

}  // namespace fincore
