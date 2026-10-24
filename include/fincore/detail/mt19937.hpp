#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace fincore::detail {

// MT19937 with NumPy legacy RandomState seeding and bounded-integer drawing.
// This exists so seeded simulation paths match `np.random.RandomState(seed)
// .randint(0, n, size=...)` exactly for 32-bit integer seeds.
class numpy_mt19937 {
 public:
  explicit numpy_mt19937(std::uint32_t seed);

  std::uint32_t next_uint32();
  // Uniform integer in [0, upper_exclusive), using NumPy's masked rejection.
  // upper_exclusive == 1 returns 0 without consuming a draw, as rk_interval.
  std::uint32_t randint(std::uint32_t upper_exclusive);

 private:
  void generate();

  std::array<std::uint32_t, 624> state_{};
  std::size_t index_ = 624U;
};

}  // namespace fincore::detail
