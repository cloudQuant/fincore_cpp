#pragma once

#include <cstdint>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <variant>
#include <vector>

namespace fincore::extensions {

/// Native equivalent of the pinned Python `ExtensionHook` value. The Python
/// object derives a `module:qualname` fingerprint from the callback; the
/// native ABI requires that fingerprint explicitly because C++ callables have
/// no portable qualified name.
inline constexpr int default_hook_priority = 100;

/// One native hook argument. Python hooks accept arbitrary positional and
/// keyword arguments; the native ABI narrows this to one transformed value
/// plus named scalar keywords.
using extension_value =
    std::variant<std::monostate, bool, std::int64_t, double, std::string>;
using extension_keywords = std::map<std::string, extension_value>;

struct extension_hook_invocation {
  extension_value value;
  extension_keywords keywords;
};

/// A hook either returns a replacement for the first argument or nullopt to
/// leave it unchanged. Returning a value for a notification invocation is
/// ignored, matching the source `execute_hooks` contract.
using extension_hook_callback =
    std::function<std::optional<extension_value>(
        const extension_hook_invocation&)>;

/// Renderers are native callbacks that map a request key to a stable payload.
/// The payload format is owned by the plugin; the snapshot only owns
/// registration, ordering and fingerprinting.
using extension_renderer_callback =
    std::function<std::string(std::string_view request)>;

/// Trim ASCII whitespace around a registration name. Case is preserved, as in
/// the pinned source where names are case-sensitive. Returns an empty string
/// when the input only contains whitespace.
[[nodiscard]] std::string normalize_extension_name(std::string_view value);

class extension_hook {
 public:
  extension_hook(std::string event, std::string fingerprint,
                 extension_hook_callback callback,
                 int priority = default_hook_priority,
                 std::map<std::string, std::string> provenance = {});

  [[nodiscard]] const std::string& event() const noexcept { return event_; }
  [[nodiscard]] const std::string& fingerprint() const noexcept {
    return fingerprint_;
  }
  [[nodiscard]] int priority() const noexcept { return priority_; }
  [[nodiscard]] const std::map<std::string, std::string>& provenance()
      const noexcept {
    return provenance_;
  }
  [[nodiscard]] const extension_hook_callback& callback() const noexcept {
    return callback_;
  }

 private:
  std::string event_;
  std::string fingerprint_;
  extension_hook_callback callback_;
  int priority_ = default_hook_priority;
  std::map<std::string, std::string> provenance_;
};

class renderer_registration {
 public:
  renderer_registration(std::string name, std::string fingerprint,
                        extension_renderer_callback renderer,
                        std::map<std::string, std::string> provenance = {});

  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  [[nodiscard]] const std::string& fingerprint() const noexcept {
    return fingerprint_;
  }
  [[nodiscard]] const extension_renderer_callback& renderer() const noexcept {
    return renderer_;
  }
  [[nodiscard]] const std::map<std::string, std::string>& provenance()
      const noexcept {
    return provenance_;
  }

 private:
  std::string name_;
  std::string fingerprint_;
  extension_renderer_callback renderer_;
  std::map<std::string, std::string> provenance_;
};

/// Native extension operation descriptor. It mirrors the metadata subset of
/// `fincore.runtime.specs.OperationSpec` that contributes to
/// `ExtensionSnapshot.digest`; the callable and schemas stay in the runtime
/// catalog rather than in this overlay descriptor.
struct extension_operation {
  std::string operation_id;
  std::string capability_id;
  std::string domain = "extensions";
  std::string implementation_fingerprint;
  std::optional<std::string> optional_extra;
  bool deterministic = true;
  std::string rng_policy = "none";
  std::optional<std::string> semantic_mode;
  std::optional<std::string> mode_approval;
  std::map<std::string, std::string> provenance;

  /// Compact, key-sorted, ASCII-escaped JSON for the fields that participate
  /// in the snapshot digest. Stable across processes and platforms.
  [[nodiscard]] std::string to_json() const;
};

/// Immutable native snapshot of extension operations, hooks and renderers.
///
/// Ordering matches the pinned Python snapshot: operations by operation_id,
/// hooks grouped by event then by (priority, fingerprint), renderers by name.
/// Hooks are intentionally not duplicate-rejected (the source allows repeated
/// registrations); operations and renderers are.
class extension_snapshot {
 public:
  explicit extension_snapshot(
      std::vector<extension_operation> operations = {},
      std::vector<extension_hook> hooks = {},
      std::vector<renderer_registration> renderers = {});

  [[nodiscard]] const std::vector<extension_operation>& operations()
      const noexcept {
    return operations_;
  }
  [[nodiscard]] const std::vector<extension_hook>& hooks() const noexcept {
    return hooks_;
  }
  [[nodiscard]] const std::vector<renderer_registration>& renderers()
      const noexcept {
    return renderers_;
  }
  [[nodiscard]] const std::string& digest() const noexcept { return digest_; }

  /// Compact, key-sorted, ASCII-escaped JSON used to compute `digest`.
  [[nodiscard]] std::string to_json() const;

  [[nodiscard]] extension_snapshot with_operation(
      extension_operation operation) const;
  [[nodiscard]] extension_snapshot with_hook(extension_hook hook) const;
  [[nodiscard]] extension_snapshot with_renderer(
      renderer_registration renderer) const;

  [[nodiscard]] std::span<const extension_hook> hooks_for(
      std::string_view event) const;

  /// Run the priority-ordered hooks for `event`. When `first` is present the
  /// hooks transform it left-to-right and the final value is returned; when it
  /// is absent the hooks are notifications and a monostate value is returned.
  [[nodiscard]] extension_value execute_hooks(
      std::string_view event,
      std::optional<extension_value> first = std::nullopt,
      const extension_keywords& keywords = {}) const;

  [[nodiscard]] std::optional<extension_renderer_callback> renderer(
      std::string_view name) const;

 private:
  std::vector<extension_operation> operations_;
  std::vector<extension_hook> hooks_;
  std::vector<renderer_registration> renderers_;
  std::map<std::string, std::vector<extension_hook>> hooks_by_event_;
  std::map<std::string, std::size_t> renderer_index_;
  std::string digest_;
};

/// Builtin extension-domain operations. The pinned source returns an empty
/// tuple: third parties register namespaced operations through snapshots.
[[nodiscard]] std::vector<extension_operation> operations();

}  // namespace fincore::extensions
