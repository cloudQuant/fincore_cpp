#include <fincore/runtime.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <limits>
#include <map>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

struct oracle_summary {
  std::string test_case;
  std::string frame;
  std::vector<fincore::timestamp> index;
  std::vector<std::string> columns;
};

struct oracle_cell {
  std::string test_case;
  std::string frame;
  std::size_t row{};
  fincore::timestamp index{};
  std::string column;
  double value{};
};

struct oracle_error {
  std::string test_case;
  std::string message;
};

struct oracle_stddev {
  std::string test_case;
  std::vector<double> values;
  std::size_t ddof{};
  double result{};
};

std::vector<std::string> split(const std::string &value, const char delimiter) {
  std::vector<std::string> fields;
  std::stringstream stream{value};
  std::string field;
  while (std::getline(stream, field, delimiter)) {
    if (!field.empty() && field.back() == '\r') {
      field.pop_back();
    }
    fields.push_back(std::move(field));
  }
  if (!value.empty() && value.back() == delimiter) {
    fields.emplace_back();
  }
  return fields;
}

std::vector<oracle_summary> read_summaries() {
  const auto path =
      std::filesystem::path{__FILE__}.parent_path().parent_path() /
      "fixtures/runtime_utilities/alignment_summary.csv";
  std::ifstream input{path};
  if (!input.is_open()) {
    throw std::runtime_error{"could not open runtime alignment summary"};
  }
  std::vector<oracle_summary> result;
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    const auto fields = split(line, ',');
    if (fields.size() != 4U) {
      throw std::runtime_error{"invalid runtime alignment summary row"};
    }
    oracle_summary summary{fields[0], fields[1], {}, {}};
    if (!fields[2].empty()) {
      for (const auto &value : split(fields[2], ';')) {
        summary.index.push_back(std::stoll(value));
      }
    }
    if (!fields[3].empty()) {
      summary.columns = split(fields[3], ';');
    }
    result.push_back(std::move(summary));
  }
  return result;
}

std::vector<oracle_cell> read_cells() {
  const auto path =
      std::filesystem::path{__FILE__}.parent_path().parent_path() /
      "fixtures/runtime_utilities/alignment_oracle.csv";
  std::ifstream input{path};
  if (!input.is_open()) {
    throw std::runtime_error{"could not open runtime alignment oracle"};
  }
  std::vector<oracle_cell> result;
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    const auto fields = split(line, ',');
    if (fields.size() != 6U) {
      throw std::runtime_error{"invalid runtime alignment oracle row"};
    }
    result.push_back({fields[0], fields[1],
                      static_cast<std::size_t>(std::stoull(fields[2])),
                      std::stoll(fields[3]), fields[4], std::stod(fields[5])});
  }
  return result;
}

std::vector<oracle_error> read_errors() {
  const auto path =
      std::filesystem::path{__FILE__}.parent_path().parent_path() /
      "fixtures/runtime_utilities/alignment_errors.csv";
  std::ifstream input{path};
  if (!input.is_open()) {
    throw std::runtime_error{"could not open runtime alignment error oracle"};
  }
  std::vector<oracle_error> result;
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    const auto fields = split(line, ',');
    if (fields.size() != 4U) {
      throw std::runtime_error{"invalid runtime alignment error row"};
    }
    result.push_back({fields[0], fields[3]});
  }
  return result;
}

std::vector<oracle_stddev> read_stddev_cases() {
  const auto path =
      std::filesystem::path{__FILE__}.parent_path().parent_path() /
      "fixtures/runtime_utilities/numeric_stddev.csv";
  std::ifstream input{path};
  if (!input.is_open()) {
    throw std::runtime_error{"could not open runtime numeric oracle"};
  }
  std::vector<oracle_stddev> result;
  std::string line;
  std::getline(input, line);
  while (std::getline(input, line)) {
    const auto fields = split(line, ',');
    if (fields.size() != 4U) {
      throw std::runtime_error{"invalid runtime numeric oracle row"};
    }
    oracle_stddev item{fields[0],
                       {},
                       static_cast<std::size_t>(std::stoull(fields[2])),
                       std::stod(fields[3])};
    for (const auto &value : split(fields[1], ';')) {
      item.values.push_back(std::stod(value));
    }
    result.push_back(std::move(item));
  }
  return result;
}

fincore::dataframe
make_frame(std::vector<fincore::timestamp> index,
           std::vector<std::pair<std::string, std::vector<double>>> columns) {
  fincore::dataframe result;
  result.load_index(std::move(index));
  for (auto &[name, values] : columns) {
    result.load_column<double>(name.c_str(), std::move(values),
                               hmdf::nan_policy::dont_pad_with_nans);
  }
  return result;
}

