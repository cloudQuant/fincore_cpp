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
///
/// Hooks use priority 100 unless another value is supplied; lower priorities
/// run first.
inline constexpr int default_hook_priority = 100;

/// One native hook argument. Python hooks accept arbitrary positional and
/// keyword arguments; the native ABI narrows this to one transformed value
/// plus named scalar keywords.
using extension_value =
    std::variant<std::monostate, bool, std::int64_t, double, std::string>;
/// Named scalar arguments passed to every hook callback.
using extension_keywords = std::map<std::string, extension_value>;

/// Payload passed to a hook callback.
struct extension_hook_invocation {
  extension_value value;        ///< Current value; monostate for notifications.
  extension_keywords keywords;  ///< Named scalar arguments.
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

/// Trims ASCII whitespace around a registration name. Case is preserved, as in
/// the pinned source where names are case-sensitive. Returns an empty string
/// when the input only contains whitespace.
///
/// @param value Text to trim.
/// @return Trimmed text, or an empty string when only whitespace was supplied.
[[nodiscard]] std::string normalize_extension_name(std::string_view value);

/// A registered hook for one event.
///
/// The event is trimmed when stored; the fingerprint is stored verbatim and
/// must be non-blank. The callback must be callable. Hooks with the same event
/// and priority are ordered by fingerprint inside extension_snapshot.
class extension_hook {
 public:
  /// Creates a hook.
  ///
  /// @param event Event name; surrounding ASCII whitespace is trimmed.
  /// @param fingerprint Stable hook fingerprint; must not be blank.
  /// @param callback Callback invoked by execute_hooks(); must be callable.
  /// @param priority Lower values run first; defaults to
  ///     default_hook_priority.
  /// @param provenance Free-form provenance metadata.
  /// @throws fincore_error With category `ValidationError` when `event` or
  ///     `fingerprint` is blank, or `callback` is empty.
  extension_hook(std::string event, std::string fingerprint,
                 extension_hook_callback callback,
                 int priority = default_hook_priority,
                 std::map<std::string, std::string> provenance = {});

  /// Returns the normalized event name.
  [[nodiscard]] const std::string& event() const noexcept { return event_; }
  /// Returns the fingerprint as supplied.
  [[nodiscard]] const std::string& fingerprint() const noexcept {
    return fingerprint_;
  }
  /// Returns the hook priority.
  [[nodiscard]] int priority() const noexcept { return priority_; }
  /// Returns the provenance metadata.
  [[nodiscard]] const std::map<std::string, std::string>& provenance()
      const noexcept {
    return provenance_;
  }
  /// Returns the callback.
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

/// A registered renderer callback.
///
/// The name is trimmed when stored; the fingerprint is stored verbatim and must
/// be non-blank. Renderer names are unique within a snapshot.
class renderer_registration {
 public:
  /// Creates a renderer registration.
  ///
  /// @param name Renderer name; surrounding ASCII whitespace is trimmed.
  /// @param fingerprint Stable renderer fingerprint; must not be blank.
  /// @param renderer Callback invoked by the host; must be callable.
  /// @param provenance Free-form provenance metadata.
  /// @throws fincore_error With category `ValidationError` when `name` or
  ///     `fingerprint` is blank, or `renderer` is empty.
  renderer_registration(std::string name, std::string fingerprint,
                        extension_renderer_callback renderer,
                        std::map<std::string, std::string> provenance = {});

  /// Returns the normalized renderer name.
  [[nodiscard]] const std::string& name() const noexcept { return name_; }
  /// Returns the fingerprint as supplied.
  [[nodiscard]] const std::string& fingerprint() const noexcept {
    return fingerprint_;
  }
  /// Returns the renderer callback.
  [[nodiscard]] const extension_renderer_callback& renderer() const noexcept {
    return renderer_;
  }
  /// Returns the provenance metadata.
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
  std::string operation_id;  ///< `extension.`-prefixed id with at least two
                             ///< dots.
  std::string capability_id;  ///< Capability id in the same namespace.
  std::string domain = "extensions";  ///< Domain; must be `extensions`.
  std::string implementation_fingerprint;  ///< Non-blank implementation
                                           ///< fingerprint.
  std::optional<std::string> optional_extra;  ///< Optional non-blank extra tag.
  bool deterministic = true;  ///< Whether the operation is deterministic.
  std::string rng_policy = "none";  ///< Non-blank RNG policy label.
  std::optional<std::string> semantic_mode;  ///< Optional semantic mode.
  std::optional<std::string> mode_approval;  ///< Optional mode approval.
  std::map<std::string, std::string> provenance;  ///< Metadata; excluded from
                                                  ///< the digest JSON.

