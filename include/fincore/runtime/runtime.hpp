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

/// Reports that a catalog lookup found no matching operation identifier.
class operation_resolution_error : public std::runtime_error {
public:
    /// Constructs the error with a message naming `operation_id`.
    explicit operation_resolution_error(const std::string& operation_id)
        : std::runtime_error("unknown operation: " + operation_id) {}
};

/// Owning dynamically typed value passed through the runtime layer.
struct runtime_value {
    /// Supported alternatives; `std::monostate` represents a null value.
    using storage = std::variant<std::monostate, bool, std::int64_t, double,
                                 std::string, dataframe, report_document,
                                 risk_model_spec, walk_forward_var_result,
                                 risk_validation_report,
                                 std::vector<portfolio_transaction>>;
    /// Active stored alternative.
    storage data;

    /// Constructs a null value.
    runtime_value() = default;
    /// Constructs a boolean value.
    runtime_value(bool value) : data(value) {}
    /// Constructs a 64-bit integer value from `value`.
    runtime_value(int value) : data(static_cast<std::int64_t>(value)) {}
    /// Constructs a 64-bit integer value.
    runtime_value(std::int64_t value) : data(value) {}
    /// Constructs a double value.
    runtime_value(double value) : data(value) {}
    /// Constructs a string value.
    runtime_value(std::string value) : data(std::move(value)) {}
    /// Constructs a dataframe value.
    runtime_value(dataframe value) : data(std::move(value)) {}
    /// Constructs a report document value.
    runtime_value(report_document value) : data(std::move(value)) {}
    /// Constructs a risk model specification value.
    runtime_value(risk_model_spec value) : data(std::move(value)) {}
    /// Constructs a walk-forward VaR result value.
    runtime_value(walk_forward_var_result value) : data(std::move(value)) {}
    /// Constructs a risk validation report value.
    runtime_value(risk_validation_report value) : data(std::move(value)) {}
    /// Constructs a portfolio transaction list value.
    runtime_value(std::vector<portfolio_transaction> value)
        : data(std::move(value)) {}
};

/// Named runtime inputs keyed by non-empty input name.
using runtime_inputs = std::map<std::string, runtime_value>;
/// Named runtime configuration keyed by non-empty config name.
using runtime_config = std::map<std::string, runtime_value>;

/// Returns a deterministic SHA-256 digest of `value`.
///
/// DataFrame digests include index values, column names, types and contents
/// for bool, character, integral, float, double and std::string columns. Other
/// column types are rejected so session cache keys never omit input data.
/// Throws `input_error` for an unsupported dataframe column type.
std::string runtime_value_digest(const runtime_value& value);

/// Declaration of one catalog operation and its native callable.
struct operation_spec {
    /// Unique catalog identifier.
    std::string operation_id;
    /// Leaf capability identifier shared by mode variants.
    std::string capability_id;
    /// Source domain, such as `metrics` or `report`.
    std::string domain;
    /// Native function invoked with validated runtime inputs.
    std::function<runtime_value(const runtime_inputs&)> callable;
    /// Whether the operation result may be cached by an analysis session.
    bool deterministic = true;
    /// Random-state policy; `none` for deterministic kernels.
    std::string rng_policy = "none";
    /// Optional semantic mode distinguishing variants of one capability.
    std::optional<std::string> semantic_mode;
    /// Optional approval token that must accompany `semantic_mode`.
    std::optional<std::string> mode_approval;
    /// Free-form provenance metadata such as owner and kernel symbol.
    std::map<std::string, std::string> provenance;

    /// Returns the stable native implementation identity.
    ///
    /// The declared `kernel_symbol` provenance value wins when present (the
    /// native analogue of Python module:qualname); otherwise the identity is
    /// `domain:capability_id`.
    std::string implementation_fingerprint() const;
};

/// Factory that returns a copy of a fixed set of operation declarations.
using operations_provider = std::function<std::vector<operation_spec>()>;
/// Creates a provider that captures and returns the supplied declarations.
operations_provider make_operations_provider(
    std::vector<operation_spec> declarations);

