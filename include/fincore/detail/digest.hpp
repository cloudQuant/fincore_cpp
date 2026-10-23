#pragma once

#include <cstdint>
#include <span>
#include <string>

namespace fincore::detail {

std::string sha256_hex(std::span<const std::uint8_t> payload);

}  // namespace fincore::detail
