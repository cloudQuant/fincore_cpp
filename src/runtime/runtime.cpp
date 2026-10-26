#include <fincore/runtime/runtime.hpp>

#include <fincore/runtime/operations_metrics.hpp>
#include <fincore/runtime/operations_portfolio.hpp>
#include <fincore/runtime/operations_factor.hpp>
#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>
#include <fincore/errors.hpp>
#include <fincore/metrics/alpha_beta.hpp>
#include <fincore/metrics/drawdown.hpp>
#include <fincore/metrics/ratios.hpp>
#include <fincore/metrics/returns.hpp>
#include <fincore/metrics/risk.hpp>
#include <fincore/metrics/stats.hpp>
#include <fincore/metrics/rolling.hpp>
#include <fincore/risk/backtesting.hpp>
#include <fincore/report/portfolio.hpp>
#include <fincore/report/risk.hpp>
#include <fincore/risk/report.hpp>
#include <fincore/risk/walk_forward.hpp>

#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <fcntl.h>
#include <iterator>
#include <limits>
#include <map>
#include <set>
#include <string>
#include <span>
#include <string_view>
#include <system_error>
#include <typeindex>
#include <typeinfo>
#include <type_traits>
#include <utility>
#include <vector>
#if defined(_WIN32)
#include <io.h>
#include <sys/stat.h>
#else
#include <sys/types.h>
#include <unistd.h>
#endif

