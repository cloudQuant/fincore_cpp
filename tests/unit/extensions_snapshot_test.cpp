#include <fincore/errors.hpp>
#include <fincore/extensions.hpp>

#include <gtest/gtest.h>

#include <map>
#include <optional>
#include <string>
#include <utility>
#include <vector>

namespace {

namespace ext = fincore::extensions;

ext::extension_operation make_operation(std::string operation_id,
                                        std::string fingerprint) {
  ext::extension_operation operation;
  operation.operation_id = std::move(operation_id);
  operation.capability_id = operation.operation_id;
  operation.implementation_fingerprint = std::move(fingerprint);
  return operation;
}

ext::extension_operation make_walk_operation() {
  ext::extension_operation operation;
  operation.operation_id = "extension.demo.walk";
  operation.capability_id = "extension.demo.walk";
  operation.implementation_fingerprint = "demo.plugin:walk";
  operation.optional_extra = "walk";
  operation.deterministic = false;
  operation.rng_policy = "seeded";
  operation.semantic_mode = "strict";
  operation.mode_approval = "approved";
  return operation;
}

ext::extension_hook make_transform_hook(std::string event,
                                        std::string fingerprint,
                                        const int priority,
                                        const std::string& suffix,
                                        std::vector<std::string>* order) {
  auto callback = [suffix, fingerprint, order](
                      const ext::extension_hook_invocation& invocation)
      -> std::optional<ext::extension_value> {
    if (order != nullptr) {
      order->push_back(fingerprint);
    }
    const auto* value = std::get_if<std::string>(&invocation.value);
    if (value == nullptr) {
      return std::nullopt;
    }
    return ext::extension_value{*value + suffix};
  };
  return ext::extension_hook{std::move(event), std::move(fingerprint),
                             std::move(callback), priority};
}

ext::renderer_registration make_renderer(std::string name,
                                         std::string fingerprint,
                                         std::string prefix) {
  auto callback = [prefix = std::move(prefix)](
                      const std::string_view request) {
    return prefix + std::string(request);
  };
  return ext::renderer_registration{std::move(name), std::move(fingerprint),
                                    std::move(callback)};
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

TEST(ExtensionSnapshotTest, HookAndRendererNormalizeNamesAndKeepProvenance) {
  std::vector<std::string> order;
  auto hook = make_transform_hook("  snapshot\t", " demo.plugin:hook ", 20,
                                  "!", &order);
  EXPECT_EQ(hook.event(), "snapshot");
  EXPECT_EQ(hook.fingerprint(), " demo.plugin:hook ");
  EXPECT_EQ(hook.priority(), 20);

  std::map<std::string, std::string> provenance{{"owner", "desk"}};
  ext::extension_hook with_provenance{"event", "fp", [](const auto&) {
                                        return std::optional<ext::extension_value>{};
                                      },
                                      7, provenance};
  provenance["owner"] = "mutated";
  EXPECT_EQ(with_provenance.provenance().at("owner"), "desk");

  auto renderer = make_renderer("  HTML\n", "demo.plugin:render_html", "r:");
  EXPECT_EQ(renderer.name(), "HTML");
  EXPECT_EQ(renderer.fingerprint(), "demo.plugin:render_html");
  EXPECT_EQ(renderer.renderer()("body"), "r:body");
}

TEST(ExtensionSnapshotTest, RejectsEmptyRegistrationsAndInvalidPriorityShape) {
  expect_validation_error([] {
    static_cast<void>(ext::extension_hook{
        " \t\n", "fp", [](const auto&) { return std::nullopt; }});
  });
  expect_validation_error([] {
    static_cast<void>(ext::extension_hook{
        "event", "   ", [](const auto&) { return std::nullopt; }});
  });
  expect_validation_error([] {
    static_cast<void>(ext::extension_hook{"event", "fp",
                                          ext::extension_hook_callback{}});
  });
  expect_validation_error([] {
    static_cast<void>(ext::renderer_registration{
        "   ", "fp", [](const std::string_view) { return std::string{}; }});
  });
  expect_validation_error([] {
    static_cast<void>(ext::renderer_registration{
        "name", "", [](const std::string_view) { return std::string{}; }});
  });
  expect_validation_error([] {
    static_cast<void>(ext::renderer_registration{
        "name", "fp", ext::extension_renderer_callback{}});
  });
}

TEST(ExtensionSnapshotTest, OrdersOperationsHooksAndRenderers) {
  auto snapshot = ext::extension_snapshot{
      {make_operation("extension.demo.zeta", "demo.plugin:zeta"),
       make_operation("extension.demo.alpha", "demo.plugin:alpha")},
      {make_transform_hook("snapshot", "demo.plugin:hook_b", 20, "", nullptr),
       make_transform_hook("align", "demo.plugin:hook_c", 100, "", nullptr),
       make_transform_hook("snapshot", "demo.plugin:hook_a", 10, "", nullptr)},
      {make_renderer("json", "demo.plugin:render_json", ""),
       make_renderer("html", "demo.plugin:render_html", "")}};

  ASSERT_EQ(snapshot.operations().size(), 2U);
  EXPECT_EQ(snapshot.operations()[0].operation_id, "extension.demo.alpha");
  EXPECT_EQ(snapshot.operations()[1].operation_id, "extension.demo.zeta");

  // Hook ordering is event-major, so the align hook leads even though its
  // priority is the largest in the snapshot; one global priority sort differs.
  ASSERT_EQ(snapshot.hooks().size(), 3U);
  EXPECT_EQ(snapshot.hooks()[0].event(), "align");
  EXPECT_EQ(snapshot.hooks()[1].fingerprint(), "demo.plugin:hook_a");
  EXPECT_EQ(snapshot.hooks()[2].fingerprint(), "demo.plugin:hook_b");

  ASSERT_EQ(snapshot.renderers().size(), 2U);
  EXPECT_EQ(snapshot.renderers()[0].name(), "html");
  EXPECT_EQ(snapshot.renderers()[1].name(), "json");

  const auto snapshot_hooks = snapshot.hooks_for(" snapshot ");
  ASSERT_EQ(snapshot_hooks.size(), 2U);
  EXPECT_EQ(snapshot_hooks[0].priority(), 10);
  EXPECT_EQ(snapshot_hooks[1].priority(), 20);
  EXPECT_TRUE(snapshot.hooks_for("missing").empty());
}

TEST(ExtensionSnapshotTest, RejectsDuplicateAndMalformedOperations) {
  auto operation = make_operation("extension.demo.run", "demo.plugin:run");
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{{operation, operation}});
  });

  auto bad_namespace = make_operation("extension.demo", "demo.plugin:run");
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{{bad_namespace}});
  });

  auto bad_capability = make_operation("extension.demo.run", "demo.plugin:run");
  bad_capability.capability_id = "metrics.demo.run";
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{{bad_capability}});
  });

  auto bad_domain = make_operation("extension.demo.run", "demo.plugin:run");
  bad_domain.domain = "metrics";
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{{bad_domain}});
  });

  auto missing_fingerprint =
      make_operation("extension.demo.run", "");
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{{missing_fingerprint}});
  });

  auto missing_mode = make_operation("extension.demo.run", "demo.plugin:run");
  missing_mode.semantic_mode = "strict";
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{{missing_mode}});
  });

  auto duplicate_renderer =
      make_renderer("html", "demo.plugin:render_html", "");
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{
        {}, {}, {duplicate_renderer, duplicate_renderer}});
  });
  expect_validation_error([&] {
    static_cast<void>(ext::extension_snapshot{
        {}, {},
        {make_renderer(" HTML", "demo.plugin:a", ""),
         make_renderer("HTML ", "demo.plugin:b", "")}});
  });
}

