#include <fincore/errors.hpp>

#include <gtest/gtest.h>

#include <cmath>
#include <stdexcept>
#include <vector>

namespace {

TEST(ErrorsTest, ValidationFactoryPreservesFields) {
  const auto info = fincore::validation_error("bad input", "window", "0");
  EXPECT_EQ(info.category, "ValidationError");
  EXPECT_EQ(info.code, "fincore.validation");
  EXPECT_EQ(info.message, "bad input");
  EXPECT_EQ(info.fields.at("param_name"), "window");
  EXPECT_EQ(info.fields.at("value"), "0");
}

TEST(ErrorsTest, DictRoundTripPreservesCategoryAndFields) {
  const auto original = fincore::insufficient_data_error("too short", 10, 3);
  const auto dict = original.to_dict();
  EXPECT_EQ(dict.at("error_type"), "InsufficientDataError");
  EXPECT_EQ(dict.at("required_length"), "10");
  EXPECT_EQ(dict.at("actual_length"), "3");

  const auto restored = fincore::error_info::from_dict(dict);
  EXPECT_EQ(restored.category, original.category);
  EXPECT_EQ(restored.code, original.code);
  EXPECT_EQ(restored.message, original.message);
  EXPECT_EQ(restored.fields, original.fields);

  const auto exception = fincore::fincore_error::from_dict(dict);
  EXPECT_EQ(exception.category(), "InsufficientDataError");
  EXPECT_STREQ(exception.what(), "too short");
}

TEST(ErrorsTest, FromDictRejectsMissingErrorType) {
  EXPECT_THROW(fincore::error_info::from_dict({{"message", "x"}}),
               std::invalid_argument);
}

TEST(ErrorsTest, JsonEscapesMessageAndFields) {
  const auto info = fincore::validation_error("quote \" and\nnewline",
                                              "param", "a,b");
  const std::string json = info.to_json();
  EXPECT_NE(json.find("\\\" and\\nnewline"), std::string::npos);
  EXPECT_NE(json.find("\"category\":\"ValidationError\""),
            std::string::npos);
}

TEST(ErrorsTest, RuntimeDiagnosticCarriesReport) {
  const auto info = fincore::runtime_diagnostic_error(
      "failed", "resolution_failure", "op", "cap", {{"k", "v"}},
      "retry");
  EXPECT_EQ(info.category, "RuntimeDiagnosticError");
  EXPECT_EQ(info.fields.at("runtime_category"), "resolution_failure");
  EXPECT_EQ(info.fields.at("operation_id"), "op");
  EXPECT_EQ(info.fields.at("capability_id"), "cap");
  EXPECT_EQ(info.fields.at("remediation"), "retry");
  EXPECT_EQ(info.report.at("k"), "v");
  const auto dict = info.to_dict();
  EXPECT_EQ(dict.at("report.k"), "v");
}

TEST(ErrorsTest, OperationResolutionMessageMatchesSourcePredicate) {
  const auto info = fincore::make_operation_resolution_error("metrics.none");
  EXPECT_EQ(info.category, "RuntimeDiagnosticError");
  EXPECT_EQ(info.message, "unknown operation_id: metrics.none");
  EXPECT_EQ(info.fields.at("operation_id"), "metrics.none");
}

TEST(ErrorsTest, ContextualErrorsCarryOperationContext) {
  const auto info = fincore::input_contract_error("bad", "op", "param",
                                                  "path", "profile");
  EXPECT_EQ(info.category, "InputContractError");
  EXPECT_EQ(info.fields.at("operation_id"), "op");
  EXPECT_EQ(info.fields.at("parameter"), "param");
  EXPECT_EQ(info.fields.at("path"), "path");
  EXPECT_EQ(info.fields.at("profile"), "profile");
}

TEST(ErrorsTest, SafeDivideSqrtAndNanHelpers) {
  EXPECT_DOUBLE_EQ(fincore::safe_divide(1.0, 4.0), 0.25);
  EXPECT_TRUE(std::isnan(fincore::safe_divide(1.0, 0.0)));
  EXPECT_DOUBLE_EQ(fincore::safe_divide(1.0, 0.0, -1.0), -1.0);
  EXPECT_DOUBLE_EQ(fincore::safe_sqrt(9.0), 3.0);
  EXPECT_TRUE(std::isnan(fincore::safe_sqrt(-1.0)));
  EXPECT_DOUBLE_EQ(fincore::safe_sqrt(-1.0, 0.0), 0.0);
  EXPECT_DOUBLE_EQ(fincore::ensure_not_nan(2.0, "x"), 2.0);
  EXPECT_DOUBLE_EQ(
      fincore::ensure_not_nan(std::nan(""), "x", 5.0), 5.0);
  EXPECT_THROW(fincore::ensure_not_nan(std::nan(""), "x"),
               fincore::fincore_error);
}

TEST(ErrorsTest, HandleNumericalErrorWrapsAndPassesThrough) {
  // Each catch hands back 1: without that sentinel a missing throw would skip
  // the block and its expectations would never run.
  const auto wrapped = [&]() -> int {
    try {
      return fincore::handle_numerical_error("kernel", []() -> double {
        throw std::domain_error("domain");
      });
    } catch (const fincore::fincore_error& error) {
      EXPECT_EQ(error.category(), "NumericalError");
      EXPECT_EQ(error.info().fields.at("operation"), "kernel");
      EXPECT_NE(std::string(error.what()).find("domain"), std::string::npos);
      return 1;
    }
  }();
  EXPECT_EQ(wrapped, 1);

  const auto passthrough = [&]() -> int {
    try {
      return fincore::handle_numerical_error("kernel", []() -> double {
        throw fincore::fincore_error{
            fincore::validation_error("already structured")};
      });
    } catch (const fincore::fincore_error& error) {
      EXPECT_EQ(error.category(), "ValidationError");
      return 1;
    }
  }();
  EXPECT_EQ(passthrough, 1);
}

TEST(ErrorsTest, CheckDependenciesListsAllMissing) {
  const std::vector<fincore::dependency_status> available{
      {"numpy", true, ""}, {"pandas", true, ""}};
  EXPECT_NO_THROW(fincore::check_dependencies(available));

  // numpy stays in the failing list as an available entry that must be left
  // out of the joined names, and extra carries only the first unavailable
  // entry's reason, never yfinance's.
  const std::vector<fincore::dependency_status> missing{
      {"numpy", true, ""}, {"pymc", false, "bayesian"},
      {"yfinance", false, "data"}};
  try {
    fincore::check_dependencies(missing);
    FAIL() << "expected DependencyError";
  } catch (const fincore::fincore_error& error) {
    EXPECT_EQ(error.category(), "DependencyError");
    EXPECT_EQ(error.info().fields.at("dependency"), "pymc, yfinance");
    EXPECT_EQ(error.info().fields.at("extra"), "bayesian");
    EXPECT_NE(std::string(error.what()).find("pymc, yfinance"),
              std::string::npos);
  }
}

}  // namespace