namespace fincore {
namespace {

std::string digest_text(const std::string& text) {
    return detail::sha256_hex(std::span<const std::uint8_t>(
        reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

void validate_non_empty_keys(const runtime_inputs& values,
                             const std::string_view map_name) {
    for (const auto& entry : values) {
        if (entry.first.empty()) {
            throw input_error{std::string(map_name) +
                              " keys must be non-empty strings"};
        }
    }
}

// Every field is written as its byte count, a colon and its bytes, so no value can
// be reinterpreted as a separator, and each entry's position is digested too: the
// same pair in a different order gives a different digest even though the map
// itself iterates in one fixed order.
std::string runtime_map_digest(const std::string_view map_type,
                               const runtime_inputs& values) {
    std::string payload;
    payload += std::to_string(map_type.size());
    payload.push_back(':');
    payload.append(map_type);
    payload += std::to_string(values.size());
    payload.push_back(':');
    std::size_t index = 0U;
    for (const auto& entry : values) {
        const auto append = [&payload](const std::string_view value) {
            payload += std::to_string(value.size());
            payload.push_back(':');
            payload.append(value);
        };
        append(std::to_string(index++));
        append(entry.first);
        append(runtime_value_digest(entry.second));
    }
    return digest_text(payload);
}

// Each field carries a one-character type tag alongside its name, so a bool written
// as "1" and an integer one cannot produce the same bytes: the digest distinguishes
// the declared type, not just the rendered text.
class typed_digest_builder {
  public:
    explicit typed_digest_builder(const std::string_view type) {
        append("runtime_type", 't', type);
    }

    void string_field(const std::string_view name,
                      const std::string_view value) {
        append(name, 's', value);
    }

    void bool_field(const std::string_view name, const bool value) {
        append(name, 'b', value ? "1" : "0");
    }

    void signed_field(const std::string_view name, const std::int64_t value) {
        append(name, 'i', std::to_string(value));
    }

    void unsigned_field(const std::string_view name,
                        const std::uint64_t value) {
        append(name, 'u', std::to_string(value));
    }

    void size_field(const std::string_view name, const std::size_t value) {
        unsigned_field(name, static_cast<std::uint64_t>(value));
    }

    void enum_field(const std::string_view name, const int value) {
        append(name, 'e', std::to_string(value));
    }

    void double_field(const std::string_view name, const double value) {
        std::uint64_t bits = 0U;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        append(name, 'f', std::to_string(bits));
    }

    void float_field(const std::string_view name, const float value) {
        std::uint32_t bits = 0U;
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        append(name, 'g', std::to_string(bits));
    }

    void optional_string_field(
        const std::string_view name,
        const std::optional<std::string>& value) {
        bool_field(std::string(name) + ".present", value.has_value());
        if (value.has_value()) {
            string_field(name, *value);
        }
    }

    std::string finish() const { return digest_text(payload_); }

  private:
    void append(const std::string_view name, const char type,
                const std::string_view value) {
        payload_ += std::to_string(name.size());
        payload_.push_back(':');
        payload_.append(name);
        payload_.push_back(type);
        payload_ += std::to_string(value.size());
        payload_.push_back(':');
        payload_.append(value);
        payload_.push_back(';');
    }

    std::string payload_;
};

std::string canonical_operation(const operation_spec& spec) {
    typed_digest_builder digest{"operation_spec"};
    digest.string_field("operation_id", spec.operation_id);
    digest.string_field("capability_id", spec.capability_id);
    digest.string_field("domain", spec.domain);
    digest.bool_field("deterministic", spec.deterministic);
    digest.string_field("rng_policy", spec.rng_policy);
    digest.optional_string_field("semantic_mode", spec.semantic_mode);
    digest.optional_string_field("mode_approval", spec.mode_approval);
    digest.size_field("provenance.size", spec.provenance.size());
    std::size_t index = 0U;
    for (const auto& entry : spec.provenance) {
        const auto prefix =
            "provenance[" + std::to_string(index++) + "]";
        digest.string_field(prefix + ".key", entry.first);
        digest.string_field(prefix + ".value", entry.second);
    }
    return digest.finish();
}

std::string indexed_field(const std::string_view prefix,
                          const std::size_t index,
                          const std::string_view suffix) {
    return std::string(prefix) + "[" + std::to_string(index) + "]." +
           std::string(suffix);
}

void append_risk_model_spec(typed_digest_builder& digest,
                            const std::string_view prefix,
                            const risk_model_spec& spec) {
    const std::string field(prefix);
    digest.enum_field(field + ".forecast_target",
                      static_cast<int>(spec.forecast_target));
    digest.double_field(field + ".confidence_level", spec.confidence_level);
    digest.size_field(field + ".horizon", spec.horizon);
    digest.string_field(field + ".distribution", spec.distribution);
    digest.enum_field(field + ".tail", static_cast<int>(spec.tail));
    digest.string_field(field + ".sign_convention", spec.sign_convention);
    digest.size_field(field + ".window", spec.window);
    digest.size_field(field + ".refit_cadence", spec.refit_cadence);
    digest.string_field(field + ".model_version", spec.model_version);
}

void append_risk_backtest(typed_digest_builder& digest,
                          const std::string_view prefix,
                          const risk_backtest_result& result) {
    const std::string field(prefix);
    digest.enum_field(field + ".method", static_cast<int>(result.method));
    digest.double_field(field + ".confidence_level", result.confidence_level);
    digest.size_field(field + ".observations", result.observations);
    digest.size_field(field + ".exceptions", result.exceptions);
    digest.double_field(field + ".expected_exceptions",
                        result.expected_exceptions);
    digest.double_field(field + ".exception_rate", result.exception_rate);
    digest.double_field(field + ".kupiec_lr", result.kupiec_lr);
    digest.double_field(field + ".kupiec_pvalue", result.kupiec_pvalue);
    digest.double_field(field + ".christoffersen_lr",
                        result.christoffersen_lr);
    digest.double_field(field + ".christoffersen_pvalue",
                        result.christoffersen_pvalue);
    digest.string_field(field + ".inputs_digest", result.inputs_digest);
    digest.size_field(field + ".diagnostics.size",
                      result.diagnostics.size());
    std::size_t diagnostic_index = 0U;
    for (const auto& entry : result.diagnostics) {
        const auto diagnostic_field =
            indexed_field(field + ".diagnostics", diagnostic_index, "key");
        digest.string_field(diagnostic_field, entry.first);
        digest.double_field(indexed_field(field + ".diagnostics",
                                          diagnostic_index, "value"),
                            entry.second);
        ++diagnostic_index;
    }
    digest.enum_field(field + ".status", static_cast<int>(result.status));
}

void append_walk_forward_result(typed_digest_builder& digest,
                                const walk_forward_var_result& result) {
    append_risk_model_spec(digest, "spec", result.spec);
    digest.size_field("index.size", result.index.size());
    for (std::size_t index = 0U; index < result.index.size(); ++index) {
        digest.signed_field(indexed_field("index", index, "timestamp"),
                            result.index[index]);
    }
    digest.size_field("forecast.size", result.forecast.size());
    for (std::size_t index = 0U; index < result.forecast.size(); ++index) {
        digest.double_field(indexed_field("forecast", index, "value"),
                            result.forecast[index]);
    }
    digest.size_field("realized.size", result.realized.size());
    for (std::size_t index = 0U; index < result.realized.size(); ++index) {
        digest.double_field(indexed_field("realized", index, "value"),
                            result.realized[index]);
    }
    digest.size_field("refit_timestamps.size",
                      result.refit_timestamps.size());
    for (std::size_t index = 0U; index < result.refit_timestamps.size();
         ++index) {
        digest.signed_field(
            indexed_field("refit_timestamps", index, "timestamp"),
            result.refit_timestamps[index]);
    }
    digest.size_field("fits.size", result.fits.size());
    for (std::size_t index = 0U; index < result.fits.size(); ++index) {
        const auto& fit = result.fits[index];
        digest.signed_field(indexed_field("fits", index, "date"), fit.date);
        digest.double_field(indexed_field("fits", index, "forecast"),
                            fit.forecast);
        digest.double_field(indexed_field("fits", index, "mean"), fit.mean);
        digest.double_field(indexed_field("fits", index, "standard_deviation"),
                            fit.standard_deviation);
        digest.size_field(indexed_field("fits", index, "n_observations"),
                          fit.n_observations);
    }
    digest.string_field("inputs_digest", result.inputs_digest);
    digest.enum_field("status", static_cast<int>(result.status));
    digest.string_field("unsupported_reason", result.unsupported_reason);
    digest.size_field("n_observations", result.n_observations);
    append_risk_backtest(digest, "backtest", result.backtest);
    digest.bool_field("has_backtest", result.has_backtest);
}

void append_risk_validation_report(typed_digest_builder& digest,
                                   const risk_validation_report& report) {
    digest.enum_field("status", static_cast<int>(report.status));
    digest.string_field("inputs_digest", report.inputs_digest);
    append_risk_model_spec(digest, "specification", report.specification);
    digest.size_field("forecast_events.size", report.forecast_events.size());
    for (std::size_t index = 0U; index < report.forecast_events.size();
         ++index) {
        const auto& event = report.forecast_events[index];
        digest.signed_field(indexed_field("forecast_events", index, "date"),
                            event.date);
        digest.double_field(
            indexed_field("forecast_events", index, "forecast"),
            event.forecast);
        digest.double_field(
            indexed_field("forecast_events", index, "realized"),
            event.realized);
        digest.bool_field(
            indexed_field("forecast_events", index, "exception"),
            event.exception);
        digest.bool_field(indexed_field("forecast_events", index, "refit"),
                          event.refit);
    }
    digest.size_field("refits.size", report.refits.size());
    for (std::size_t index = 0U; index < report.refits.size(); ++index) {
        const auto& refit = report.refits[index];
        digest.signed_field(indexed_field("refits", index, "date"),
                            refit.date);
        digest.double_field(indexed_field("refits", index, "forecast"),
                            refit.forecast);
        digest.double_field(indexed_field("refits", index, "mean"),
                            refit.mean);
        digest.double_field(
            indexed_field("refits", index, "standard_deviation"),
            refit.standard_deviation);
        digest.size_field(indexed_field("refits", index, "n_observations"),
                          refit.n_observations);
    }
    digest.string_field("diagnostics.method", report.diagnostics.method);
    digest.string_field("diagnostics.quantile_method",
                        report.diagnostics.quantile_method);
    digest.size_field("diagnostics.window", report.diagnostics.window);
    digest.size_field("diagnostics.refit_cadence",
                      report.diagnostics.refit_cadence);
    digest.size_field("diagnostics.forecast_count",
                      report.diagnostics.forecast_count);
    digest.string_field("diagnostics.backtest_status",
                        report.diagnostics.backtest_status);
    digest.string_field("diagnostics.reason", report.diagnostics.reason);
    digest.size_field("diagnostics.n_observations",
                      report.diagnostics.n_observations);
    digest.bool_field("has_backtest", report.has_backtest);
    append_risk_backtest(digest, "backtest", report.backtest);
    digest.string_field("traffic_light_zone", report.traffic_light_zone);
    digest.optional_string_field("timestamp_index_name",
                                 report.timestamp_index_name);
    digest.optional_string_field("timestamp_timezone",
                                 report.timestamp_timezone);
    digest.string_field("disclosure", report.disclosure);
    digest.signed_field("schema_version", report.schema_version);
}

void append_portfolio_transactions(
    typed_digest_builder& digest,
    const std::vector<portfolio_transaction>& transactions) {
    digest.size_field("transactions.size", transactions.size());
    for (std::size_t index = 0U; index < transactions.size(); ++index) {
        const auto& transaction = transactions[index];
        digest.signed_field(indexed_field("transactions", index, "date"),
                            transaction.date);
        digest.string_field(indexed_field("transactions", index, "symbol"),
                            transaction.symbol);
        digest.double_field(indexed_field("transactions", index, "amount"),
                            transaction.amount);
        digest.double_field(indexed_field("transactions", index, "price"),
                            transaction.price);
    }
}

template <typename T>
void append_dataframe_column(const dataframe& frame, const std::string& name,
                             const std::string& prefix,
                             const std::string_view type_name,
                             typed_digest_builder& digest) {
    digest.string_field(prefix + ".type", type_name);
    const auto& values = frame.get_column<T>(name.c_str());
    digest.size_field(prefix + ".values.size", values.size());
    for (std::size_t index = 0U; index < values.size(); ++index) {
        const auto field = indexed_field(prefix + ".values", index, "value");
        if constexpr (std::is_same_v<T, bool>) {
            digest.bool_field(field, static_cast<bool>(values[index]));
        } else if constexpr (std::is_same_v<T, float>) {
            digest.float_field(field, values[index]);
        } else if constexpr (std::is_same_v<T, double>) {
            digest.double_field(field, values[index]);
        } else if constexpr (std::is_integral_v<T> && std::is_signed_v<T>) {
            digest.signed_field(field,
                                static_cast<std::int64_t>(values[index]));
        } else if constexpr (std::is_integral_v<T> &&
                             std::is_unsigned_v<T>) {
            digest.unsigned_field(field,
                                  static_cast<std::uint64_t>(values[index]));
        } else if constexpr (std::is_same_v<T, std::string>) {
            digest.string_field(field, values[index]);
        }
    }
}

bool append_supported_dataframe_column(const dataframe& frame,
                                       const std::string& name,
                                       const std::string& prefix,
                                       const std::type_index type,
                                       typed_digest_builder& digest) {
#define FINCORE_RUNTIME_APPEND_COLUMN(Type, Label)                            \
    if (type == std::type_index(typeid(Type))) {                              \
        append_dataframe_column<Type>(frame, name, prefix, Label, digest);    \
        return true;                                                          \
    }
    FINCORE_RUNTIME_APPEND_COLUMN(bool, "bool")
    FINCORE_RUNTIME_APPEND_COLUMN(char, "char")
    FINCORE_RUNTIME_APPEND_COLUMN(signed char, "signed_char")
    FINCORE_RUNTIME_APPEND_COLUMN(unsigned char, "unsigned_char")
    FINCORE_RUNTIME_APPEND_COLUMN(short, "short")
    FINCORE_RUNTIME_APPEND_COLUMN(unsigned short, "unsigned_short")
    FINCORE_RUNTIME_APPEND_COLUMN(int, "int")
    FINCORE_RUNTIME_APPEND_COLUMN(unsigned int, "unsigned_int")
    FINCORE_RUNTIME_APPEND_COLUMN(long, "long")
    FINCORE_RUNTIME_APPEND_COLUMN(unsigned long, "unsigned_long")
    FINCORE_RUNTIME_APPEND_COLUMN(long long, "long_long")
    FINCORE_RUNTIME_APPEND_COLUMN(unsigned long long, "unsigned_long_long")
    FINCORE_RUNTIME_APPEND_COLUMN(float, "float")
    FINCORE_RUNTIME_APPEND_COLUMN(double, "double")
    FINCORE_RUNTIME_APPEND_COLUMN(std::string, "string")
#undef FINCORE_RUNTIME_APPEND_COLUMN
    return false;
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

std::optional<std::int64_t> optional_integer(const runtime_inputs& inputs,
                                             const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return std::nullopt;
    }
    const auto* value = std::get_if<std::int64_t>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be an integer: " + name};
    }
    return *value;
}

std::size_t to_non_negative_size(const std::int64_t value,
                                 const std::string& name) {
    if (value < 0) {
        throw input_error{"runtime input must be non-negative: " + name};
    }
    const auto unsigned_value = static_cast<std::uint64_t>(value);
    if (unsigned_value >
        static_cast<std::uint64_t>(std::numeric_limits<std::size_t>::max())) {
        throw input_error{"runtime input is too large: " + name};
    }
    return static_cast<std::size_t>(unsigned_value);
}

std::size_t require_non_negative_size(const runtime_inputs& inputs,
                                     const std::string& name) {
    return to_non_negative_size(require_integer(inputs, name), name);
}

std::optional<std::size_t> optional_non_negative_size(
    const runtime_inputs& inputs, const std::string& name) {
    const auto value = optional_integer(inputs, name);
    if (!value.has_value()) {
        return std::nullopt;
    }
    return to_non_negative_size(*value, name);
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

const dataframe* optional_frame(const runtime_inputs& inputs,
                                const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return nullptr;
    }
    const auto* value = std::get_if<dataframe>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a dataframe: " + name};
    }
    return value;
}

const risk_model_spec& require_risk_model_spec(const runtime_inputs& inputs,
                                              const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value = std::get_if<risk_model_spec>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a risk_model_spec: " + name};
    }
    return *value;
}

const walk_forward_var_result& require_walk_forward_result(
    const runtime_inputs& inputs, const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value =
        std::get_if<walk_forward_var_result>(&found->second.data);
    if (value == nullptr) {
        throw input_error{
            "runtime input must be a walk_forward_var_result: " + name};
    }
    return *value;
}

const risk_validation_report& require_risk_validation_report(
    const runtime_inputs& inputs, const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        throw input_error{"missing runtime input: " + name};
    }
    const auto* value =
        std::get_if<risk_validation_report>(&found->second.data);
    if (value == nullptr) {
        throw input_error{
            "runtime input must be a risk_validation_report: " + name};
    }
    return *value;
}

std::optional<std::string> optional_string(const runtime_inputs& inputs,
                                          const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return std::nullopt;
    }
    const auto* value = std::get_if<std::string>(&found->second.data);
    if (value == nullptr) {
        throw input_error{"runtime input must be a string: " + name};
    }
    return *value;
}