  /// Returns compact, key-sorted, ASCII-escaped JSON for the fields that
  /// participate in the snapshot digest. Stable across processes and platforms.
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
  /// Creates a snapshot, validates entries, orders them and computes the
  /// digest.
  ///
  /// Operation and capability ids must start with `extension.` and contain at
  /// least two dots. Operations must have domain `extensions` and non-blank
  /// implementation fingerprints and RNG policies; operation ids must be
  /// unique, and `semantic_mode` and `mode_approval` must be supplied together.
  /// Hooks may repeat. Renderer names must be unique. Hooks are ordered by
  /// event, then priority and fingerprint; operations by operation_id;
  /// renderers by name. The digest is the SHA-256 of to_json().
  ///
  /// @param operations Operation descriptors.
  /// @param hooks Registered hooks.
  /// @param renderers Registered renderers.
  /// @throws fincore_error With category `ValidationError` for an invalid
  ///     operation, a duplicate operation id or a duplicate renderer name.
  explicit extension_snapshot(
      std::vector<extension_operation> operations = {},
      std::vector<extension_hook> hooks = {},
      std::vector<renderer_registration> renderers = {});

  /// Returns the operations ordered by operation_id.
  [[nodiscard]] const std::vector<extension_operation>& operations()
      const noexcept {
    return operations_;
  }
  /// Returns the hooks ordered by event, then priority and fingerprint.
  [[nodiscard]] const std::vector<extension_hook>& hooks() const noexcept {
    return hooks_;
  }
  /// Returns the renderers ordered by name.
  [[nodiscard]] const std::vector<renderer_registration>& renderers()
      const noexcept {
    return renderers_;
  }
  /// Returns the lowercase hexadecimal SHA-256 digest of to_json().
  [[nodiscard]] const std::string& digest() const noexcept { return digest_; }

  /// Returns compact, key-sorted, ASCII-escaped JSON used to compute `digest`.
  ///
  /// @return Canonical snapshot JSON.
  [[nodiscard]] std::string to_json() const;

  /// Returns a new snapshot with one more operation.
  ///
  /// The receiver is unchanged. The operation is validated as in the
  /// constructor, so a duplicate operation id is rejected.
  ///
  /// @param operation Operation to add.
  /// @return New snapshot containing all existing entries plus `operation`.
  /// @throws fincore_error With category `ValidationError` when the operation
  ///     is invalid or its id is already registered.
  [[nodiscard]] extension_snapshot with_operation(
      extension_operation operation) const;

  /// Returns a new snapshot with one more hook.
  ///
  /// The receiver is unchanged. Repeated hook registrations are allowed.
  ///
  /// @param hook Hook to add.
  /// @return New snapshot containing all existing entries plus `hook`.
  [[nodiscard]] extension_snapshot with_hook(extension_hook hook) const;

  /// Returns a new snapshot with one more renderer.
  ///
  /// The receiver is unchanged. A duplicate renderer name is rejected.
  ///
  /// @param renderer Renderer to add.
  /// @return New snapshot containing all existing entries plus `renderer`.
  /// @throws fincore_error With category `ValidationError` when the name is
  ///     already registered.
  [[nodiscard]] extension_snapshot with_renderer(
      renderer_registration renderer) const;

  /// Returns the hooks registered for `event`.
  ///
  /// `event` is normalized before lookup. The returned hooks are in execution
  /// order: ascending priority, then ascending fingerprint.
  ///
  /// @param event Event name.
  /// @return View of matching hooks; empty when none are registered.
  [[nodiscard]] std::span<const extension_hook> hooks_for(
      std::string_view event) const;

  /// Runs the priority-ordered hooks for `event`. When `first` is present the
  /// hooks transform it left-to-right and the final value is returned; when it
  /// is absent the hooks are notifications and a monostate value is returned.
  ///
  /// Exceptions thrown by a callback propagate to the caller.
  ///
  /// @param event Event name; normalized before lookup.
  /// @param first Value transformed by the hooks, or nullopt for a
  ///     notification.
  /// @param keywords Named scalar arguments passed to every callback.
  /// @return Final transformed value, or monostate for a notification.
  [[nodiscard]] extension_value execute_hooks(
      std::string_view event,
      std::optional<extension_value> first = std::nullopt,
      const extension_keywords& keywords = {}) const;

  /// Looks up a renderer by name.
  ///
  /// `name` is normalized before lookup; a blank or unknown name yields
  /// nullopt.
  ///
  /// @param name Renderer name.
  /// @return Copy of the renderer callback, or nullopt when not registered.
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

/// Returns the builtin extension-domain operations.
///
/// The pinned source returns an empty tuple: third parties register namespaced
/// operations through snapshots.
///
/// @return An empty vector.
[[nodiscard]] std::vector<extension_operation> operations();

}  // namespace fincore::extensions
