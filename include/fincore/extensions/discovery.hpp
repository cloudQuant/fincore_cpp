#pragma once

#include <fincore/extensions/snapshot.hpp>

#include <array>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore::extensions {

/// Source group names from the pinned `fincore.extensions.discovery` module.
/// The native ABI replaces `importlib.metadata` entry points with explicitly
/// supplied manifests while keeping the same group label.
inline constexpr std::array<std::string_view, 1> ENTRY_POINT_GROUPS{
    "fincore.extensions"};

/// Returns the source entry-point group names used by default.
///
/// @return View of ENTRY_POINT_GROUPS; valid for the process lifetime.
[[nodiscard]] std::span<const std::string_view> entry_point_groups() noexcept;

/// One unexecuted native extension manifest. Field names mirror
/// `DiscoveredExtension`; `value` uses the native `component:symbol`
/// convention instead of a Python import path.
struct discovered_extension {
  std::string name;                          ///< Extension name.
  std::string group = "fincore.extensions";  ///< Entry-point group label.
  std::string distribution;                  ///< Supplying distribution.
  std::string value;                         ///< `component:symbol` target.

  /// Returns the stable SHA-256 of to_json().
  [[nodiscard]] std::string fingerprint() const;
  /// Returns compact, key-sorted, ASCII-escaped JSON for the manifest fields.
  [[nodiscard]] std::string to_json() const;
};

/// Native name for an input manifest supplied by a host application.
using extension_manifest = discovered_extension;

/// Result of validating one discovered manifest. The native ABI does not
/// import code or call `dlopen`; this record is the load validation boundary.
struct loaded_extension {
  discovered_extension manifest;  ///< Normalized, validated manifest.
  std::string fingerprint;        ///< SHA-256 of the normalized manifest.
};

/// Discovers and deterministically orders manifests for the requested groups.
///
/// Each manifest is normalized by trimming its four text fields. A blank
/// required field is rejected even when the manifest's group is not requested.
/// Manifests whose normalized group is not in `groups` are ignored. Results are
/// stably sorted by `(group, name, distribution, value)`, so duplicates are
/// preserved in input order.
///
/// @param manifests Candidate manifests.
/// @param groups Requested entry-point groups; defaults to
///     entry_point_groups().
/// @return Normalized manifests in deterministic order.
/// @throws fincore_error With category `ValidationError` when any candidate has
///     a blank name, group, distribution or value.
[[nodiscard]] std::vector<discovered_extension> discover_extensions(
    std::span<const discovered_extension> manifests,
    std::span<const std::string_view> groups = entry_point_groups());

/// Discovers and orders manifests from a vector.
///
/// @param manifests Candidate manifests.
/// @param groups Requested entry-point groups; defaults to
///     entry_point_groups().
/// @return Normalized manifests in deterministic order.
/// @throws fincore_error With category `ValidationError` when any candidate has
///     a blank name, group, distribution or value.
[[nodiscard]] std::vector<discovered_extension> discover_extensions(
    const std::vector<discovered_extension>& manifests,
    std::span<const std::string_view> groups = entry_point_groups());

/// Validates and normalizes one manifest for explicit loading.
///
/// The value must use the `component:symbol` form with exactly one colon,
/// non-empty parts and no ASCII whitespace. The returned manifest is trimmed
/// and fingerprinted. No dynamic library is opened and no code is loaded.
///
/// @param extension Manifest to validate.
/// @return Normalized manifest and its SHA-256 fingerprint.
/// @throws fincore_error With category `ValidationError` when a text field is
///     blank or the value has the wrong shape.
[[nodiscard]] loaded_extension load_extension(
    const discovered_extension& extension);

}  // namespace fincore::extensions