std::span<const portfolio_transaction> optional_transactions(
    const runtime_inputs& inputs, const std::string& name) {
    const auto found = inputs.find(name);
    if (found == inputs.end()) {
        return {};
    }
    const auto* value =
        std::get_if<std::vector<portfolio_transaction>>(&found->second.data);
    if (value == nullptr) {
        throw input_error{
            "runtime input must be a vector of portfolio_transaction: " +
            name};
    }
    return *value;
}

constexpr std::string_view artifact_manifest_name =
    ".fincore-manifest.json";

bool path_entry_exists(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        return false;
    }
    if (error) {
        throw std::filesystem::filesystem_error(
            "cannot inspect artifact path", path, error);
    }
    return status.type() != std::filesystem::file_type::not_found;
}

#if defined(_WIN32)
bool is_windows_reserved_component(const std::string& component) {
    auto stem = component.substr(0, component.find('.'));
    for (auto& character : stem) {
        if (character >= 'A' && character <= 'Z') {
            character = static_cast<char>(character - 'A' + 'a');
        }
    }
    if (stem == "con" || stem == "prn" || stem == "aux" || stem == "nul") {
        return true;
    }
    return stem.size() == 4 &&
           (stem.compare(0, 3, "com") == 0 ||
            stem.compare(0, 3, "lpt") == 0) &&
           stem[3] >= '1' && stem[3] <= '9';
}
#endif

