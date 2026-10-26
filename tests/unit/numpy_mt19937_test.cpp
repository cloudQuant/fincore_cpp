#include <fincore/detail/mt19937.hpp>
#include <fincore/metrics/bayesian.hpp>

#include <gtest/gtest.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace {

TEST(NumpyMt19937Test, MatchesRandomStateSeedZero) {
  fincore::detail::numpy_mt19937 engine{0U};
  std::vector<std::uint32_t> tens;
  for (int index = 0; index < 5; ++index) {
    tens.push_back(engine.randint(10U));
  }
  EXPECT_EQ(tens, (std::vector<std::uint32_t>{5U, 0U, 3U, 3U, 7U}));

  fincore::detail::numpy_mt19937 hundreds{0U};
  std::vector<std::uint32_t> values;
  for (int index = 0; index < 5; ++index) {
    values.push_back(hundreds.randint(100U));
  }
  EXPECT_EQ(values, (std::vector<std::uint32_t>{44U, 47U, 64U, 67U, 67U}));
}

TEST(NumpyMt19937Test, MatchesRandomStateSeedFortyTwo) {
  fincore::detail::numpy_mt19937 engine{42U};
  std::vector<std::uint32_t> values;
  for (int index = 0; index < 5; ++index) {
    values.push_back(engine.randint(10U));
  }
  EXPECT_EQ(values, (std::vector<std::uint32_t>{6U, 3U, 7U, 4U, 6U}));
}

TEST(NumpyMt19937Test, BoundedOneDoesNotConsumeADraw) {
  fincore::detail::numpy_mt19937 with_one{42U};
  fincore::detail::numpy_mt19937 without_one{42U};
  EXPECT_EQ(with_one.randint(1U), 0U);
  EXPECT_EQ(with_one.randint(10U), without_one.randint(10U));
}

TEST(NumpyMt19937Test, SimulatePathsReadsPinnedNumPyFixture) {
  const auto fixture = std::filesystem::path(__FILE__).parent_path().parent_path() /
                       "fixtures/iteration8/simulate_paths_seed42.csv";
  std::ifstream input{fixture};
  ASSERT_TRUE(input) << fixture;
  std::string header;
  ASSERT_TRUE(std::getline(input, header));
  std::string parameters;
  ASSERT_TRUE(std::getline(input, parameters));
  std::stringstream parameter_stream{parameters};
  std::uint32_t seed = 0U;
  std::size_t num_samples = 0U;
  std::size_t num_days = 0U;
  std::size_t population = 0U;
  char comma = '\0';
  parameter_stream >> seed >> comma >> num_samples >> comma >> num_days >> comma >>
      population;
  ASSERT_EQ(seed, 42U);
  ASSERT_EQ(population, 5U);
  ASSERT_EQ(num_samples, 3U);
  ASSERT_EQ(num_days, 4U);

  std::vector<std::uint32_t> indices;
  std::string line;
  while (std::getline(input, line)) {
    if (line.empty()) {
      continue;
    }
    std::stringstream row{line};
    std::uint32_t index = 0U;
    while (row >> index) {
      indices.push_back(index);
      if (row.peek() == ',') {
        row.get();
      }
    }
  }
  ASSERT_EQ(indices.size(), num_samples * num_days);

  const std::array<double, 5> returns{0.01, 0.02, 0.03, 0.04, 0.05};
  const auto paths = fincore::simulate_paths(returns, num_days, 1.0,
                                             num_samples, seed);
  for (std::size_t position = 0U; position < indices.size(); ++position) {
    const double expected = 0.01 * static_cast<double>(indices[position] + 1U);
    EXPECT_DOUBLE_EQ(paths.values()[position], expected);
  }
}

TEST(NumpyMt19937Test, SimulatePathsMatchesRandomStateIndices) {
  const std::array<double, 5> returns{0.01, 0.02, 0.03, 0.04, 0.05};
  const auto paths = fincore::simulate_paths(returns, 4U, 1.0, 3U, 42U);
  ASSERT_EQ(paths.rows(), 3U);
  ASSERT_EQ(paths.columns(), 4U);
  const std::array<double, 12> expected{
      0.04, 0.05, 0.03, 0.05, 0.05, 0.02,
      0.03, 0.03, 0.03, 0.05, 0.04, 0.03};
  for (std::size_t index = 0U; index < expected.size(); ++index) {
    EXPECT_DOUBLE_EQ(paths.values()[index], expected[index]);
  }
}

}  // namespace
