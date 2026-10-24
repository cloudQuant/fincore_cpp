#include <fincore/viz/render_model.hpp>

#include <fincore/error.hpp>

#include <utility>

namespace fincore {
namespace {

bool is_ascii_whitespace(const char value) {
  return value == ' ' || value == '\t' || value == '\n' || value == '\r' ||
         value == '\f' || value == '\v';
}

std::string normalize_renderer_name(const std::string_view name) {
  std::size_t first = 0U;
  while (first < name.size() && is_ascii_whitespace(name[first])) {
    ++first;
  }

  std::size_t last = name.size();
  while (last > first && is_ascii_whitespace(name[last - 1U])) {
    --last;
  }

  std::string normalized;
  normalized.reserve(last - first);
  for (std::size_t index = first; index < last; ++index) {
    char value = name[index];
    if (value >= 'A' && value <= 'Z') {
      value = static_cast<char>(value - 'A' + 'a');
    }
    normalized.push_back(value);
  }
  return normalized;
}

} // namespace

render_model::render_model(dataframe returns_value,
                           std::optional<dataframe> factor_returns_value,
                           const frequency period_value)
    : returns{std::move(returns_value)},
      factor_returns{std::move(factor_returns_value)}, period{period_value} {}

renderer_snapshot::renderer_snapshot(renderer_map renderers) {
  for (auto &[name, callback] : renderers) {
    auto normalized_name = normalize_renderer_name(name);
    if (normalized_name.empty()) {
      throw input_error{"renderer names must not be empty"};
    }
    if (!callback) {
      throw input_error{"renderer callbacks must not be empty"};
    }
    if (!renderers_.emplace(std::move(normalized_name), std::move(callback))
             .second) {
      throw input_error{"renderer names must be unique after normalization"};
    }
  }
}

renderer_callback
renderer_snapshot::resolve_renderer(const std::string_view name) const {
  const auto normalized_name = normalize_renderer_name(name);
  if (normalized_name.empty()) {
    throw input_error{"renderer name must not be empty"};
  }
  const auto found = renderers_.find(normalized_name);
  if (found == renderers_.end()) {
    throw input_error{"unknown viz renderer '" + normalized_name + "'"};
  }
  return found->second;
}

report_document renderer_snapshot::render(const std::string_view name,
                                          const render_model &model) const {
  const auto callback = resolve_renderer(name);
  const render_model frozen_model = model;
  return callback(frozen_model);
}

} // namespace fincore