std::filesystem::path validate_artifact_name(const std::string& name) {
    if (name.empty() || name.find('\0') != std::string::npos ||
        name.find('\\') != std::string::npos) {
        throw input_error{"artifact names must be non-empty relative paths"};
    }
    const std::filesystem::path path{name};
    if (path.is_absolute() || path.has_root_name() ||
        path.has_root_directory() || path.filename().empty()) {
        throw input_error{"artifact names must be non-empty relative paths"};
    }
    for (const auto& component : path) {
        if (component == ".." || component == ".") {
            throw input_error{"artifact names must not contain dot paths"};
        }
#if defined(_WIN32)
        const auto text = component.string();
        if (text.find(':') != std::string::npos ||
            (!text.empty() &&
             (text.back() == '.' || text.back() == ' ')) ||
            is_windows_reserved_component(text)) {
            throw input_error{"artifact names must be portable to Windows"};
        }
#endif
    }
    const auto normalized = path.lexically_normal();
    if (normalized.empty() || normalized == ".") {
        throw input_error{"artifact names must identify a file"};
    }
    return normalized;
}

bool is_mime_token_character(const unsigned char character) {
    return (character >= 'a' && character <= 'z') ||
           (character >= 'A' && character <= 'Z') ||
           (character >= '0' && character <= '9') ||
           character == '!' || character == '#' || character == '$' ||
           character == '%' || character == '&' || character == '\'' ||
           character == '*' || character == '+' || character == '-' ||
           character == '.' || character == '^' || character == '_' ||
           character == '`' || character == '|' || character == '~';
}

void validate_media_type(const std::string& media_type) {
    const auto slash = media_type.find('/');
    if (slash == std::string::npos || slash == 0U ||
        slash + 1U == media_type.size() ||
        media_type.find('/', slash + 1U) != std::string::npos ||
        !std::all_of(media_type.begin(), media_type.end(),
                     [](const unsigned char character) {
                         return character == '/' ||
                                is_mime_token_character(character);
                     })) {
        throw input_error{
            "artifact media types must be non-empty type/subtype tokens"};
    }
}

