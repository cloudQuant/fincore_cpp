#include <fincore/extensions/snapshot.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/errors.hpp>

#include <algorithm>
#include <cstdint>
#include <set>
#include <span>
#include <utility>

namespace fincore::extensions {
namespace {

bool is_ascii_whitespace(const char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

bool is_blank(const std::string_view value) {
  return std::all_of(value.begin(), value.end(), is_ascii_whitespace);
}

[[noreturn]] void fail_validation(std::string message,
                                  std::string param_name = {}) {
  throw fincore_error{validation_error(std::move(message),
                                       std::move(param_name))};
}

std::string require_nonblank(std::string value, const std::string_view label,
                             const std::string_view param_name) {
  if (value.empty() || is_blank(value)) {
    fail_validation(std::string(label) + " must be a non-empty string",
                    std::string(param_name));
  }
  return value;
}

std::string require_normalized(std::string_view value,
                               const std::string_view label,
                               const std::string_view param_name) {
  auto normalized = normalize_extension_name(value);
  if (normalized.empty()) {
    fail_validation(std::string(label) + " must be a non-empty string",
                    std::string(param_name));
  }
  return normalized;
}

bool uses_extension_namespace(const std::string& value) {
  return value.rfind("extension.", 0U) == 0U &&
         std::count(value.begin(), value.end(), '.') >= 2;
}

void validate_operation(const extension_operation& operation) {
  if (operation.operation_id.empty() || is_blank(operation.operation_id) ||
      !uses_extension_namespace(operation.operation_id)) {
    fail_validation("extension operation_id must use the extension namespace",
                    "operation_id");
  }
  if (operation.capability_id.empty() || is_blank(operation.capability_id) ||
      !uses_extension_namespace(operation.capability_id)) {
    fail_validation("extension capability_id must use the extension namespace",
                    "capability_id");
  }
  if (operation.domain != "extensions") {
    fail_validation("extension operations must use domain='extensions'",
                    "domain");
  }
  if (operation.implementation_fingerprint.empty() ||
      is_blank(operation.implementation_fingerprint)) {
    fail_validation(
        "extension implementation_fingerprint must be a non-empty string",
        "implementation_fingerprint");
  }
  if (operation.rng_policy.empty() || is_blank(operation.rng_policy)) {
    fail_validation("rng_policy must be a non-empty string", "rng_policy");
  }
  if (operation.optional_extra.has_value() &&
      (operation.optional_extra->empty() ||
       is_blank(*operation.optional_extra))) {
    fail_validation("optional_extra must be a non-empty string",
                    "optional_extra");
  }
  if (operation.semantic_mode.has_value() !=
      operation.mode_approval.has_value()) {
    fail_validation(
        "semantic_mode and mode_approval must be provided together",
        "semantic_mode");
  }
  if (operation.semantic_mode.has_value() &&
      (operation.semantic_mode->empty() || is_blank(*operation.semantic_mode))) {
    fail_validation("semantic_mode must be a non-empty string",
                    "semantic_mode");
  }
  if (operation.mode_approval.has_value() &&
      (operation.mode_approval->empty() || is_blank(*operation.mode_approval))) {
    fail_validation("mode_approval must be a non-empty string",
                    "mode_approval");
  }
}

void append_key(std::string& output, bool& first, const std::string_view key) {
  if (!first) {
    output.push_back(',');
  }
  first = false;
  detail::append_json_string(output, key);
  output.push_back(':');
}

void append_string_field(std::string& output, bool& first,
                         const std::string_view key,
                         const std::string_view value) {
  append_key(output, first, key);
  detail::append_json_string(output, value);
}

void append_optional_string_field(std::string& output, bool& first,
                                  const std::string_view key,
                                  const std::optional<std::string>& value) {
  append_key(output, first, key);
  if (value.has_value()) {
    detail::append_json_string(output, *value);
  } else {
    output += "null";
  }
}

void append_bool_field(std::string& output, bool& first,
                       const std::string_view key, const bool value) {
  append_key(output, first, key);
  output += value ? "true" : "false";
}

void append_integer_field(std::string& output, bool& first,
                          const std::string_view key,
                          const std::int64_t value) {
  append_key(output, first, key);
  output += std::to_string(value);
}

std::string canonical_operation_json(const extension_operation& operation) {
  std::string output;
  output.push_back('{');
  bool first = true;
  append_string_field(output, first, "capability_id",
                      operation.capability_id);
  append_bool_field(output, first, "deterministic", operation.deterministic);
  append_string_field(output, first, "domain", operation.domain);
  append_string_field(output, first, "implementation_fingerprint",
                      operation.implementation_fingerprint);
  append_optional_string_field(output, first, "mode_approval",
                               operation.mode_approval);
  append_string_field(output, first, "operation_id", operation.operation_id);
  append_optional_string_field(output, first, "optional_extra",
                               operation.optional_extra);
  append_string_field(output, first, "rng_policy", operation.rng_policy);
  append_optional_string_field(output, first, "semantic_mode",
                               operation.semantic_mode);
  output.push_back('}');
  return output;
}

std::string canonical_hook_json(const extension_hook& hook) {
  std::string output;
  output.push_back('{');
  bool first = true;
  append_string_field(output, first, "event", hook.event());
  append_string_field(output, first, "fingerprint", hook.fingerprint());
  append_integer_field(output, first, "priority", hook.priority());
  output.push_back('}');
  return output;
}

std::string canonical_renderer_json(
    const renderer_registration& renderer) {
  std::string output;
  output.push_back('{');
  bool first = true;
  append_string_field(output, first, "fingerprint", renderer.fingerprint());
  append_string_field(output, first, "name", renderer.name());
  output.push_back('}');
  return output;
}

std::string canonical_snapshot_json(
    const std::vector<extension_operation>& operations,
    const std::vector<extension_hook>& hooks,
    const std::vector<renderer_registration>& renderers) {
  std::string output;
  output.push_back('{');
  bool first = true;

  append_key(output, first, "hooks");
  output.push_back('[');
  for (std::size_t index = 0; index < hooks.size(); ++index) {
    if (index != 0U) {
      output.push_back(',');
    }
    output += canonical_hook_json(hooks[index]);
  }
  output.push_back(']');

  append_key(output, first, "operations");
  output.push_back('[');
  for (std::size_t index = 0; index < operations.size(); ++index) {
    if (index != 0U) {
      output.push_back(',');
    }
    output += canonical_operation_json(operations[index]);
  }
  output.push_back(']');

  append_key(output, first, "renderers");
  output.push_back('[');
  for (std::size_t index = 0; index < renderers.size(); ++index) {
    if (index != 0U) {
      output.push_back(',');
    }
    output += canonical_renderer_json(renderers[index]);
  }
  output.push_back(']');

  output.push_back('}');
  return output;
}

std::string sha256_text(const std::string_view text) {
  return detail::sha256_hex(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

}  // namespace

std::string normalize_extension_name(const std::string_view value) {
  std::size_t first = 0U;
  while (first < value.size() && is_ascii_whitespace(value[first])) {
    ++first;
  }
  std::size_t last = value.size();
  while (last > first && is_ascii_whitespace(value[last - 1U])) {
    --last;
  }
  return std::string(value.substr(first, last - first));
}

extension_hook::extension_hook(
    std::string event, std::string fingerprint,
    extension_hook_callback callback, const int priority,
    std::map<std::string, std::string> provenance)
    : event_(require_normalized(event, "event", "event")),
      fingerprint_(require_nonblank(std::move(fingerprint), "fingerprint",
                                    "fingerprint")),
      callback_(std::move(callback)),
      priority_(priority),
      provenance_(std::move(provenance)) {
  if (!callback_) {
    fail_validation("extension target must be callable", "callback");
  }
}

renderer_registration::renderer_registration(
    std::string name, std::string fingerprint,
    extension_renderer_callback renderer,
    std::map<std::string, std::string> provenance)
    : name_(require_normalized(name, "renderer name", "name")),
      fingerprint_(require_nonblank(std::move(fingerprint), "fingerprint",
                                    "fingerprint")),
      renderer_(std::move(renderer)),
      provenance_(std::move(provenance)) {
  if (!renderer_) {
    fail_validation("extension target must be callable", "renderer");
  }
}

std::string extension_operation::to_json() const {
  return canonical_operation_json(*this);
}

extension_snapshot::extension_snapshot(
    std::vector<extension_operation> operations,
    std::vector<extension_hook> hooks,
    std::vector<renderer_registration> renderers)
    : operations_(std::move(operations)), hooks_(std::move(hooks)) {
  std::set<std::string> operation_ids;
  for (const auto& operation : operations_) {
    validate_operation(operation);
    if (!operation_ids.insert(operation.operation_id).second) {
      fail_validation("duplicate extension operation_id: " +
                          operation.operation_id,
                      "operation_id");
    }
  }
  std::stable_sort(operations_.begin(), operations_.end(),
                   [](const extension_operation& left,
                      const extension_operation& right) {
                     return left.operation_id < right.operation_id;
                   });

  std::map<std::string, std::vector<extension_hook>> grouped_hooks;
  for (auto& hook : hooks_) {
    grouped_hooks[hook.event()].push_back(std::move(hook));
  }
  std::vector<extension_hook> flattened;
  for (auto& [event, event_hooks] : grouped_hooks) {
    std::stable_sort(event_hooks.begin(), event_hooks.end(),
                     [](const extension_hook& left,
                        const extension_hook& right) {
                       if (left.priority() != right.priority()) {
                         return left.priority() < right.priority();
                       }
                       return left.fingerprint() < right.fingerprint();
                     });
    for (const auto& hook : event_hooks) {
      flattened.push_back(hook);
    }
  }
  hooks_ = std::move(flattened);
  hooks_by_event_ = std::move(grouped_hooks);

  std::map<std::string, renderer_registration> ordered_renderers;
  for (auto& renderer : renderers) {
    const auto inserted =
        ordered_renderers.emplace(renderer.name(), std::move(renderer));
    if (!inserted.second) {
      fail_validation("duplicate renderer registration: " +
                          inserted.first->first,
                      "name");
    }
  }
  renderers_.reserve(ordered_renderers.size());
  for (auto& [name, renderer] : ordered_renderers) {
    renderer_index_[name] = renderers_.size();
    renderers_.push_back(std::move(renderer));
  }

  digest_ = sha256_text(canonical_snapshot_json(operations_, hooks_,
                                                 renderers_));
}

std::string extension_snapshot::to_json() const {
  return canonical_snapshot_json(operations_, hooks_, renderers_);
}

extension_snapshot extension_snapshot::with_operation(
    extension_operation operation) const {
  auto operations_copy = operations_;
  operations_copy.push_back(std::move(operation));
  return extension_snapshot{std::move(operations_copy), hooks_, renderers_};
}

extension_snapshot extension_snapshot::with_hook(
    extension_hook hook) const {
  auto hooks_copy = hooks_;
  hooks_copy.push_back(std::move(hook));
  return extension_snapshot{operations_, std::move(hooks_copy), renderers_};
}

extension_snapshot extension_snapshot::with_renderer(
    renderer_registration renderer) const {
  auto renderers_copy = renderers_;
  renderers_copy.push_back(std::move(renderer));
  return extension_snapshot{operations_, hooks_, std::move(renderers_copy)};
}

std::span<const extension_hook> extension_snapshot::hooks_for(
    const std::string_view event) const {
  const auto normalized = normalize_extension_name(event);
  const auto found = hooks_by_event_.find(normalized);
  if (found == hooks_by_event_.end()) {
    return {};
  }
  return std::span<const extension_hook>(found->second);
}

extension_value extension_snapshot::execute_hooks(
    const std::string_view event, std::optional<extension_value> first,
    const extension_keywords& keywords) const {
  const auto hooks = hooks_for(event);
  if (!first.has_value()) {
    const extension_hook_invocation invocation{std::monostate{}, keywords};
    for (const auto& hook : hooks) {
      static_cast<void>(hook.callback()(invocation));
    }
    return std::monostate{};
  }

  auto current = std::move(*first);
  for (const auto& hook : hooks) {
    const extension_hook_invocation invocation{current, keywords};
    auto transformed = hook.callback()(invocation);
    if (transformed.has_value()) {
      current = std::move(*transformed);
    }
  }
  return current;
}

std::optional<extension_renderer_callback> extension_snapshot::renderer(
    const std::string_view name) const {
  const auto normalized = normalize_extension_name(name);
  if (normalized.empty()) {
    return std::nullopt;
  }
  const auto found = renderer_index_.find(normalized);
  if (found == renderer_index_.end()) {
    return std::nullopt;
  }
  return renderers_[found->second].renderer();
}

std::vector<extension_operation> operations() { return {}; }

}  // namespace fincore::extensions
