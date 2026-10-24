#pragma once

#include <string_view>

namespace fincore {

// Native library version retained from the current C4 base. The pinned Python
// oracle version is exposed separately below so installed-consumer metadata
// checks can compare against the frozen Python pin without pretending the
// native ABI version changed.
inline constexpr int version_major = 0;
inline constexpr int version_minor = 1;
inline constexpr int version_patch = 0;
inline constexpr const char* version_string = "0.1.0";

// Pinned Python oracle identity (`fincore.__version__` at
// 3e9ef205ef2023f92e89fddd1052d46b16429c64).
inline constexpr int pinned_version_major = 0;
inline constexpr int pinned_version_minor = 5;
inline constexpr int pinned_version_patch = 2;
inline constexpr const char* pinned_version_string = "0.5.2.dev0";

/// The pinned oracle version as an owning-friendly view.
[[nodiscard]] std::string_view version() noexcept;

}  // namespace fincore