std::filesystem::path validate_output_directory(
    const std::filesystem::path& requested) {
    if (requested.empty()) {
        throw input_error{"artifact output directory must be non-empty"};
    }
    for (const auto& component : requested) {
        if (component == "..") {
            throw input_error{
                "artifact output directory must not contain parent paths"};
        }
    }
    const auto absolute =
        std::filesystem::absolute(requested).lexically_normal();
    const auto leaf = absolute.filename();
    if (leaf.empty() || leaf == "." || leaf == "..") {
        throw input_error{"artifact output directory must name a new directory"};
    }

    std::filesystem::path current = absolute.root_path();
    for (const auto& component : absolute.parent_path().relative_path()) {
        if (component.empty() || component == ".") {
            continue;
        }
        current /= component;
        if (!path_entry_exists(current)) {
            throw input_error{
                "artifact output directory parent must already exist"};
        }
        std::error_code error;
        const auto status = std::filesystem::symlink_status(current, error);
        if (error) {
            throw std::filesystem::filesystem_error(
                "cannot inspect artifact output parent", current, error);
        }
        if (std::filesystem::is_symlink(status)) {
            throw input_error{
                "artifact output directory must not have symlink parents"};
        }
        if (!std::filesystem::is_directory(status)) {
            throw input_error{
                "artifact output directory parent must be a directory"};
        }
    }
    return absolute;
}

std::string artifact_manifest(const artifact_bundle& bundle) {
    std::string output;
    detail::json_writer writer(output);
    writer.begin_object();
    writer.key("artifacts");
    writer.begin_array();
    for (const auto& entry : bundle.artifacts) {
        writer.value_string(entry.first);
    }
    writer.end_array();
    writer.key("files");
    writer.begin_array();
    for (const auto& entry : bundle.artifacts) {
        const auto descriptor = bundle.descriptors.find(entry.first);
        const artifact_descriptor defaults{};
        const auto& file_descriptor =
            descriptor == bundle.descriptors.end() ? defaults
                                                   : descriptor->second;
        if (entry.second.size() >
            static_cast<std::size_t>(std::numeric_limits<std::int64_t>::max())) {
            throw input_error{"artifact is too large to describe in manifest"};
        }
        writer.begin_object();
        writer.key("path");
        writer.value_string(entry.first);
        writer.key("media_type");
        writer.value_string(file_descriptor.media_type);
        writer.key("size_bytes");
        writer.value_integer(static_cast<std::int64_t>(entry.second.size()));
        writer.key("sha256");
        writer.value_string(digest_text(entry.second));
        writer.key("provenance");
        writer.begin_object();
        for (const auto& item : file_descriptor.provenance) {
            writer.key(item.first);
            writer.value_string(item.second);
        }
        writer.end_object();
        writer.end_object();
    }
    writer.end_array();
    writer.key("metadata");
    writer.begin_object();
    for (const auto& entry : bundle.metadata) {
        writer.key(entry.first);
        writer.value_string(entry.second);
    }
    writer.end_object();
    writer.key("schema_version");
    writer.value_integer(2);
    writer.end_object();
    output.push_back('\n');
    return output;
}

// The two platforms cannot express the same open: only POSIX can make the open call
// itself refuse a symlink, which is why the Windows branch checks the path
// separately, and the no-inherit flag exists only there because a child process
// would otherwise hold the handle. The chunk limit follows the narrowest write
// signature, the Windows one taking an unsigned length.
#if defined(_WIN32)
constexpr int exclusive_write_flags =
    _O_WRONLY | _O_CREAT | _O_EXCL | _O_BINARY | _O_NOINHERIT;
constexpr std::size_t max_write_chunk =
    std::numeric_limits<unsigned int>::max();

int open_exclusive(const std::filesystem::path& path) {
    return _wopen(path.c_str(), exclusive_write_flags,
                  _S_IREAD | _S_IWRITE);
}
int write_bytes(const int descriptor, const char* data,
                const std::size_t size) {
    return _write(descriptor, data, static_cast<unsigned int>(size));
}
int close_file(const int descriptor) { return _close(descriptor); }
int remove_file(const std::filesystem::path& path) {
    return _wunlink(path.c_str());
}
#else
constexpr int exclusive_write_flags =
    O_WRONLY | O_CREAT | O_EXCL | O_NOFOLLOW;
constexpr std::size_t max_write_chunk =
    static_cast<std::size_t>(std::numeric_limits<ssize_t>::max());

int open_exclusive(const std::filesystem::path& path) {
    return ::open(path.c_str(), exclusive_write_flags, 0666);
}
ssize_t write_bytes(const int descriptor, const char* data,
                    const std::size_t size) {
    return ::write(descriptor, data, size);
}
int close_file(const int descriptor) { return ::close(descriptor); }
int remove_file(const std::filesystem::path& path) {
    return ::unlink(path.c_str());
}
#endif