std::vector<fincore::dataframe> fixture_frames(const std::string &test_case) {
  using frame = fincore::dataframe;
  if (test_case == "strict_same_order") {
    return {
        make_frame({30, 10, 20},
                   {{"close", {3.0, NAN, 2.0}}, {"aux", {0.3, 0.1, 0.2}}}),
        make_frame({30, 10, 20}, {{"benchmark", {30.0, 10.0, 20.0}},
                                  {"weight", {1.0, 2.0, 3.0}}}),
    };
  }
  if (test_case == "inner_unsorted") {
    return {make_frame({30, 10, 20}, {{"close", {3.0, 1.0, 2.0}}}),
            make_frame({40, 30, 20}, {{"benchmark", {4.0, 3.0, 2.0}}})};
  }
  if (test_case == "outer_dropna_nan_and_inf") {
    return {
        make_frame({10, 20, 30, 50}, {{"close", {1.0, NAN, 3.0, INFINITY}},
                                      {"aux", {0.1, 0.2, NAN, 0.5}}}),
        make_frame({20, 30, 40, 50}, {{"benchmark", {2.0, 3.0, 4.0, 5.0}},
                                      {"quality", {20.0, 30.0, 40.0, 50.0}}})};
  }
  if (test_case == "all_empty") {
    return {make_frame({}, {{"close", {}}, {"aux", {}}}),
            make_frame({}, {{"benchmark", {}}})};
  }
  if (test_case == "outer_zero_columns_same") {
    return {make_frame({3, 1}, {}), make_frame({3, 1}, {})};
  }
  throw std::runtime_error{"unknown runtime alignment fixture"};
}

std::size_t frame_position(const std::string &name) {
  if (name == "left") {
    return 0U;
  }
  if (name == "right") {
    return 1U;
  }
  throw std::runtime_error{"unknown frame name in oracle"};
}

fincore::alignment_policy policy_for(const std::string &test_case) {
  if (test_case == "strict_same_order") {
    return fincore::alignment_policy::strict;
  }
  if (test_case == "inner_unsorted") {
    return fincore::alignment_policy::inner;
  }
  if (test_case == "outer_dropna_nan_and_inf" || test_case == "all_empty" ||
      test_case == "outer_zero_columns_same") {
    return fincore::alignment_policy::outer_dropna;
  }
  throw std::runtime_error{"unknown runtime alignment policy fixture"};
}

TEST(RuntimeUtilities, AlignmentMatchesPinnedPythonOracleExactlyByLabel) {
  const auto summaries = read_summaries();
  const auto cells = read_cells();
  ASSERT_EQ(summaries.size(), 10U);
  ASSERT_EQ(cells.size(), 20U);

  std::map<std::string, std::vector<fincore::dataframe>> actual_by_case;
  for (const auto &summary : summaries) {
    if (!actual_by_case.contains(summary.test_case)) {
      actual_by_case.emplace(
          summary.test_case,
          fincore::align_time_series(fixture_frames(summary.test_case),
                                     policy_for(summary.test_case)));
    }
    const auto &actual =
        actual_by_case.at(summary.test_case)[frame_position(summary.frame)];
    const auto &index = actual.get_index();
    ASSERT_EQ(index.size(), summary.index.size());
    EXPECT_TRUE(std::equal(index.begin(), index.end(), summary.index.begin()));

    const auto columns = actual.get_columns_info<double>();
    ASSERT_EQ(columns.size(), summary.columns.size());
    for (std::size_t column = 0U; column < columns.size(); ++column) {
      EXPECT_EQ(std::get<0>(columns[column]).c_str(), summary.columns[column]);
    }
  }

  for (const auto &cell : cells) {
    const auto &frame =
        actual_by_case.at(cell.test_case)[frame_position(cell.frame)];
    const auto &values = frame.get_column<double>(cell.column.c_str());
    ASSERT_LT(cell.row, values.size());
    EXPECT_EQ(frame.get_index()[cell.row], cell.index);
    if (std::isnan(cell.value)) {
      EXPECT_TRUE(std::isnan(values[cell.row]));
    } else if (std::isinf(cell.value)) {
      EXPECT_EQ(values[cell.row], cell.value);
    } else {
      EXPECT_NEAR(values[cell.row], cell.value, 1e-14);
    }
  }
}