TEST(ExtensionSnapshotTest, DigestMatchesPinnedPythonCanonicalPayload) {
  auto snapshot = ext::extension_snapshot{
      {make_operation("extension.demo.walk", "demo.plugin:walk"),
       make_operation("extension.demo.run", "demo.plugin:run")},
      {make_transform_hook("snapshot", "demo.plugin:hook_b", 20, "", nullptr),
       make_transform_hook("align", "demo.plugin:hook_c", 100, "", nullptr),
       make_transform_hook("snapshot", "demo.plugin:hook_a", 10, "", nullptr)},
      {make_renderer("json", "demo.plugin:render_json", ""),
       make_renderer("html", "demo.plugin:render_html", "")}};
  // Replace the first walk operation with the full optional-mode fixture so
  // the payload exercises every digest field.
  auto operations = snapshot.operations();
  for (auto& operation : operations) {
    if (operation.operation_id == "extension.demo.walk") {
      operation = make_walk_operation();
    }
  }
  ext::extension_snapshot full{operations, snapshot.hooks(),
                               snapshot.renderers()};

  EXPECT_EQ(full.digest(),
            "b73196d1f19a85dcfeaa81afdb7c539d064c29d8820c3f43d8f8bcb4270bb933");
  EXPECT_EQ(full.to_json(),
            "{\"hooks\":[{\"event\":\"align\",\"fingerprint\":"
            "\"demo.plugin:hook_c\",\"priority\":100},{\"event\":\"snapshot\","
            "\"fingerprint\":\"demo.plugin:hook_a\",\"priority\":10},"
            "{\"event\":\"snapshot\",\"fingerprint\":\"demo.plugin:hook_b\","
            "\"priority\":20}],\"operations\":[{\"capability_id\":"
            "\"extension.demo.run\",\"deterministic\":true,\"domain\":"
            "\"extensions\",\"implementation_fingerprint\":\"demo.plugin:run\","
            "\"mode_approval\":null,\"operation_id\":\"extension.demo.run\","
            "\"optional_extra\":null,\"rng_policy\":\"none\","
            "\"semantic_mode\":null},{\"capability_id\":"
            "\"extension.demo.walk\",\"deterministic\":false,\"domain\":"
            "\"extensions\",\"implementation_fingerprint\":"
            "\"demo.plugin:walk\",\"mode_approval\":\"approved\","
            "\"operation_id\":\"extension.demo.walk\",\"optional_extra\":"
            "\"walk\",\"rng_policy\":\"seeded\",\"semantic_mode\":\"strict\"}],"
            "\"renderers\":[{\"fingerprint\":\"demo.plugin:render_html\","
            "\"name\":\"html\"},{\"fingerprint\":\"demo.plugin:render_json\","
            "\"name\":\"json\"}]}");

  const auto run = full.operations()[0];
  EXPECT_EQ(run.to_json(),
            "{\"capability_id\":\"extension.demo.run\",\"deterministic\":true,"
            "\"domain\":\"extensions\",\"implementation_fingerprint\":"
            "\"demo.plugin:run\",\"mode_approval\":null,\"operation_id\":"
            "\"extension.demo.run\",\"optional_extra\":null,\"rng_policy\":"
            "\"none\",\"semantic_mode\":null}");

  const ext::extension_snapshot empty{};
  EXPECT_EQ(empty.digest(),
            "7645271c3e8dcec1117c5000e4053c7047b047a2472f27ae217629897df3ce86");
  EXPECT_EQ(ext::extension_snapshot{}.digest(), empty.digest());
}

