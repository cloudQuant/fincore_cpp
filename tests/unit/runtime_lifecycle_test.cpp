#include <fincore/errors.hpp>
#include <fincore/runtime/runtime.hpp>
#include <fincore/runtime/utilities.hpp>

#include <gtest/gtest.h>

#include <filesystem>
#include <string>
#include <vector>

namespace {

TEST(ArtifactBundleLifecycleTest, AddCloseAndSeal) {
  fincore::artifact_bundle bundle;
  bundle.add("report.html", "<html></html>",
             fincore::artifact_descriptor{"text/html", {{"kind", "report"}}});
  EXPECT_EQ(bundle.artifacts.at("report.html"), "<html></html>");
  EXPECT_EQ(bundle.descriptors.at("report.html").media_type, "text/html");
  EXPECT_FALSE(bundle.closed());
  bundle.close();
  EXPECT_TRUE(bundle.closed());
  bundle.close();
  EXPECT_TRUE(bundle.closed());

  EXPECT_THROW(bundle.add("late.txt", "x"), fincore::fincore_error);
  EXPECT_THROW(
      fincore::write_artifact_bundle(
          bundle, std::filesystem::temp_directory_path() / "i8-closed-bundle"),
      fincore::fincore_error);
}

TEST(ArtifactBundleLifecycleTest, RejectsEmptyAndDuplicateNames) {
  fincore::artifact_bundle bundle;
  EXPECT_THROW(bundle.add("", "x"), fincore::fincore_error);
  bundle.add("a.txt", "a");
  EXPECT_THROW(bundle.add("a.txt", "b"), fincore::fincore_error);
  EXPECT_EQ(bundle.artifacts.size(), 1U);
}

TEST(ArtifactBundleLifecycleTest, WritesUnclosedBundle) {
  fincore::artifact_bundle bundle;
  bundle.add("a.txt", "alpha");
  const auto directory =
      std::filesystem::current_path() / "i8-open-bundle";
  std::error_code error;
  std::filesystem::remove_all(directory, error);
  fincore::write_artifact_bundle(bundle, directory);
  EXPECT_TRUE(std::filesystem::exists(directory / "a.txt"));
  EXPECT_TRUE(std::filesystem::exists(directory / ".fincore-manifest.json"));
  std::filesystem::remove_all(directory, error);
}

TEST(OperationSpecTest, ImplementationFingerprintUsesKernelSymbol) {
  fincore::operation_spec spec;
  spec.operation_id = "metrics.alpha";
  spec.capability_id = "metrics.alpha";
  spec.domain = "metrics";
  spec.callable = [](const fincore::runtime_inputs&) {
    return fincore::runtime_value{1.0};
  };
  spec.provenance["kernel_symbol"] = "fincore::alpha";
  EXPECT_EQ(spec.implementation_fingerprint(), "fincore::alpha");

  spec.provenance.clear();
  EXPECT_EQ(spec.implementation_fingerprint(), "metrics:metrics.alpha");
}

TEST(OperationSpecTest, OperationsProviderReturnsStableDeclarations) {
  fincore::operation_spec spec;
  spec.operation_id = "metrics.alpha";
  spec.capability_id = "metrics.alpha";
  spec.domain = "metrics";
  spec.callable = [](const fincore::runtime_inputs&) {
    return fincore::runtime_value{1.0};
  };
  spec.provenance["kernel_symbol"] = "fincore::alpha";
  auto provider = fincore::make_operations_provider({spec});
  const auto first = provider();
  const auto second = provider();
  ASSERT_EQ(first.size(), 1U);
  ASSERT_EQ(second.size(), 1U);
  EXPECT_EQ(first[0].operation_id, "metrics.alpha");
  EXPECT_EQ(first[0].implementation_fingerprint(), "fincore::alpha");
}

TEST(OptionalModuleTest, AvailableIsNoOpAndMissingIsDependencyError) {
  EXPECT_NO_THROW(fincore::load_optional_module("numpy", true));
  try {
    fincore::load_optional_module("pymc", false, "pymc", "bayesian");
    FAIL() << "expected DependencyError";
  } catch (const fincore::fincore_error& error) {
    EXPECT_EQ(error.category(), "DependencyError");
    EXPECT_EQ(error.info().fields.at("dependency"), "pymc");
    EXPECT_EQ(error.info().fields.at("extra"), "bayesian");
    EXPECT_NE(std::string(error.what()).find("optional_dependency_missing"),
              std::string::npos);
  }
}

TEST(OperationCatalogExtensionsTest, SnapshotAndMergeRequireCallables) {
  fincore::operation_spec base;
  base.operation_id = "metrics.alpha";
  base.capability_id = "metrics.alpha";
  base.domain = "metrics";
  base.callable = [](const fincore::runtime_inputs&) {
    return fincore::runtime_value{1.0};
  };
  base.provenance["kernel_symbol"] = "fincore::alpha";
  const fincore::operation_catalog catalog({base});
  EXPECT_FALSE(catalog.extension_snapshot().has_value());

  fincore::extensions::extension_operation extension;
  extension.operation_id = "extension.demo.echo";
  extension.capability_id = "extension.demo.echo";
  extension.domain = "extensions";
  extension.implementation_fingerprint = "demo:echo";
  extension.provenance["kernel_symbol"] = "demo:echo";
  const fincore::extensions::extension_snapshot extension_snapshot({extension});

  std::map<std::string,
           std::function<fincore::runtime_value(const fincore::runtime_inputs&)>>
      callables;
  callables["extension.demo.echo"] = [](const fincore::runtime_inputs&) {
    return fincore::runtime_value{42.0};
  };
  const auto merged = catalog.with_extensions(extension_snapshot, callables);
  EXPECT_EQ(merged.operation_ids().size(), 2U);
  ASSERT_TRUE(merged.extension_snapshot().has_value());
  EXPECT_EQ(merged.extension_snapshot()->digest(), extension_snapshot.digest());
  EXPECT_EQ(merged.resolve("extension.demo.echo").implementation_fingerprint(),
            "demo:echo");
  EXPECT_NE(merged.digest(), catalog.digest());

  EXPECT_THROW(catalog.with_extensions(extension_snapshot, {}),
               fincore::input_error);
  EXPECT_THROW(merged.with_extensions(extension_snapshot, callables),
               fincore::input_error);
}

}  // namespace
