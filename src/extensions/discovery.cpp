#include <fincore/extensions/discovery.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/errors.hpp>

#include <algorithm>
#include <cstdint>
#include <span>
#include <tuple>
#include <utility>

namespace fincore::extensions {
namespace {

bool is_ascii_whitespace(const char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

[[noreturn]] void fail_validation(std::string message,
                                  std::string param_name = {}) {
  throw fincore_error{validation_error(std::move(message),
                                       std::move(param_name))};
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

discovered_extension normalize_manifest(const discovered_extension& source) {
  discovered_extension normalized;
  normalized.name =
      require_normalized(source.name, "extension name", "name");
  normalized.group =
      require_normalized(source.group, "extension group", "group");
  normalized.distribution = require_normalized(
      source.distribution, "extension distribution", "distribution");
  normalized.value =
      require_normalized(source.value, "extension value", "value");
  return normalized;
}

void validate_entry_point_value(const std::string& value) {
  if (std::any_of(value.begin(), value.end(), is_ascii_whitespace)) {
    fail_validation("extension value must not contain whitespace", "value");
  }
  const auto separator = value.find(':');
  if (separator == std::string::npos || separator == 0U ||
      separator + 1U >= value.size() ||
      value.find(':', separator + 1U) != std::string::npos) {
    fail_validation(
        "extension value must use the '<component>:<symbol>' form", "value");
  }
}

bool group_requested(const std::string& group,
                     const std::span<const std::string_view> groups) {
  return std::any_of(groups.begin(), groups.end(),
                     [&group](const std::string_view requested) {
                       return normalize_extension_name(requested) == group;
                     });
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

std::string canonical_manifest_json(const discovered_extension& extension) {
  std::string output;
  output.push_back('{');
  bool first = true;
  append_string_field(output, first, "distribution", extension.distribution);
  append_string_field(output, first, "group", extension.group);
  append_string_field(output, first, "name", extension.name);
  append_string_field(output, first, "value", extension.value);
  output.push_back('}');
  return output;
}

std::string sha256_text(const std::string_view text) {
  return detail::sha256_hex(std::span<const std::uint8_t>(
      reinterpret_cast<const std::uint8_t*>(text.data()), text.size()));
}

}  // namespace

std::span<const std::string_view> entry_point_groups() noexcept {
  return ENTRY_POINT_GROUPS;
}

std::string discovered_extension::to_json() const {
  return canonical_manifest_json(*this);
}

std::string discovered_extension::fingerprint() const {
  return sha256_text(to_json());
}

std::vector<discovered_extension> discover_extensions(
    const std::span<const discovered_extension> manifests,
    const std::span<const std::string_view> groups) {
  std::vector<discovered_extension> discovered;
  discovered.reserve(manifests.size());
  for (const auto& manifest : manifests) {
    auto normalized = normalize_manifest(manifest);
    if (group_requested(normalized.group, groups)) {
      discovered.push_back(std::move(normalized));
    }
  }
  std::stable_sort(
      discovered.begin(), discovered.end(),
      [](const discovered_extension& left, const discovered_extension& right) {
        return std::tie(left.group, left.name, left.distribution,
                        left.value) < std::tie(right.group, right.name,
                                               right.distribution,
                                               right.value);
      });
  return discovered;
}

std::vector<discovered_extension> discover_extensions(
    const std::vector<discovered_extension>& manifests,
    const std::span<const std::string_view> groups) {
  return discover_extensions(
      std::span<const discovered_extension>(manifests), groups);
}

loaded_extension load_extension(const discovered_extension& extension) {
  auto manifest = normalize_manifest(extension);
  validate_entry_point_value(manifest.value);
  auto fingerprint = manifest.fingerprint();
  return loaded_extension{std::move(manifest), std::move(fingerprint)};
}

}  // namespace fincore::extensions