TEST(ExtensionSnapshotTest, DigestUsesAsciiEscapedStableJson) {
  // The renderer name arrives as raw UTF-8 bytes, so the pinned digest covers
  // the escaped payload rather than whatever the host text encoding emits.
  const ext::extension_snapshot snapshot{
      {}, {}, {make_renderer("\xCE\xB2-render", "demo.plugin:render", "")}};
  EXPECT_EQ(snapshot.digest(),
            "30a292bae48b4216033b69df7843cc044f6ba32bc4ca70aa1b502cdf3a541871");
}

TEST(ExtensionSnapshotTest, WithMethodsReturnNewImmutableSnapshots) {
  const ext::extension_snapshot base{
      {make_operation("extension.demo.run", "demo.plugin:run")},
      {make_transform_hook("event", "demo.plugin:hook", 100, "", nullptr)},
      {make_renderer("html", "demo.plugin:render_html", "")}};
  const auto base_digest = base.digest();

  const auto with_hook = base.with_hook(
      make_transform_hook("event", "demo.plugin:hook2", 10, "", nullptr));
  EXPECT_EQ(base.hooks().size(), 1U);
  EXPECT_EQ(base.digest(), base_digest);
  ASSERT_EQ(with_hook.hooks().size(), 2U);
  EXPECT_EQ(with_hook.hooks()[0].fingerprint(), "demo.plugin:hook2");
  EXPECT_NE(with_hook.digest(), base_digest);

  const auto with_operation = base.with_operation(
      make_operation("extension.demo.extra", "demo.plugin:extra"));
  EXPECT_EQ(base.operations().size(), 1U);
  ASSERT_EQ(with_operation.operations().size(), 2U);

  const auto with_renderer = base.with_renderer(
      make_renderer("json", "demo.plugin:render_json", ""));
  EXPECT_EQ(base.renderers().size(), 1U);
  ASSERT_EQ(with_renderer.renderers().size(), 2U);
  EXPECT_TRUE(base.renderer("json") == std::nullopt);
  EXPECT_TRUE(with_renderer.renderer("json").has_value());
}

