#include <fincore/detail/mt19937.hpp>

namespace fincore::detail {

numpy_mt19937::numpy_mt19937(const std::uint32_t seed) {
  state_[0] = seed;
  for (std::size_t index = 1U; index < state_.size(); ++index) {
    // NumPy's init_genrand multiplier; the reference MT19937 value 69069U
    // would produce a different stream from the same seed.
    state_[index] =
        1812433253U * (state_[index - 1U] ^ (state_[index - 1U] >> 30U)) +
        static_cast<std::uint32_t>(index);
  }
  // Positioning at the buffer end forces the first draw to twist.
  index_ = state_.size();
}

void numpy_mt19937::generate() {
  constexpr std::uint32_t upper_mask = 0x80000000U;
  constexpr std::uint32_t lower_mask = 0x7fffffffU;
  for (std::size_t position = 0U; position < state_.size(); ++position) {
    const std::uint32_t value =
        (state_[position] & upper_mask) |
        (state_[(position + 1U) % state_.size()] & lower_mask);
    const std::uint32_t twisted =
        state_[(position + 397U) % state_.size()] ^ (value >> 1U) ^
        ((value & 1U) != 0U ? 0x9908b0dfU : 0U);
    state_[position] = twisted;
  }
  index_ = 0U;
}

std::uint32_t numpy_mt19937::next_uint32() {
  if (index_ >= state_.size()) {
    generate();
  }
  std::uint32_t value = state_[index_++];
  value ^= value >> 11U;
  value ^= (value << 7U) & 0x9d2c5680U;
  value ^= (value << 15U) & 0xefc60000U;
  value ^= value >> 18U;
  return value;
}

std::uint32_t numpy_mt19937::randint(const std::uint32_t upper_exclusive) {
  if (upper_exclusive <= 1U) {
    return 0U;
  }
  // Smallest mask covering the range, then reject overshoots: drawing
  // next_uint32() % upper_exclusive would bias low values whenever the bound
  // is not a power of two, and NumPy's bounded generator is mask-based.
  const std::uint32_t maximum = upper_exclusive - 1U;
  std::uint32_t mask = maximum;
  mask |= mask >> 1U;
  mask |= mask >> 2U;
  mask |= mask >> 4U;
  mask |= mask >> 8U;
  mask |= mask >> 16U;
  std::uint32_t value = 0U;
  do {
    value = next_uint32() & mask;
  } while (value > maximum);
  return value;
}

}  // namespace fincore::detail