/// Ordered, digest-addressed set of validated operation declarations.
class operation_catalog {
  public:
    /// Creates a catalog from `operations`, optionally binding an extension
    /// digest.
    ///
    /// Declarations are sorted by `operation_id`. Throws `input_error` for
    /// empty identifiers or RNG policies, a missing callable, duplicate
    /// operation identifiers, unpaired semantic modes, or repeated modes on
    /// one capability.
    explicit operation_catalog(
        std::vector<operation_spec> operations,
        std::optional<std::string> extension_digest = std::nullopt);

    /// Returns all declarations sorted by operation identifier.
    const std::vector<operation_spec>& operations() const { return operations_; }
    /// Returns the SHA-256 digest of the canonical catalog payload.
    const std::string& digest() const { return digest_; }
    /// Returns the declaration for `operation_id`.
    ///
    /// Throws `operation_resolution_error` when the identifier is unknown.
    const operation_spec& resolve(const std::string& operation_id) const;
    /// Returns operation identifiers in catalog order.
    std::vector<std::string> operation_ids() const;

    /// Returns the pinned extension snapshot, or nullopt when unbound.
    ///
    /// This mirrors the source property that returns None for a catalog
    /// without extension bindings.
    const std::optional<extensions::extension_snapshot>& extension_snapshot()
        const noexcept {
        return extension_snapshot_;
    }

    /// Returns a new catalog with one immutable extension snapshot bound.
    ///
    /// The caller supplies native callables because the extension ABI carries
    /// metadata, not callables; a missing callable is a contract error, never
    /// a placeholder. Throws `input_error` when a callable is missing, this
    /// catalog is already bound, or merged declarations are invalid.
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

/// Snapshot of one operation request with digests of its inputs and config.
struct operation_request {
    /// Requested operation identifier.
    std::string operation_id;
    /// Inputs forwarded to the operation callable.
    runtime_inputs inputs;
    /// Recorded configuration; never forwarded to the callable.
    runtime_config config;
    /// Digest over `inputs`, computed by the constructor.
    std::string input_digest;
    /// Digest over `config`, computed by the constructor.
    std::string config_digest;

    /// Creates a request and computes both request digests.
    ///
    /// Throws `input_error` when `operation_id` is empty or when an input or
    /// config key is empty.
    operation_request(std::string operation_id, runtime_inputs inputs,
                      runtime_config config = {});
};

/// Resolved batch of requests tied to the digest of one catalog snapshot.
struct execution_plan {
    /// Digest of the catalog the plan was resolved against.
    std::string catalog_digest;
    /// Requests in caller-supplied order.
    std::vector<operation_request> requests;
};

/// Validates every request against `catalog` and records its digest.
///
/// Throws `operation_resolution_error` for an unknown operation identifier.
execution_plan plan(const std::vector<operation_request>& requests,
                    const operation_catalog& catalog);

/// Operation value plus string metadata describing the run.
struct runtime_result {
    /// Value returned by the operation callable.
    runtime_value value;
    /// Run metadata such as operation, input and catalog digests.
    std::map<std::string, std::string> metadata;
};

/// Runs one operation and returns its value with run metadata.
///
/// Config is recorded in result metadata only; operation callables receive
/// runtime inputs and never receive config.
///
/// Throws `operation_resolution_error` when the operation is unknown and
/// propagates validation errors raised by the callable.
runtime_result run(const std::string& operation_id,
                   const runtime_inputs& inputs,
                   const runtime_config& config,
                   const operation_catalog& catalog);

/// Runs every request in `plan` against the catalog that produced it.
///
/// Results are returned in request order. Throws `input_error` when the plan
/// digest does not match `catalog`, and propagates per-request validation
/// errors.
std::vector<runtime_result> batch(const execution_plan& plan,
                                  const operation_catalog& catalog);

/// Owning snapshot of runtime inputs with a stable input digest.
class analysis_snapshot {
  public:
    /// Constructs an empty snapshot with the digest of an empty input map.
    analysis_snapshot() = default;
    /// Creates a snapshot that owns `inputs` and computes its digest.
    explicit analysis_snapshot(runtime_inputs inputs);
    /// Returns the owned inputs.
    const runtime_inputs& materialize() const { return inputs_; }
    /// Returns the digest of the owned inputs.
    const std::string& digest() const { return digest_; }

