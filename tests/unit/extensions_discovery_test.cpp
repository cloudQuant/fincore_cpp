#include <fincore/errors.hpp>
#include <fincore/extensions.hpp>

#include <gtest/gtest.h>

#include <array>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace {

namespace ext = fincore::extensions;

ext::discovered_extension make_manifest(std::string name, std::string group,
                                        std::string distribution,
                                        std::string value) {
  ext::discovered_extension manifest;
  manifest.name = std::move(name);
  manifest.group = std::move(group);
  manifest.distribution = std::move(distribution);
  manifest.value = std::move(value);
  return manifest;
}

template <typename Function>
void expect_validation_error(Function&& function) {
  try {
    function();
    ADD_FAILURE() << "expected fincore_error";
  } catch (const fincore::fincore_error& error) {
    EXPECT_EQ(error.category(), "ValidationError");
    EXPECT_EQ(error.code(), "fincore.validation");
  }
}

TEST(ExtensionDiscoveryTest, ExposesSourceEntryPointGroups) {
  EXPECT_EQ(ext::ENTRY_POINT_GROUPS.size(), 1U);
  EXPECT_EQ(ext::ENTRY_POINT_GROUPS[0], "fincore.extensions");
  const auto groups = ext::entry_point_groups();
  ASSERT_EQ(groups.size(), 1U);
  EXPECT_EQ(groups[0], "fincore.extensions");
}

TEST(ExtensionDiscoveryTest, DiscoversNormalizesAndOrdersManifests) {
  const std::vector<ext::discovered_extension> manifests{
      make_manifest("zeta", "fincore.extensions", "dist-z", "libz:make"),
      make_manifest(" alpha ", " fincore.extensions ", " dist-a ",
                    " liba:make "),
      make_manifest("beta", "other.group", "dist-b", "libb:make"),
      make_manifest("beta2", "fincore.extensions", "dist-b", "libb:make")};

  const auto discovered = ext::discover_extensions(manifests);
  ASSERT_EQ(discovered.size(), 3U);
  EXPECT_EQ(discovered[0].name, "alpha");
  EXPECT_EQ(discovered[0].group, "fincore.extensions");
  EXPECT_EQ(discovered[0].distribution, "dist-a");
  EXPECT_EQ(discovered[0].value, "liba:make");
  EXPECT_EQ(discovered[1].name, "beta2");
  EXPECT_EQ(discovered[2].name, "zeta");
}

TEST(ExtensionDiscoveryTest, FiltersRequestedGroupsAndPreservesDuplicates) {
  const std::vector<ext::discovered_extension> manifests{
      make_manifest("alpha", "fincore.extensions", "dist-a", "liba:make"),
      make_manifest("beta", "other.group", "dist-b", "libb:make")};

  const std::array<std::string_view, 1> other_groups{"other.group"};
  const auto other = ext::discover_extensions(manifests, other_groups);
  ASSERT_EQ(other.size(), 1U);
  EXPECT_EQ(other[0].name, "beta");

  const std::array<std::string_view, 1> no_groups{"none.group"};
  EXPECT_TRUE(ext::discover_extensions(manifests, no_groups).empty());

  const auto duplicate = make_manifest(
      "alpha", "fincore.extensions", "dist-a", "liba:make");
  const auto duplicated =
      ext::discover_extensions(std::vector<ext::discovered_extension>{
          duplicate, duplicate});
  EXPECT_EQ(duplicated.size(), 2U);
}

TEST(ExtensionDiscoveryTest, RejectsMalformedManifests) {
  const auto valid = make_manifest("alpha", "fincore.extensions", "dist",
                                   "lib:make");
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.name = "  ";
    static_cast<void>(
        ext::discover_extensions(std::vector<ext::discovered_extension>{
            invalid}));
  });
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.group.clear();
    static_cast<void>(
        ext::discover_extensions(std::vector<ext::discovered_extension>{
            invalid}));
  });
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.distribution = "\t";
    static_cast<void>(
        ext::discover_extensions(std::vector<ext::discovered_extension>{
            invalid}));
  });
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.value.clear();
    static_cast<void>(
        ext::discover_extensions(std::vector<ext::discovered_extension>{
            invalid}));
  });
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.name.clear();
    invalid.group = "unrequested.group";
    static_cast<void>(
        ext::discover_extensions(std::vector<ext::discovered_extension>{
            invalid}));
  });
}

TEST(ExtensionDiscoveryTest, ManifestJsonAndFingerprintAreStable) {
  const auto manifest = make_manifest("alpha", "fincore.extensions", "dist-a",
                                      "liba:make");
  EXPECT_EQ(manifest.to_json(),
            "{\"distribution\":\"dist-a\",\"group\":\"fincore.extensions\","
            "\"name\":\"alpha\",\"value\":\"liba:make\"}");
  EXPECT_EQ(manifest.fingerprint(),
            "c67ef388afc38e487f3ff64befa79e1641344a2a0b317af9afed197e0c74b069");
  EXPECT_EQ(manifest.fingerprint(), manifest.fingerprint());

  auto changed = manifest;
  changed.value = "liba:other";
  EXPECT_NE(changed.fingerprint(), manifest.fingerprint());
}

TEST(ExtensionDiscoveryTest, LoadValidatesAndNormalizesManifest) {
  const auto manifest = make_manifest(" alpha ", " fincore.extensions ",
                                      " dist-a ", " liba:make ");
  const auto loaded = ext::load_extension(manifest);
  EXPECT_EQ(loaded.manifest.name, "alpha");
  EXPECT_EQ(loaded.manifest.group, "fincore.extensions");
  EXPECT_EQ(loaded.manifest.distribution, "dist-a");
  EXPECT_EQ(loaded.manifest.value, "liba:make");
  EXPECT_EQ(loaded.fingerprint,
            "c67ef388afc38e487f3ff64befa79e1641344a2a0b317af9afed197e0c74b069");
  EXPECT_EQ(loaded.fingerprint, loaded.manifest.fingerprint());

  const auto custom = make_manifest("beta", "custom.group", "dist-b",
                                    "libb:make");
  EXPECT_EQ(ext::load_extension(custom).manifest.group, "custom.group");
}

TEST(ExtensionDiscoveryTest, LoadRejectsMalformedManifestsAndValues) {
  const auto valid = make_manifest("alpha", "fincore.extensions", "dist",
                                   "lib:make");
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.name = " ";
    static_cast<void>(ext::load_extension(invalid));
  });
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.group.clear();
    static_cast<void>(ext::load_extension(invalid));
  });
  expect_validation_error([&] {
    auto invalid = valid;
    invalid.distribution.clear();
    static_cast<void>(ext::load_extension(invalid));
  });

  for (const std::string& value :
       {"libplugin", "lib:", ":make", "a:b:c", "a b:c", "a:\tb"}) {
    auto invalid = valid;
    invalid.value = value;
    expect_validation_error(
        [&] { static_cast<void>(ext::load_extension(invalid)); });
  }
}

}  // namespace