TEST(RuntimeUtilities,
     DenseArrayValidationCopiesAndChecksShapeAndFiniteValues) {
  const fincore::dense_array source{{1.0, 2.0, 3.0, 4.0}, {2U, 2U}};
  auto validated = fincore::validate_finite_array(source, "matrix", 4, 2);
  EXPECT_EQ(validated.shape, (std::vector<std::size_t>{2U, 2U}));
  EXPECT_EQ(validated.values, source.values);
  validated.values[0] = 99.0;
  EXPECT_EQ(source.values[0], 1.0);

  const fincore::native_backend backend;
  const auto converted = backend.as_float_array(std::vector<int>{1, 2, 3});
  EXPECT_EQ(converted.shape, (std::vector<std::size_t>{3U}));
  EXPECT_EQ(converted.values, (std::vector<double>{1.0, 2.0, 3.0}));

  EXPECT_THROW(fincore::validate_finite_array({{1.0}, {}}, "scalar"),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{1.0}, {2U, 2U}}, "bad"),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{1.0}, {1U}}, "short", 2),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{1.0}, {1U}}, "rank", 1, 2),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{1.0}, {1U}}, "minimum", -1),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{1.0}, {1U}}, "rank", 1, -1),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{NAN}, {1U}}, "nan"),
               fincore::input_error);
  EXPECT_THROW(fincore::validate_finite_array({{INFINITY}, {1U}}, "infinite"),
               fincore::input_error);
  EXPECT_THROW(
      fincore::validate_finite_array(
          {{}, {std::numeric_limits<std::size_t>::max(), 2U}}, "overflow", 0),
      fincore::input_error);
}

TEST(RuntimeUtilities, NativeBackendMatchesSampleAndCumulativeNumerics) {
  const fincore::native_backend backend;
  const fincore::dense_array values{{1.0, 2.0, 3.0, 4.0}, {4U}};
  const auto product = backend.cumulative_product(values);
  EXPECT_EQ(product.values, (std::vector<double>{1.0, 2.0, 6.0, 24.0}));
  EXPECT_EQ(product.shape, values.shape);
  EXPECT_NEAR(backend.sample_standard_deviation(values), 1.2909944487358056,
              1e-14);
  EXPECT_EQ(backend.cumulative_product(std::vector<int>{2, 3, 4}).values,
            (std::vector<double>{2.0, 6.0, 24.0}));
  EXPECT_DOUBLE_EQ(backend.sample_standard_deviation(std::vector<int>{1, 2, 3}),
                   1.0);
  EXPECT_THROW(backend.sample_standard_deviation({{1.0}, {1U}}),
               fincore::input_error);
  EXPECT_THROW(backend.cumulative_product({{1.0, 2.0}, {1U, 2U}}),
               fincore::input_error);
}

TEST(RuntimeUtilities, SampleStandardDeviationMatchesPinnedNumpyReduction) {
  const auto cases = read_stddev_cases();
  ASSERT_EQ(cases.size(), 4U);
  constexpr double absolute_tolerance = 1e-14;
  constexpr double relative_tolerance = 0.0;
  const fincore::native_backend backend;
  for (const auto &test_case : cases) {
    ASSERT_EQ(test_case.ddof, 1U) << test_case.test_case;
    const auto actual = backend.sample_standard_deviation(
        fincore::dense_array{test_case.values, {test_case.values.size()}});
    if (std::isinf(test_case.result)) {
      EXPECT_TRUE(std::isinf(actual)) << test_case.test_case;
      EXPECT_EQ(std::signbit(actual), std::signbit(test_case.result))
          << test_case.test_case;
      continue;
    }
    ASSERT_TRUE(std::isfinite(actual)) << test_case.test_case;
    EXPECT_LE(std::abs(actual - test_case.result),
              absolute_tolerance +
                  relative_tolerance * std::abs(test_case.result))
        << test_case.test_case;
  }
}

TEST(RuntimeUtilities, MappingReturnsACopyAndRejectsEmptyKeys) {
  const std::map<std::string, int> input{{"alpha", 1}, {"beta", 2}};
  auto output = fincore::validate_mapping(input, "inputs");
  EXPECT_EQ(output, input);
  output["alpha"] = 9;
  EXPECT_EQ(input.at("alpha"), 1);
  EXPECT_THROW(
      fincore::validate_mapping(std::map<std::string, int>{{"", 1}}, "inputs"),
      fincore::input_error);
}

