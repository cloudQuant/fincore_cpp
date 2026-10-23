#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/report/models.hpp>

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <variant>
#include <vector>

namespace fincore {

class operation_resolution_error : public std::runtime_error {
public:
    explicit operation_resolution_error(const std::string& operation_id)
        : std::runtime_error("unknown operation: " + operation_id) {}
};

struct runtime_value {
    using storage = std::variant<std::monostate, bool, std::int64_t, double,
                                 std::string, dataframe, report_document>;
    storage data;

    runtime_value() = default;
    runtime_value(bool value) : data(value) {}
    runtime_value(int value) : data(static_cast<std::int64_t>(value)) {}
    runtime_value(std::int64_t value) : data(value) {}
    runtime_value(double value) : data(value) {}
    runtime_value(std::string value) : data(std::move(value)) {}
    runtime_value(dataframe value) : data(std::move(value)) {}
    runtime_value(report_document value) : data(std::move(value)) {}
};

using runtime_inputs = std::map<std::string, runtime_value>;
using runtime_config = std::map<std::string, runtime_value>;

std::string runtime_value_digest(const runtime_value& value);

struct operation_spec {
    std::string operation_id;
    std::string capability_id;
    std::string domain;
    std::function<runtime_value(const runtime_inputs&)> callable;
    bool deterministic = true;
    std::string rng_policy = "none";
    std::optional<std::string> semantic_mode;
    std::optional<std::string> mode_approval;
    std::map<std::string, std::string> provenance;
};

class operation_catalog {
  public:
    explicit operation_catalog(std::vector<operation_spec> operations);

    const std::vector<operation_spec>& operations() const { return operations_; }
    const std::string& digest() const { return digest_; }
    const operation_spec& resolve(const std::string& operation_id) const;
    std::vector<std::string> operation_ids() const;

  private:
    std::vector<operation_spec> operations_;
    std::map<std::string, std::size_t> by_id_;
    std::string digest_;
};

struct operation_request {
    std::string operation_id;
    runtime_inputs inputs;
    runtime_config config;
    std::string input_digest;
    std::string config_digest;

    operation_request(std::string operation_id, runtime_inputs inputs,
                      runtime_config config = {});
};

struct execution_plan {
    std::string catalog_digest;
    std::vector<operation_request> requests;
};

execution_plan plan(const std::vector<operation_request>& requests,
                    const operation_catalog& catalog);

struct runtime_result {
    runtime_value value;
    std::map<std::string, std::string> metadata;
};

runtime_result run(const std::string& operation_id,
                   const runtime_inputs& inputs,
                   const runtime_config& config,
                   const operation_catalog& catalog);

std::vector<runtime_result> batch(const execution_plan& plan,
                                  const operation_catalog& catalog);

class analysis_snapshot {
  public:
    analysis_snapshot() = default;
    explicit analysis_snapshot(runtime_inputs inputs);
    const runtime_inputs& materialize() const { return inputs_; }
    const std::string& digest() const { return digest_; }

  private:
    runtime_inputs inputs_;
    std::string digest_;
};

class analysis_session {
  public:
    explicit analysis_session(const operation_catalog& catalog);
    runtime_result run(const std::string& operation_id,
                       const runtime_inputs& inputs,
                       const runtime_config& config = {});
    void clear();

  private:
    const operation_catalog& catalog_;
    std::map<std::string, runtime_result> cache_;
};

struct artifact_bundle {
    std::map<std::string, std::string> artifacts;
    std::map<std::string, std::string> metadata;
};

operation_catalog builtin_catalog();

}  // namespace fincore
