#include <fincore/version.hpp>

namespace fincore {

std::string_view version() noexcept { return pinned_version_string; }

}  // namespace fincore
