#pragma once

#include <fincore/factor/research_detailed.hpp>

#include <string>

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

}  // namespace fincore
