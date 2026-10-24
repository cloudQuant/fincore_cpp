#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/extensions/snapshot.hpp>
#include <fincore/report/models.hpp>
#include <fincore/report/portfolio.hpp>
#include <fincore/risk/report.hpp>
#include <fincore/risk/walk_forward.hpp>

#include <cstddef>
#include <cstdint>
#include <filesystem>
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
                                 std::string, dataframe, report_document,
                                 risk_model_spec, walk_forward_var_result,
                                 risk_validation_report,
                                 std::vector<portfolio_transaction>>;
    storage data;

    runtime_value() = default;
    runtime_value(bool value) : data(value) {}
    runtime_value(int value) : data(static_cast<std::int64_t>(value)) {}
    runtime_value(std::int64_t value) : data(value) {}
    runtime_value(double value) : data(value) {}
    runtime_value(std::string value) : data(std::move(value)) {}
    runtime_value(dataframe value) : data(std::move(value)) {}
    runtime_value(report_document value) : data(std::move(value)) {}
    runtime_value(risk_model_spec value) : data(std::move(value)) {}
    runtime_value(walk_forward_var_result value) : data(std::move(value)) {}
    runtime_value(risk_validation_report value) : data(std::move(value)) {}
    runtime_value(std::vector<portfolio_transaction> value)
        : data(std::move(value)) {}
};

using runtime_inputs = std::map<std::string, runtime_value>;
using runtime_config = std::map<std::string, runtime_value>;

// DataFrame digests include index values, column names, types and contents for
// bool, character, integral, float, double and std::string columns. Other
// column types are rejected so session cache keys never omit input data.
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

    // Stable native implementation identity, derived from the declared kernel
    // symbol when present (the native analogue of Python module:qualname).
    std::string implementation_fingerprint() const;
};

using operations_provider = std::function<std::vector<operation_spec>()>;
operations_provider make_operations_provider(
    std::vector<operation_spec> declarations);

class operation_catalog {
  public:
    explicit operation_catalog(
        std::vector<operation_spec> operations,
        std::optional<std::string> extension_digest = std::nullopt);

    const std::vector<operation_spec>& operations() const { return operations_; }
    const std::string& digest() const { return digest_; }
    const operation_spec& resolve(const std::string& operation_id) const;
    std::vector<std::string> operation_ids() const;

    // The pinned extension snapshot, or nullopt when this catalog is not
    // bound to extensions (the source property returns None).
    const std::optional<extensions::extension_snapshot>& extension_snapshot()
        const noexcept {
        return extension_snapshot_;
    }

    // Bind one immutable extension snapshot to a new catalog. The caller
    // supplies native callables because the extension ABI carries metadata,
    // not callables; a missing callable is a contract error, never a
    // placeholder. Binding an already-bound catalog is rejected.
    operation_catalog with_extensions(
        const extensions::extension_snapshot& snapshot,
        const std::map<std::string,
                       std::function<runtime_value(const runtime_inputs&)>>&
            callables) const;

  private:
    std::vector<operation_spec> operations_;
    std::map<std::string, std::size_t> by_id_;
    std::string digest_;
    std::optional<extensions::extension_snapshot> extension_snapshot_;
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

// Config is recorded in result metadata only; operation callables receive
// runtime inputs and never receive config.
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
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    [[nodiscard]] std::size_t cache_entries() const noexcept {
        return cache_.size();
    }
    [[nodiscard]] const std::string& catalog_digest() const noexcept {
        return catalog_.digest();
    }
    runtime_result run(const std::string& operation_id,
                       const runtime_inputs& inputs,
                       const runtime_config& config = {});
    // Clear entries without changing whether the session is open or closed.
    void clear();
    void close() noexcept;

  private:
    operation_catalog catalog_;
    std::map<std::string, runtime_result> cache_;
    bool closed_ = false;
};

struct artifact_descriptor {
    // MIME type used by consumers of this artifact. Legacy artifacts without
    // an explicit descriptor are recorded as application/octet-stream.
    std::string media_type = "application/octet-stream";
    std::map<std::string, std::string> provenance;
};

struct artifact_bundle {
    std::map<std::string, std::string> artifacts;
    std::map<std::string, std::string> metadata;
    // Optional per-artifact metadata keyed by the exact artifact path. Every
    // descriptor key must name an entry in artifacts. The manifest records
    // the media type, byte length, SHA-256 of the actual artifact bytes, and
    // provenance for each artifact; omitted descriptors use the legacy
    // octet-stream/empty-provenance defaults.
    std::map<std::string, artifact_descriptor> descriptors;
    // Native lifecycle seal. Artifacts are owning byte strings, so close has
    // no external resources to release; it prevents further registration.
    bool is_closed = false;

    void add(std::string name, std::string bytes,
             artifact_descriptor descriptor = {});
    void close() noexcept { is_closed = true; }
    [[nodiscard]] bool closed() const noexcept { return is_closed; }
};

// Write a bundle into a new output directory. Existing output directories,
// unsafe artifact paths and symlink parents are rejected. The bundle metadata
// and artifact names are preserved in `.fincore-manifest.json`; per-file
// descriptors are validated before any output is created.
void write_artifact_bundle(const artifact_bundle& bundle,
                           const std::filesystem::path& output_directory);

// Builtin report operation inputs follow the pinned source IDs. The
// walk-forward operation requires `returns` (dataframe) and `spec`
// (risk_model_spec). Risk validation requires `result`
// (walk_forward_var_result) and accepts optional `index_name` (string). Risk
// document construction requires `report` (risk_validation_report) and accepts
// optional `title` (string). Portfolio report construction requires `returns`
// (dataframe) and accepts optional `benchmark`, `positions` (dataframe),
// `transactions` (vector<portfolio_transaction>), `rolling_window` (integer),
// and `title` (string). These operations ignore config values and do not
// forward them to domain functions.
operation_catalog builtin_catalog();

}  // namespace fincore
