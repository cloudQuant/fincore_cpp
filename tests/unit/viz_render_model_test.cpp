#include <fincore/report/html.hpp>
#include <fincore/viz.hpp>

#include <gtest/gtest.h>

#include <optional>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

namespace {

fincore::dataframe make_frame(std::vector<double> values,
                              const std::string &column) {
  std::vector<fincore::timestamp> index;
  index.reserve(values.size());
  for (std::size_t row = 0U; row < values.size(); ++row) {
    index.push_back(static_cast<fincore::timestamp>(row + 1U));
  }
  return fincore::make_series(std::move(index), column, std::move(values));
}

fincore::render_model make_model() {
  auto returns = make_frame({0.01, -0.02}, "returns");
  auto factor_returns = make_frame({0.03, 0.04}, "factor_returns");
  return fincore::render_model{std::move(returns), std::move(factor_returns),
                               fincore::frequency::weekly};
}

fincore::report_document titled_document(std::string title) {
  fincore::report_document document;
  document.domain = "custom-viz";
  document.title = std::move(title);
  return document;
}

TEST(RendererSnapshot, NormalizesNameAndDispatchesCustomCallbackOnce) {
  std::size_t callback_count = 0U;
  const auto model = make_model();
  const auto *caller_model = &model;
  fincore::renderer_snapshot snapshot({
      {" \tCuStOm\r\n",
       [&](const fincore::render_model &received) -> fincore::report_document {
         ++callback_count;
         EXPECT_NE(&received, caller_model);
         EXPECT_TRUE(received.factor_returns.has_value());
         EXPECT_EQ(received.period, fincore::frequency::weekly);
         EXPECT_DOUBLE_EQ(received.returns.get_column<double>("returns")[0],
                          0.01);
         if (!received.factor_returns.has_value()) {
           return titled_document("missing factor returns");
         }
         EXPECT_DOUBLE_EQ(
             received.factor_returns->get_column<double>("factor_returns")[1],
             0.04);
         return titled_document("custom renderer");
       }},
  });

  const auto document = snapshot.render("\tCUSTOM ", model);

  EXPECT_EQ(callback_count, 1U);
  EXPECT_EQ(document.domain, "custom-viz");
  EXPECT_EQ(document.title, "custom renderer");
  const auto callback_copy = snapshot.resolve_renderer("  custom\n");
  EXPECT_TRUE(static_cast<bool>(callback_copy));
}

TEST(RendererSnapshot, SnapshotsOwnCallbacksAndRemainIsolated) {
  fincore::renderer_map original_callbacks{
      {"custom",
       [](const fincore::render_model &) {
         return titled_document("first snapshot");
       }},
  };
  const fincore::renderer_snapshot first{original_callbacks};
  original_callbacks["custom"] = [](const fincore::render_model &) {
    return titled_document("mutated source map");
  };
  const fincore::renderer_snapshot second({
      {"CUSTOM",
       [](const fincore::render_model &) {
         return titled_document("second snapshot");
       }},
  });
  const auto model = make_model();

  EXPECT_EQ(first.render("custom", model).title, "first snapshot");
  EXPECT_EQ(second.render("custom", model).title, "second snapshot");
}

TEST(RendererSnapshot, RejectsInvalidRegistrationAndUnknownNames) {
  const fincore::renderer_callback callback =
      [](const fincore::render_model &) { return titled_document("valid"); };
  EXPECT_THROW(static_cast<void>(fincore::renderer_snapshot(
                   fincore::renderer_map{{" \t\n", callback}})),
               fincore::input_error);
  EXPECT_THROW(
      static_cast<void>(fincore::renderer_snapshot(fincore::renderer_map{
          {"Renderer", callback}, {" renderer ", callback}})),
      fincore::input_error);
  EXPECT_THROW(
      static_cast<void>(fincore::renderer_snapshot(
          fincore::renderer_map{{"empty", fincore::renderer_callback{}}})),
      fincore::input_error);

  const fincore::renderer_snapshot snapshot({{"known", callback}});
  EXPECT_THROW(static_cast<void>(snapshot.resolve_renderer("missing")),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(snapshot.resolve_renderer(" \t ")),
               fincore::input_error);
  EXPECT_THROW(static_cast<void>(snapshot.render("missing", make_model())),
               fincore::input_error);
}

TEST(RendererSnapshot, OwnsModelDataAndDoesNotRecomputeStoredHtmlResult) {
  auto model = make_model();
  std::optional<fincore::render_model> queued_model;
  std::size_t callback_count = 0U;
  const auto *caller_model = &model;
  const fincore::renderer_snapshot snapshot({
      {"capture",
       [&](const fincore::render_model &received) {
         ++callback_count;
         EXPECT_NE(&received, caller_model);
         queued_model.emplace(received);
         return titled_document("frozen output");
       }},
  });

  const auto document = snapshot.render("capture", model);
  ASSERT_TRUE(queued_model.has_value());
  ASSERT_TRUE(queued_model->factor_returns.has_value());
  EXPECT_DOUBLE_EQ(queued_model->returns.get_column<double>("returns")[0],
                   0.01);
  EXPECT_DOUBLE_EQ(
      queued_model->factor_returns->get_column<double>("factor_returns")[0],
      0.03);
  EXPECT_EQ(queued_model->period, fincore::frequency::weekly);

  model = fincore::render_model{make_frame({9.9}, "returns")};
  EXPECT_DOUBLE_EQ(queued_model->returns.get_column<double>("returns")[0],
                   0.01);
  const auto html = fincore::render_html(document);
  EXPECT_NE(html.find("frozen output"), std::string::npos);
  EXPECT_EQ(callback_count, 1U);
}

TEST(RendererSnapshot, PropagatesCallbackExceptions) {
  const fincore::renderer_snapshot snapshot({
      {"throws",
       [](const fincore::render_model &) -> fincore::report_document {
         throw std::runtime_error{"renderer failure"};
       }},
  });

  try {
    static_cast<void>(snapshot.render("throws", make_model()));
    FAIL() << "expected renderer exception";
  } catch (const std::runtime_error &error) {
    EXPECT_STREQ(error.what(), "renderer failure");
  }
}

} // namespace
