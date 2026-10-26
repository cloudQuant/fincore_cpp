#include <fincore/errors.hpp>

#include <algorithm>
#include <cmath>
#include <sstream>

namespace fincore {
namespace {

// Control characters are escaped numerically while everything from 0x20 upward is
// copied through, including non-ASCII text: this payload is meant to be read as
// UTF-8 JSON rather than the ASCII-escaped form the extension snapshots use, so a
// message keeps the caller's wording byte for byte.
std::string json_escape(const std::string_view text) {
  std::string escaped;
  escaped.reserve(text.size() + 2U);
  for (const char character : text) {
    switch (character) {
      case '"':
        escaped += "\\\"";
        break;
      case '\\':
        escaped += "\\\\";
        break;
      case '\b':
        escaped += "\\b";
        break;
      case '\f':
        escaped += "\\f";
        break;
      case '\n':
        escaped += "\\n";
        break;
      case '\r':
        escaped += "\\r";
        break;
      case '\t':
        escaped += "\\t";
        break;
      default:
        if (static_cast<unsigned char>(character) < 0x20U) {
          std::ostringstream code;
          code << "\\u" << std::hex;
          code.width(4);
          code.fill('0');
          code << static_cast<int>(static_cast<unsigned char>(character));
          escaped += code.str();
        } else {
          escaped.push_back(character);
        }
    }
  }
  return escaped;
}

void append_map(std::ostringstream& output,
                const std::map<std::string, std::string>& values) {
  output << '{';
  bool first = true;
  for (const auto& [key, value] : values) {
    if (!first) {
      output << ',';
    }
    first = false;
    output << '"' << json_escape(key) << "\":\"" << json_escape(value)
           << '"';
  }
  output << '}';
}

std::string join_names(const std::vector<std::string>& names) {
  std::string joined;
  for (std::size_t index = 0U; index < names.size(); ++index) {
    if (index != 0U) {
      joined += ", ";
    }
    joined += names[index];
  }
  return joined;
}

std::string optional_number(const std::optional<std::int64_t> value) {
  return value.has_value() ? std::to_string(*value) : std::string{};
}

}  // namespace

// The flat dictionary starts from the free-form fields and then overwrites the
// reserved names, so a caller cannot shadow the category or message with a field of
// the same key; an empty code is the one exception, leaving whatever the caller put
// there. Nested report entries come in prefixed rather than colliding, and the map's
// key order is what makes the rendering deterministic.
std::map<std::string, std::string> error_info::to_dict() const {
  std::map<std::string, std::string> data = fields;
  data["error_type"] = category;
  data["message"] = message;
  if (!code.empty()) {
    data["code"] = code;
  }
  for (const auto& [key, value] : report) {
    data["report." + key] = value;
  }
  return data;
}

std::string error_info::to_json() const {
  std::ostringstream output;
  output << "{\"category\":\"" << json_escape(category)
         << "\",\"code\":\"" << json_escape(code)
         << "\",\"message\":\"" << json_escape(message)
         << "\",\"fields\":";
  append_map(output, fields);
  output << ",\"report\":";
  append_map(output, report);
  output << '}';
  return output.str();
}

error_info error_info::from_dict(
    const std::map<std::string, std::string>& data) {
  const auto type = data.find("error_type");
  if (type == data.end() || type->second.empty()) {
    throw std::invalid_argument("error dict must contain error_type");
  }
  error_info info;
  info.category = type->second;
  info.code = "fincore." + type->second;
  for (const auto& [key, value] : data) {
    if (key == "error_type") {
      continue;
    }
    if (key == "message") {
      info.message = value;
    } else if (key == "code") {
      if (!value.empty()) {
        info.code = value;
      }
    } else if (key.rfind("report.", 0U) == 0U) {
      info.report[key.substr(7U)] = value;
    } else {
      info.fields[key] = value;
    }
  }
  if (info.message.empty()) {
    const auto message = info.fields.find("message");
    if (message != info.fields.end()) {
      info.message = message->second;
    }
  }
  return info;
}

fincore_error::fincore_error(error_info info)
    : std::runtime_error(info.message), info_(std::move(info)) {}

fincore_error::fincore_error(std::string message)
    : std::runtime_error(message),
      info_(make_error("FincoreError", "fincore.error", std::move(message))) {}

fincore_error fincore_error::from_dict(
    const std::map<std::string, std::string>& data) {
  return fincore_error{error_info::from_dict(data)};
}

error_info make_error(std::string category, std::string code,
                      std::string message,
                      std::map<std::string, std::string> fields,
                      std::map<std::string, std::string> report) {
  error_info info;
  info.category = std::move(category);
  info.code = std::move(code);
  info.message = std::move(message);
  info.fields = std::move(fields);
  info.report = std::move(report);
  return info;
}

error_info validation_error(std::string message, std::string param_name,
                            std::string value) {
  std::map<std::string, std::string> fields;
  if (!param_name.empty()) {
    fields["param_name"] = std::move(param_name);
  }
  if (!value.empty()) {
    fields["value"] = std::move(value);
  }
  return make_error("ValidationError", "fincore.validation",
                    std::move(message), std::move(fields));
}

error_info insufficient_data_error(std::string message,
                                   std::optional<std::int64_t> required_length,
                                   std::optional<std::int64_t> actual_length) {
  std::map<std::string, std::string> fields;
  if (required_length.has_value()) {
    fields["required_length"] = optional_number(required_length);
  }
  if (actual_length.has_value()) {
    fields["actual_length"] = optional_number(actual_length);
  }
  return make_error("InsufficientDataError", "fincore.insufficient_data",
                    std::move(message), std::move(fields));
}

error_info invalid_period_error(std::string period) {
  std::map<std::string, std::string> fields;
  fields["period"] = period;
  fields["valid_periods"] = "daily,weekly,monthly,quarterly,yearly";
  return make_error(
      "InvalidPeriodError", "fincore.invalid_period",
      "Invalid period: '" + period + "'. Must be one of: daily, weekly, "
      "monthly, quarterly, yearly",
      std::move(fields));
}

error_info data_alignment_error(std::string message,
                                std::optional<std::int64_t> returns_length,
                                std::optional<std::int64_t> factor_length) {
  std::map<std::string, std::string> fields;
  if (returns_length.has_value()) {
    fields["returns_length"] = optional_number(returns_length);
  }
  if (factor_length.has_value()) {
    fields["factor_length"] = optional_number(factor_length);
  }
  return make_error("DataAlignmentError", "fincore.data_alignment",
                    std::move(message), std::move(fields));
}

error_info numerical_error(std::string message, std::string operation) {
  std::map<std::string, std::string> fields;
  if (!operation.empty()) {
    fields["operation"] = std::move(operation);
  }
  return make_error("NumericalError", "fincore.numerical", std::move(message),
                    std::move(fields));
}

error_info missing_data_error(std::string message,
                              std::string missing_field) {
  std::map<std::string, std::string> fields;
  if (!missing_field.empty()) {
    fields["missing_field"] = std::move(missing_field);
  }
  return make_error("MissingDataError", "fincore.missing_data",
                    std::move(message), std::move(fields));
}

error_info unsupported_format_error(std::string message,
                                    std::string format) {
  std::map<std::string, std::string> fields;
  if (!format.empty()) {
    fields["format"] = std::move(format);
  }
  return make_error("UnsupportedFormatError", "fincore.unsupported_format",
                    std::move(message), std::move(fields));
}

error_info dependency_error(std::string message, std::string dependency,
                            std::string extra) {
  std::map<std::string, std::string> fields;
  if (!dependency.empty()) {
    fields["dependency"] = std::move(dependency);
  }
  if (!extra.empty()) {
    fields["extra"] = std::move(extra);
  }
  return make_error("DependencyError", "fincore.dependency",
                    std::move(message), std::move(fields));
}

error_info runtime_diagnostic_error(
    std::string message, std::string runtime_category,
    std::string operation_id, std::string capability_id,
    std::map<std::string, std::string> details, std::string remediation) {
  std::map<std::string, std::string> fields;
  fields["runtime_category"] = std::move(runtime_category);
  if (!operation_id.empty()) {
    fields["operation_id"] = std::move(operation_id);
  }
  if (!capability_id.empty()) {
    fields["capability_id"] = std::move(capability_id);
  }
  if (!remediation.empty()) {
    fields["remediation"] = std::move(remediation);
  }
  return make_error("RuntimeDiagnosticError", "fincore.runtime.diagnostic",
                    std::move(message), std::move(fields),
                    std::move(details));
}

error_info make_operation_resolution_error(std::string operation_id) {
  error_info info = runtime_diagnostic_error(
      "unknown operation_id: " + operation_id, "resolution_failure",
      operation_id, {},
      {},
      "list catalog.operation_ids and select a registered operation");
  info.code = "fincore.runtime.operation_resolution";
  return info;
}

error_info contextual_error(std::string category, std::string message,
                            std::string operation_id, std::string parameter,
                            std::string path, std::string profile) {
  std::map<std::string, std::string> fields;
  if (!operation_id.empty()) {
    fields["operation_id"] = std::move(operation_id);
  }
  if (!parameter.empty()) {
    fields["parameter"] = std::move(parameter);
  }
  if (!path.empty()) {
    fields["path"] = std::move(path);
  }
  if (!profile.empty()) {
    fields["profile"] = std::move(profile);
  }
  return make_error(std::move(category), "fincore.contract",
                    std::move(message), std::move(fields));
}

error_info input_contract_error(std::string message, std::string operation_id,
                                std::string parameter, std::string path,
                                std::string profile) {
  return contextual_error("InputContractError", std::move(message),
                          std::move(operation_id), std::move(parameter),
                          std::move(path), std::move(profile));
}

error_info alignment_error(std::string message, std::string operation_id,
                           std::string parameter, std::string path,
                           std::string profile) {
  return contextual_error("AlignmentError", std::move(message),
                          std::move(operation_id), std::move(parameter),
                          std::move(path), std::move(profile));
}

error_info numerical_convergence_error(std::string message,
                                       std::string operation_id,
                                       std::string parameter, std::string path,
                                       std::string profile) {
  return contextual_error("NumericalConvergenceError", std::move(message),
                          std::move(operation_id), std::move(parameter),
                          std::move(path), std::move(profile));
}

error_info result_contract_error(std::string message, std::string operation_id,
                                 std::string parameter, std::string path,
                                 std::string profile) {
  return contextual_error("ResultContractError", std::move(message),
                          std::move(operation_id), std::move(parameter),
                          std::move(path), std::move(profile));
}

error_info resource_lifecycle_error(std::string message,
                                    std::string operation_id,
                                    std::string parameter, std::string path,
                                    std::string profile) {
  return contextual_error("ResourceLifecycleError", std::move(message),
                          std::move(operation_id), std::move(parameter),
                          std::move(path), std::move(profile));
}

void check_dependencies(std::span<const dependency_status> dependencies) {
  std::vector<std::string> missing;
  std::string extra;
  for (const auto& dependency : dependencies) {
    if (!dependency.available) {
      missing.push_back(dependency.name);
      if (extra.empty()) {
        extra = dependency.extra;
      }
    }
  }
  if (!missing.empty()) {
    throw fincore_error{dependency_error(
        "Missing required dependencies: " + join_names(missing),
        join_names(missing), std::move(extra))};
  }
}

double ensure_not_nan(double value, std::string_view param_name,
                      std::optional<double> replace_with) {
  if (std::isnan(value)) {
    if (replace_with.has_value()) {
      return *replace_with;
    }
    throw fincore_error{validation_error(
        std::string(param_name) + " cannot be NaN", std::string(param_name),
        "nan")};
  }
  return value;
}

double safe_divide(double numerator, double denominator, double default_value) {
  if (denominator == 0.0) {
    return default_value;
  }
  return numerator / denominator;
}

double safe_sqrt(double value, double default_value) {
  if (value < 0.0) {
    return default_value;
  }
  return std::sqrt(value);
}

}  // namespace fincore
