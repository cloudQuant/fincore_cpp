#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace fincore::detail {

class sha256_hasher {
public:
    sha256_hasher();

    void update(std::span<const std::uint8_t> data);

    /// Finalizes a copy of the current state. The hasher is unchanged, so
    /// repeated calls return the same digest and update-after-finish continues
    /// the pre-finish message stream.
    [[nodiscard]] std::string finish_hex() const;

private:
    void process_block(const std::uint8_t* block);

    std::array<std::uint32_t, 8> state_;
    std::array<std::uint8_t, 64> buffer_{};
    std::size_t buffer_size_ = 0U;
    std::uint64_t total_bytes_ = 0U;
};

std::string sha256_hex(std::span<const std::uint8_t> payload);

}  // namespace fincore::detail
