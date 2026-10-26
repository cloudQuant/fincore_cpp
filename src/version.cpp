#include <fincore/version.hpp>

namespace fincore {

// The string is a compiled-in literal rather than a value resolved at run time, so
// what a binary reports is the version it was built from, and a stale build cannot
// present itself as a newer one by reading metadata from elsewhere.
std::string_view version() noexcept { return pinned_version_string; }

}  // namespace fincore