void write_bundle_file(const std::filesystem::path& root,
                       const std::filesystem::path& canonical_root,
                       const std::filesystem::path& relative_path,
                       const std::string& bytes,
                       std::vector<std::filesystem::path>& created_paths) {
    const auto check_root = [&root, &canonical_root] {
        std::error_code error;
        const auto status = std::filesystem::symlink_status(root, error);
        if (error || std::filesystem::is_symlink(status) ||
            !std::filesystem::is_directory(status) ||
            std::filesystem::canonical(root) != canonical_root) {
            throw input_error{"artifact output directory changed during writing"};
        }
    };
    const auto within_root = [&canonical_root](
                                const std::filesystem::path& path) {
        const auto relative = path.lexically_relative(canonical_root);
        return !relative.empty() && !relative.is_absolute() &&
               (relative == "." || *relative.begin() != "..");
    };
    check_root();
    std::filesystem::path parent = root;
    for (const auto& component : relative_path.parent_path()) {
        parent /= component;
        if (path_entry_exists(parent)) {
            std::error_code error;
            const auto status = std::filesystem::symlink_status(parent, error);
            if (error) {
                throw std::filesystem::filesystem_error(
                    "cannot inspect artifact parent", parent, error);
            }
            if (std::filesystem::is_symlink(status)) {
                throw input_error{"artifact paths must not have symlink parents"};
            }
            if (!std::filesystem::is_directory(status)) {
                throw input_error{
                    "artifact path parent must be a directory"};
            }
        } else {
            if (!std::filesystem::create_directory(parent)) {
                throw std::runtime_error{
                    "could not create artifact parent directory"};
            }
            created_paths.push_back(parent);
        }
        const auto canonical_parent = std::filesystem::canonical(parent);
        if (!within_root(canonical_parent)) {
            throw input_error{"artifact parent path escapes output directory"};
        }
    }

    check_root();
    const auto canonical_parent = std::filesystem::canonical(parent);
    if (!within_root(canonical_parent)) {
        throw input_error{"artifact parent path escapes output directory"};
    }
    const auto destination = root / relative_path;
    if (path_entry_exists(destination)) {
        throw input_error{"refusing to overwrite an artifact path"};
    }
    int descriptor = open_exclusive(destination);
    if (descriptor < 0) {
        throw std::filesystem::filesystem_error(
            "could not create artifact exclusively", destination,
            std::error_code(errno, std::generic_category()));
    }
    try {
        created_paths.push_back(destination);
    } catch (...) {
        close_file(descriptor);
        remove_file(destination);
        throw;
    }

    try {
        std::size_t written = 0U;
        while (written < bytes.size()) {
            const auto chunk = std::min(bytes.size() - written,
                                        max_write_chunk);
            const auto count = write_bytes(descriptor, bytes.data() + written,
                                           chunk);
            if (count < 0) {
                if (errno == EINTR) {
                    continue;
                }
                throw std::system_error(errno, std::generic_category(),
                                        "could not write artifact bytes");
            }
            if (count == 0) {
                throw std::runtime_error{
                    "could not write complete artifact bytes"};
            }
            written += static_cast<std::size_t>(count);
        }
        const int close_status = close_file(descriptor);
        descriptor = -1;
        if (close_status != 0) {
            throw std::system_error(errno, std::generic_category(),
                                    "could not close artifact after writing");
        }
    } catch (...) {
        if (descriptor >= 0) {
            close_file(descriptor);
        }
        throw;
    }
}

}  // namespace

std::string runtime_value_digest(const runtime_value& value) {
    typed_digest_builder digest{"runtime_value"};
    if (std::holds_alternative<std::monostate>(value.data)) {
        digest.string_field("value_type", "null");
    } else if (const auto* item = std::get_if<bool>(&value.data)) {
        digest.string_field("value_type", "bool");
        digest.bool_field("value", *item);
    } else if (const auto* item = std::get_if<std::int64_t>(&value.data)) {
        digest.string_field("value_type", "int64");
        digest.signed_field("value", *item);
    } else if (const auto* item = std::get_if<double>(&value.data)) {
        digest.string_field("value_type", "double");
        digest.double_field("value", *item);
    } else if (const auto* item = std::get_if<std::string>(&value.data)) {
        digest.string_field("value_type", "string");
        digest.string_field("value", *item);
    } else if (const auto* frame = std::get_if<dataframe>(&value.data)) {
        digest.string_field("value_type", "dataframe");
        digest.size_field("index.size", frame->get_index().size());
        const auto columns = frame->get_columns_info<
            bool, char, signed char, unsigned char, short, unsigned short,
            int, unsigned int, long, unsigned long, long long,
            unsigned long long, float, double, std::string>();
        if (columns.size() != frame->shape().second) {
            throw input_error{
                "unsupported runtime dataframe column type"};
        }
        digest.size_field("columns.size", columns.size());
        for (std::size_t column_index = 0U; column_index < columns.size();
             ++column_index) {
            const auto& info = columns[column_index];
            const std::string name{std::get<0>(info).c_str()};
            const std::string prefix = "columns[" +
                                       std::to_string(column_index) + "]";
            digest.string_field(prefix + ".name", name);
            digest.size_field(prefix + ".values.size", std::get<1>(info));
            if (!append_supported_dataframe_column(
                    *frame, name, prefix, std::get<2>(info), digest)) {
                throw input_error{
                    "unsupported runtime dataframe column type: " + name};
            }
        }
        const auto& index = frame->get_index();
        for (std::size_t position = 0U; position < index.size(); ++position) {
            digest.signed_field(indexed_field("index", position, "timestamp"),
                                index[position]);
        }
    } else if (const auto* document =
                   std::get_if<report_document>(&value.data)) {
        digest.string_field("value_type", "report_document");
        digest.string_field("value", document->to_json());
    } else if (const auto* spec =
                   std::get_if<risk_model_spec>(&value.data)) {
        digest.string_field("value_type", "risk_model_spec");
        append_risk_model_spec(digest, "value", *spec);
    } else if (const auto* result =
                   std::get_if<walk_forward_var_result>(&value.data)) {
        digest.string_field("value_type", "walk_forward_var_result");
        append_walk_forward_result(digest, *result);
    } else if (const auto* report =
                   std::get_if<risk_validation_report>(&value.data)) {
        digest.string_field("value_type", "risk_validation_report");
        append_risk_validation_report(digest, *report);
    } else if (const auto* transactions =
                   std::get_if<std::vector<portfolio_transaction>>(
                       &value.data)) {
        digest.string_field("value_type", "portfolio_transactions");
        append_portfolio_transactions(digest, *transactions);
    }
    return digest.finish();
}

std::string operation_spec::implementation_fingerprint() const {
    const auto symbol = provenance.find("kernel_symbol");
    if (symbol != provenance.end() && !symbol->second.empty()) {
        return symbol->second;
    }
    return domain + ":" + capability_id;
}

operations_provider make_operations_provider(
    std::vector<operation_spec> declarations) {
    return [declarations = std::move(declarations)]() {
        return declarations;
    };
}

