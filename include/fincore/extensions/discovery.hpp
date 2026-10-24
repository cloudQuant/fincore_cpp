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

[[nodiscard]] std::span<const std::string_view> entry_point_groups() noexcept;

/// One unexecuted native extension manifest. Field names mirror
/// `DiscoveredExtension`; `value` uses the native `<component>:<symbol>`
/// convention instead of a Python import path.
struct discovered_extension {
  std::string name;
  std::string group = "fincore.extensions";
  std::string distribution;
  std::string value;

  /// Stable SHA-256 of the normalized manifest metadata.
  [[nodiscard]] std::string fingerprint() const;
  /// Compact, key-sorted, ASCII-escaped JSON for the manifest fields.
  [[nodiscard]] std::string to_json() const;
};

/// Native name for an input manifest supplied by a host application.
using extension_manifest = discovered_extension;

/// Result of validating one discovered manifest. The native ABI does not
/// import code or call `dlopen`; this record is the load validation boundary.
struct loaded_extension {
  discovered_extension manifest;
  std::string fingerprint;
};

/// Discover and deterministically order manifests for the requested groups.
/// Manifests outside `groups` are ignored; malformed manifests are rejected.
[[nodiscard]] std::vector<discovered_extension> discover_extensions(
    std::span<const discovered_extension> manifests,
    std::span<const std::string_view> groups = entry_point_groups());

[[nodiscard]] std::vector<discovered_extension> discover_extensions(
    const std::vector<discovered_extension>& manifests,
    std::span<const std::string_view> groups = entry_point_groups());

/// Validate one manifest for explicit loading. The returned record is
/// normalized and fingerprinted; no dynamic library is opened.
[[nodiscard]] loaded_extension load_extension(
    const discovered_extension& extension);

}  // namespace fincore::extensions
