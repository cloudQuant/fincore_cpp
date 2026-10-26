#pragma once

#include <fincore/dataframe.hpp>
#include <fincore/metrics/basic.hpp>
#include <fincore/report/models.hpp>

#include <functional>
#include <map>
#include <optional>
#include <string>
#include <string_view>

namespace fincore {

/**
 * Structured, owning inputs passed to a native visualization renderer.
 *
 * This maps the pinned Python `RenderModel` fields in
 * `fincore/viz/base.py` (4b8f31b1b59707480d5db9603704d83bbf7e6946):
 * `returns`, optional `factor_returns`, and `period`. C++ uses the native
 * `frequency` enum instead of the Python string convention.
 */
struct render_model {
  dataframe returns;                        ///< Portfolio return frame.
  std::optional<dataframe> factor_returns;  ///< Optional factor return frame.
  frequency period = frequency::daily;      ///< Return frequency.

  /// Creates an owning render model from the supplied frames and period.
  explicit render_model(dataframe returns,
                        std::optional<dataframe> factor_returns = std::nullopt,
                        frequency period = frequency::daily);
};

/// Callback that converts a render model into a report document.
using renderer_callback =
    std::function<report_document(const render_model &model)>;
/// Renderer callbacks keyed by caller-provided name.
using renderer_map = std::map<std::string, renderer_callback>;

/**
 * An owning snapshot of explicitly registered native renderer callbacks.
 *
 * Names are trimmed using ASCII whitespace and lowercased using ASCII rules.
 * An empty/duplicate normalized name or empty callback is rejected. The
 * snapshot does not scan plugins, import optional dependencies, or mutate a
 * process-wide registry. `render` passes a copied model to the callback for
 * the duration of the call; callbacks that queue work must copy that model
 * before returning. Mutable state captured by callbacks remains the caller's
 * responsibility. Built-in HTML rendering remains `render_html` over a
 * `report_document`.
 */
class renderer_snapshot {
public:
  /// Creates an immutable snapshot of the supplied renderer callbacks.
  ///
  /// Throws `input_error` for an empty normalized name, an empty callback, or
  /// names that collide after trimming and ASCII lowercasing.
  explicit renderer_snapshot(renderer_map renderers);

  /// Returns the callback registered under `name` after name normalization.
  ///
  /// Throws `input_error` for an empty or unknown normalized name.
  renderer_callback resolve_renderer(std::string_view name) const;
  /// Renders `name` with a copied model and returns its report document.
  ///
  /// Throws `input_error` for an unknown renderer; exceptions raised by the
  /// callback propagate unchanged.
  report_document render(std::string_view name,
                         const render_model &model) const;

private:
  renderer_map renderers_;
};

} // namespace fincore
