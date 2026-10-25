#pragma once

#include <fincore/factor/research_detailed.hpp>

#include <cstddef>
#include <map>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

inline constexpr const char* research_detailed_export_schema_id =
    "fincore.factor_research_detailed.v2";
inline constexpr int research_detailed_export_schema_version = 2;

/// Export the ten factor-local detailed research tables as UTF-8/LF CSV files
/// with a versioned JSON manifest. Rows retain the order in ``result``. The
/// single-factor result must have empty correlation and comparison tables;
/// their omission is identified in the manifest's engine metadata.
///
/// A non-empty destination is never overwritten. If an export fails, only
/// files created by this call are removed; a newly created destination is
/// removed only when it is empty. An existing empty destination is retained.
research_export_result export_factor_research_detailed(
    const research_detailed_result& result, const std::string& directory);

/// Owning metadata returned by the combined detailed evaluation and export.
/// Signal and label rows are streamed to the CSV files and are not retained in
/// this outcome; their row counts remain available here and in the manifest.
struct research_detailed_export_outcome {
    std::string status;
    std::map<std::string, std::string> manifest;
    std::vector<research_detailed_diagnostic> diagnostics;
    std::size_t signal_rows = 0U;
    std::size_t label_rows = 0U;
    research_export_result exported;
};

/// Evaluate one factor and export its detailed result without materializing
/// owning signal and label row vectors. Large tables are synchronously written
/// from the evaluator's live columns; no borrowed view escapes the call.
/// If evaluation or export fails, files created by the call are removed, and
/// a newly created destination is removed only when it is empty. Existing
/// empty destinations are retained and non-empty destinations are rejected.
/// Evaluation and writing may interleave: the first encountered failure is
/// propagated, rather than imposing the error order of two separate calls.
research_detailed_export_outcome
evaluate_and_export_factor_research_detailed(
    std::string_view factor_name, const factor_series& factors,
    const dataframe& prices, const research_spec& spec,
    const std::string& directory);

}  // namespace fincore