operation_catalog::operation_catalog(
    std::vector<operation_spec> operations,
    std::optional<std::string> extension_digest) {
    std::set<std::string> seen_ids;
    std::map<std::string, std::vector<std::optional<std::string>>>
        modes_by_capability;
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
        auto& previous_modes = modes_by_capability[spec.capability_id];
        if (!previous_modes.empty() &&
            (!spec.semantic_mode.has_value() ||
             std::any_of(previous_modes.begin(), previous_modes.end(),
                         [&spec](const auto& previous_mode) {
                             return !previous_mode.has_value() ||
                                    previous_mode == spec.semantic_mode;
                         }))) {
            throw input_error{
                "multiple operation IDs for one leaf capability require "
                "distinct semantic modes: " +
                spec.capability_id};
        }
        previous_modes.push_back(spec.semantic_mode);
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
    if (extension_digest.has_value()) {
        payload += "extension_digest:" + *extension_digest + "--\n";
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

operation_catalog operation_catalog::with_extensions(
    const extensions::extension_snapshot& snapshot,
    const std::map<std::string,
                   std::function<runtime_value(const runtime_inputs&)>>&
        callables) const {
    if (extension_snapshot_.has_value()) {
        throw input_error{
            "catalog is already bound to an extension snapshot"};
    }
    std::vector<operation_spec> merged = operations_;
    merged.reserve(operations_.size() + snapshot.operations().size());
    for (const auto& extension : snapshot.operations()) {
        const auto callable = callables.find(extension.operation_id);
        if (callable == callables.end() || !callable->second) {
            throw input_error{
                "missing callable for extension operation: " +
                extension.operation_id};
        }
        operation_spec spec;
        spec.operation_id = extension.operation_id;
        spec.capability_id = extension.capability_id;
        spec.domain = extension.domain;
        spec.callable = callable->second;
        spec.deterministic = extension.deterministic;
        spec.rng_policy = extension.rng_policy;
        spec.semantic_mode = extension.semantic_mode;
        spec.mode_approval = extension.mode_approval;
        spec.provenance = extension.provenance;
        if (!extension.implementation_fingerprint.empty()) {
            spec.provenance["kernel_symbol"] =
                extension.implementation_fingerprint;
        }
        merged.push_back(std::move(spec));
    }
    operation_catalog catalog{std::move(merged), snapshot.digest()};
    catalog.extension_snapshot_ = snapshot;
    return catalog;
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
    validate_non_empty_keys(this->inputs, "inputs");
    validate_non_empty_keys(this->config, "config");
    input_digest = runtime_map_digest("runtime_inputs", this->inputs);
    config_digest = runtime_map_digest("runtime_config", this->config);
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
        {"digest_version", "2"},
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
    digest_ = runtime_map_digest("runtime_inputs", inputs_);
}

analysis_session::analysis_session(const operation_catalog& catalog)
    : catalog_(catalog) {}

runtime_result analysis_session::run(const std::string& operation_id,
                                     const runtime_inputs& inputs,
                                     const runtime_config& config) {
    if (closed_) {
        throw std::runtime_error{"analysis session is closed"};
    }
    const auto& spec = catalog_.resolve(operation_id);
    const operation_request request(operation_id, inputs, config);
    const std::string key = operation_id + "|" + request.input_digest + "|" +
                            request.config_digest;
    if (spec.deterministic) {
        const auto cached = cache_.find(key);
        if (cached != cache_.end()) {
            runtime_result copy = cached->second;
            copy.metadata["cache"] = "hit";
            return copy;
        }
    }
    runtime_result result = fincore::run(operation_id, inputs, config, catalog_);
    if (!spec.deterministic) {
        return result;
    }
    result.metadata["cache"] = "miss";
    cache_.emplace(key, result);
    return result;
}

void analysis_session::clear() { cache_.clear(); }

void analysis_session::close() noexcept {
    cache_.clear();
    closed_ = true;
}

void artifact_bundle::add(std::string name, std::string bytes,
                          artifact_descriptor descriptor) {
    if (is_closed) {
        throw fincore_error{
            resource_lifecycle_error("artifact bundle is closed")};
    }
    if (name.empty()) {
        throw fincore_error{
            validation_error("artifact name must be a non-empty string")};
    }
    if (artifacts.find(name) != artifacts.end()) {
        throw fincore_error{
            validation_error("duplicate artifact name: " + name)};
    }
    artifacts.emplace(name, std::move(bytes));
    descriptors[name] = std::move(descriptor);
}

void write_artifact_bundle(const artifact_bundle& bundle,
                           const std::filesystem::path& output_directory) {
    if (bundle.closed()) {
        throw fincore_error{
            resource_lifecycle_error("artifact bundle is closed")};
    }
    std::map<std::string, std::filesystem::path> artifact_paths;
    std::set<std::filesystem::path> all_paths{
        std::filesystem::path(artifact_manifest_name)};
    for (const auto& entry : bundle.artifacts) {
        const auto relative_path = validate_artifact_name(entry.first);
        if (!all_paths.insert(relative_path).second) {
            throw input_error{"artifact path is reserved or duplicated"};
        }
        artifact_paths.emplace(entry.first, relative_path);
    }
    for (const auto& entry : bundle.descriptors) {
        if (bundle.artifacts.find(entry.first) == bundle.artifacts.end()) {
            throw input_error{
                "artifact descriptor must name an artifact in the bundle"};
        }
        validate_media_type(entry.second.media_type);
    }
    for (const auto& entry : artifact_paths) {
        auto parent = entry.second.parent_path();
        while (!parent.empty()) {
            if (all_paths.find(parent) != all_paths.end()) {
                throw input_error{
                    "artifact file path conflicts with another artifact"};
            }
            parent = parent.parent_path();
        }
    }

    const auto root = validate_output_directory(output_directory);
    if (path_entry_exists(root)) {
        throw input_error{"artifact output directory must not already exist"};
    }
    const auto manifest = artifact_manifest(bundle);
    std::vector<std::filesystem::path> created_paths;
    std::size_t capacity = bundle.artifacts.size() + 2U;
    for (const auto& entry : artifact_paths) {
        const auto parent = entry.second.parent_path();
        capacity += static_cast<std::size_t>(
            std::distance(parent.begin(), parent.end()));
    }
    created_paths.reserve(capacity);
    if (!std::filesystem::create_directory(root)) {
        throw input_error{"artifact output directory must not already exist"};
    }
    created_paths.push_back(root);
    try {
        const auto canonical_root = std::filesystem::canonical(root);
        for (const auto& entry : bundle.artifacts) {
            write_bundle_file(root, canonical_root,
                              artifact_paths.at(entry.first), entry.second,
                              created_paths);
        }
        write_bundle_file(root, canonical_root,
                          std::filesystem::path(artifact_manifest_name),
                          manifest, created_paths);
    } catch (...) {
        for (auto path = created_paths.rbegin();
             path != created_paths.rend(); ++path) {
            std::error_code error;
            std::filesystem::remove(*path, error);
        }
        throw;
    }
}

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
            const auto observations =
                require_non_negative_size(inputs, "observations");
            const auto exceptions =
                require_non_negative_size(inputs, "exceptions");
            const double confidence =
                require_number(inputs, "confidence_level");
            return runtime_value(
                kupiec_lr(observations, exceptions, confidence));
        });
    add("risk.calibration.basel_traffic_light", "risk",
        "basel_traffic_light",
        [](const runtime_inputs& inputs) {
            const auto exceptions =
                require_non_negative_size(inputs, "exceptions");
            const auto observations =
                require_non_negative_size(inputs, "observations");
            const double confidence =
                require_number(inputs, "confidence_level");
            return runtime_value(
                basel_traffic_light(exceptions, observations, confidence));
        });
    add("risk.calibration.expected_exception_count", "risk",
        "expected_exception_count",
        [](const runtime_inputs& inputs) {
            const auto observations =
                require_non_negative_size(inputs, "observations");
            const double confidence =
                require_number(inputs, "confidence_level");
            return runtime_value(
                expected_exception_count(observations, confidence));
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
    add("metrics.rolling.roll_sharpe_ratio", "metrics", "roll_sharpe_ratio",
        [](const runtime_inputs& inputs) {
            const auto window =
                optional_non_negative_size(inputs, "window");
            return runtime_value(roll_sharpe_ratio(
                require_frame(inputs, "returns"),
                window.value_or(252U)));
        });
    add("metrics.rolling.roll_sortino_ratio", "metrics",
        "roll_sortino_ratio",
        [](const runtime_inputs& inputs) {
            const auto window =
                optional_non_negative_size(inputs, "window");
            return runtime_value(roll_sortino_ratio(
                require_frame(inputs, "returns"),
                window.value_or(252U)));
        });
    add("metrics.rolling.roll_max_drawdown", "metrics", "roll_max_drawdown",
        [](const runtime_inputs& inputs) {
            const auto window =
                optional_non_negative_size(inputs, "window");
            return runtime_value(roll_max_drawdown(
                require_frame(inputs, "returns"),
                window.value_or(252U)));
        });
    add("metrics.rolling.roll_annual_volatility", "metrics",
        "roll_annual_volatility",
        [](const runtime_inputs& inputs) {
            const auto window = require_non_negative_size(inputs, "window");
            return runtime_value(roll_annual_volatility(
                require_frame(inputs, "returns"), window));
        });
    add("metrics.rolling.roll_beta", "metrics", "roll_beta",
        [](const runtime_inputs& inputs) {
            const auto window =
                optional_non_negative_size(inputs, "window");
            return runtime_value(roll_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                window.value_or(252U)));
        });
    add("metrics.rolling.rolling_beta", "metrics", "rolling_beta",
        [](const runtime_inputs& inputs) {
            const auto window =
                optional_non_negative_size(inputs, "rolling_window");
            return runtime_value(rolling_beta(
                require_frame(inputs, "returns"),
                require_frame(inputs, "benchmark"),
                window.value_or(126U)));
        });
    add("risk.diagnostics.walk_forward_var", "risk", "walk_forward_var",
        [](const runtime_inputs& inputs) {
            return runtime_value(walk_forward_var(
                require_frame(inputs, "returns"),
                require_risk_model_spec(inputs, "spec")));
        });
    add("risk.report.build_risk_validation_report", "risk",
        "build_risk_validation_report",
        [](const runtime_inputs& inputs) {
            return runtime_value(build_risk_validation_report(
                require_walk_forward_result(inputs, "result"),
                optional_string(inputs, "index_name")));
        });
    add("report.risk.build_risk_report", "report", "build_risk_report",
        [](const runtime_inputs& inputs) {
            const auto title = optional_string(inputs, "title");
            return runtime_value(build_risk_report(
                require_risk_validation_report(inputs, "report"),
                title.value_or("Risk Validation Report")));
        });
    add("report.portfolio.build_portfolio_report", "report",
        "build_portfolio_report",
        [](const runtime_inputs& inputs) {
            const auto title = optional_string(inputs, "title");
            const auto window =
                optional_non_negative_size(inputs, "rolling_window");
            return runtime_value(build_portfolio_report(
                require_frame(inputs, "returns"),
                optional_frame(inputs, "benchmark"),
                title.value_or("Portfolio Report"),
                optional_frame(inputs, "positions"), window.value_or(63U),
                optional_transactions(inputs, "transactions")));
        });
    const auto append = [&operations](std::vector<operation_spec> extra) {
        operations.insert(operations.end(),
                          std::make_move_iterator(extra.begin()),
                          std::make_move_iterator(extra.end()));
    };
    append(metrics_performance_operations());
    append(portfolio_risk_simulation_operations());
    append(attribution_factor_operations());
    return operation_catalog(std::move(operations));
}

}  // namespace fincore