  private:
    runtime_inputs inputs_;
    std::string digest_;
};

/// Caches deterministic results for one catalog across a session lifecycle.
class analysis_session {
  public:
    /// Creates an open session that owns a copy of `catalog`.
    explicit analysis_session(const operation_catalog& catalog);
    /// Returns whether `close()` has sealed the session.
    [[nodiscard]] bool closed() const noexcept { return closed_; }
    /// Returns the number of cached results.
    [[nodiscard]] std::size_t cache_entries() const noexcept {
        return cache_.size();
    }
    /// Returns the digest of the owned catalog snapshot.
    [[nodiscard]] const std::string& catalog_digest() const noexcept {
        return catalog_.digest();
    }
    /// Runs an operation, reusing cached results for deterministic operations.
    ///
    /// Deterministic cache hits and misses set `metadata["cache"]` to `hit`
    /// and `miss`; nondeterministic results are returned without caching.
    /// Throws `std::runtime_error` when the session is closed and
    /// `operation_resolution_error` for an unknown operation.
    runtime_result run(const std::string& operation_id,
                       const runtime_inputs& inputs,
                       const runtime_config& config = {});
    /// Clears entries without changing whether the session is open or closed.
    void clear();
    /// Clears the cache and seals the session against further runs.
    void close() noexcept;

  private:
    operation_catalog catalog_;
    std::map<std::string, runtime_result> cache_;
    bool closed_ = false;
};

/// Describes the media type and provenance of one artifact.
struct artifact_descriptor {
    /// MIME type used by consumers of this artifact.
    ///
    /// Legacy artifacts without an explicit descriptor are recorded as
    /// application/octet-stream.
    std::string media_type = "application/octet-stream";
    /// Artifact-specific provenance metadata.
    std::map<std::string, std::string> provenance;
};

/// Owning collection of named artifact bytes and their descriptors.
struct artifact_bundle {
    /// Artifact bytes keyed by exact artifact path.
    std::map<std::string, std::string> artifacts;
    /// Bundle-level metadata preserved in the manifest.
    std::map<std::string, std::string> metadata;
    /// Optional per-artifact metadata keyed by the exact artifact path.
    ///
    /// Every descriptor key must name an entry in artifacts. The manifest
    /// records the media type, byte length, SHA-256 of the actual artifact
    /// bytes, and provenance for each artifact; omitted descriptors use the
    /// legacy octet-stream/empty-provenance defaults.
    std::map<std::string, artifact_descriptor> descriptors;
    /// Native lifecycle seal set by `close()`.
    ///
    /// Artifacts are owning byte strings, so closing has no external resources
    /// to release; it prevents further registration.
    bool is_closed = false;

    /// Registers one artifact and its descriptor.
    ///
    /// Throws `fincore_error` when the bundle is closed and a validation
    /// `fincore_error` for an empty or duplicate artifact name.
    void add(std::string name, std::string bytes,
             artifact_descriptor descriptor = {});
    /// Seals the bundle against further registration; repeat calls are safe.
    void close() noexcept { is_closed = true; }
    /// Returns whether the bundle has been sealed.
    [[nodiscard]] bool closed() const noexcept { return is_closed; }
};

/// Writes a bundle into a new output directory.
///
/// Existing output directories, unsafe artifact paths and symlink parents are
/// rejected. The bundle metadata and artifact names are preserved in
/// `.fincore-manifest.json`; per-file descriptors are validated before any
/// output is created. Throws `fincore_error` for a closed bundle and
/// `input_error` for rejected paths or descriptors.
void write_artifact_bundle(const artifact_bundle& bundle,
                           const std::filesystem::path& output_directory);

/// Returns the builtin catalog of native metrics, risk and report operations.
///
/// Builtin report operation inputs follow the pinned source IDs. The
/// walk-forward operation requires `returns` (dataframe) and `spec`
/// (risk_model_spec). Risk validation requires `result`
/// (walk_forward_var_result) and accepts optional `index_name` (string). Risk
/// document construction requires `report` (risk_validation_report) and accepts
/// optional `title` (string). Portfolio report construction requires `returns`
/// (dataframe) and accepts optional `benchmark`, `positions` (dataframe),
/// `transactions` (vector<portfolio_transaction>), `rolling_window` (integer),
/// and `title` (string). These operations ignore config values and do not
/// forward them to domain functions.
operation_catalog builtin_catalog();

}  // namespace fincore