TEST(ExtensionSnapshotTest, ExecutesHooksInPriorityAndFingerprintOrder) {
  std::vector<std::string> order;
  const ext::extension_snapshot snapshot{
      {},
      {make_transform_hook("transform", "demo.plugin:z", 20, "3", &order),
       make_transform_hook("transform", "demo.plugin:b", 10, "2", &order),
       make_transform_hook("transform", "demo.plugin:a", 10, "1", &order)},
      {}};

  const auto result =
      snapshot.execute_hooks("transform", ext::extension_value{std::string{"x"}});
  ASSERT_TRUE(std::holds_alternative<std::string>(result));
  EXPECT_EQ(std::get<std::string>(result), "x123");
  EXPECT_EQ(order,
            (std::vector<std::string>{"demo.plugin:a", "demo.plugin:b",
                                      "demo.plugin:z"}));
}

TEST(ExtensionSnapshotTest, ExecutesNotificationAndKeywordHooks) {
  std::size_t notifications = 0U;
  ext::extension_hook notify{
      "notify", "demo.plugin:notify",
      [&notifications](const ext::extension_hook_invocation& invocation) {
        ++notifications;
        EXPECT_TRUE(
            std::holds_alternative<std::monostate>(invocation.value));
        return std::optional<ext::extension_value>{
            ext::extension_value{std::string{"ignored"}}};
      }};
  const ext::extension_snapshot notification_snapshot{{}, {notify}, {}};
  const auto notification = notification_snapshot.execute_hooks("notify");
  EXPECT_TRUE(std::holds_alternative<std::monostate>(notification));
  EXPECT_EQ(notifications, 1U);

  ext::extension_hook keyword{
      "keyword", "demo.plugin:keyword",
      [](const ext::extension_hook_invocation& invocation)
          -> std::optional<ext::extension_value> {
        const auto found = invocation.keywords.find("tag");
        if (found == invocation.keywords.end()) {
          return std::nullopt;
        }
        return ext::extension_value{
            std::get<std::string>(found->second)};
      }};
  const ext::extension_snapshot keyword_snapshot{{}, {keyword}, {}};
  const auto keyword_result = keyword_snapshot.execute_hooks(
      "keyword", ext::extension_value{std::string{"value"}},
      {{"tag", ext::extension_value{std::string{"tagged"}}}});
  EXPECT_EQ(std::get<std::string>(keyword_result), "tagged");
  const auto unchanged = keyword_snapshot.execute_hooks(
      "keyword", ext::extension_value{std::string{"value"}});
  EXPECT_EQ(std::get<std::string>(unchanged), "value");
}

TEST(ExtensionSnapshotTest, ResolvesRenderersByNormalizedName) {
  const ext::extension_snapshot snapshot{
      {},
      {},
      {make_renderer(" HTML ", "demo.plugin:render_html", "html:")}};
  const auto renderer = snapshot.renderer(" HTML\n");
  ASSERT_TRUE(renderer.has_value());
  EXPECT_EQ((*renderer)("body"), "html:body");
  EXPECT_TRUE(snapshot.renderer("json") == std::nullopt);
  EXPECT_TRUE(snapshot.renderer("   ") == std::nullopt);
}

TEST(ExtensionSnapshotTest, BuiltinOperationsProviderIsEmpty) {
  EXPECT_TRUE(ext::operations().empty());
}

}  // namespace