TEST(RuntimeUtilities, BinaryAlignmentSeparatesLabelledAndPositionalInputs) {
  auto left = make_frame({4, 2, 3}, {{"left", {4.0, 2.0, NAN}}});
  auto right = make_frame({3, 4, 5}, {{"right", {30.0, 40.0, 50.0}}});
  auto [aligned_left, aligned_right] =
      fincore::align_binary_metric_inputs(left, right);
  EXPECT_EQ(aligned_left.get_index(), (std::vector<fincore::timestamp>{3, 4}));
  EXPECT_EQ(aligned_right.get_index(), (std::vector<fincore::timestamp>{3, 4}));
  EXPECT_TRUE(std::isnan(left.get_column<double>("left")[2]));
  aligned_right.get_column<double>("right")[0] = 99.0;
  EXPECT_EQ(right.get_column<double>("right")[0], 30.0);

  const fincore::dense_array positional_left{{1.0, NAN, 3.0, 4.0}, {2U, 2U}};
  const fincore::dense_array positional_right{{5.0, 6.0}, {2U}};
  const auto positional = fincore::align_binary_metric_inputs(
      positional_left, positional_right, fincore::alignment_policy::strict);
  ASSERT_EQ(positional.first.values.size(), positional_left.values.size());
  for (std::size_t i = 0U; i < positional.first.values.size(); ++i) {
    if (std::isnan(positional_left.values[i])) {
      EXPECT_TRUE(std::isnan(positional.first.values[i]));
    } else {
      EXPECT_EQ(positional.first.values[i], positional_left.values[i]);
    }
  }
  EXPECT_EQ(positional.second.shape, positional_right.shape);
  EXPECT_THROW(fincore::align_binary_metric_inputs(
                   positional_left, fincore::dense_array{{1.0}, {1U}}),
               fincore::input_error);
}

TEST(RuntimeUtilities, DuplicateLabelsStrictMismatchAndUnsupportedTypesFail) {
  auto duplicate = make_frame({1, 1}, {{"x", {1.0, 2.0}}});
  auto ordinary = make_frame({1, 2}, {{"y", {1.0, 2.0}}});
  EXPECT_THROW(fincore::align_time_series({duplicate, ordinary},
                                          fincore::alignment_policy::inner),
               fincore::input_error);
  EXPECT_THROW(fincore::align_time_series(
                   {ordinary, make_frame({2, 1}, {{"y", {2.0, 1.0}}})},
                   fincore::alignment_policy::strict),
               fincore::input_error);

  fincore::dataframe unsupported;
  unsupported.load_index(std::vector<fincore::timestamp>{1});
  unsupported.load_column<int>("count", {1},
                               hmdf::nan_policy::dont_pad_with_nans);
  EXPECT_THROW(fincore::align_time_series({unsupported},
                                          fincore::alignment_policy::inner),
               fincore::input_error);
  EXPECT_THROW(fincore::align_time_series(
                   {ordinary}, static_cast<fincore::alignment_policy>(99)),
               fincore::input_error);
}

TEST(RuntimeUtilities, AlignmentErrorsMatchPinnedPythonOracle) {
  const auto errors = read_errors();
  ASSERT_EQ(errors.size(), 3U);
  for (const auto &expected : errors) {
    std::vector<fincore::dataframe> inputs;
    fincore::alignment_policy policy = fincore::alignment_policy::inner;
    if (expected.test_case == "duplicate_labels") {
      inputs = {make_frame({1, 1}, {{"x", {1.0, 2.0}}}),
                make_frame({1, 2}, {{"x", {1.0, 2.0}}})};
    } else if (expected.test_case == "strict_order_mismatch") {
      inputs = {make_frame({1, 2}, {{"x", {1.0, 2.0}}}),
                make_frame({2, 1}, {{"x", {2.0, 1.0}}})};
      policy = fincore::alignment_policy::strict;
    } else if (expected.test_case == "outer_zero_columns_missing_label") {
      inputs = {make_frame({1, 3}, {}), make_frame({2, 3}, {})};
      policy = fincore::alignment_policy::outer_dropna;
    } else {
      FAIL() << "unknown error fixture: " << expected.test_case;
    }

    try {
      static_cast<void>(fincore::align_time_series(inputs, policy));
      FAIL() << "expected alignment error for " << expected.test_case;
    } catch (const fincore::input_error &error) {
      EXPECT_EQ(error.what(), expected.message);
    }
  }
}

TEST(RuntimeUtilities, EmptyInputsAndUtcNormalizationAreDefined) {
  EXPECT_TRUE(
      fincore::align_time_series({}, fincore::alignment_policy::inner).empty());
  auto source = make_frame({2, 1}, {{"x", {2.0, 1.0}}});
  auto normalized = fincore::normalize_time_series_timezone(source, "utc");
  EXPECT_EQ(normalized.get_index(), source.get_index());
  EXPECT_EQ(normalized.get_column<double>("x"), source.get_column<double>("x"));
  normalized.get_column<double>("x")[0] = 99.0;
  EXPECT_EQ(source.get_column<double>("x")[0], 2.0);
  EXPECT_THROW(
      fincore::normalize_time_series_timezone(source, "America/New_York"),
      fincore::input_error);
}

} // namespace
