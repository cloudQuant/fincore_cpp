#pragma once

#include <fincore/report/models.hpp>

#include <string>

namespace fincore {

/// Worksheet naming options for render_xlsx.
struct xlsx_render_options {
    /// Prefix prepended to each section key to form worksheet names.
    std::string sheet_prefix;
};

/// Renders a report document as an in-memory XLSX workbook.
///
/// One worksheet is emitted per section. Worksheet names combine
/// `options.sheet_prefix` with the section key, replace invalid characters
/// with '_', and are capped at 31 characters; duplicates receive a "_2",
/// "_3", ... suffix. An empty document produces a single worksheet named
/// "report" or the prefix when one is set. No file is written.
///
/// @param document Prepared report document.
/// @param options Worksheet naming options.
/// @return ZIP-compressed XLSX bytes, deterministic for the same document and
///     options.
/// @throws input_error If a numeric value is non-finite, a table row does not
///     match its column count, or a series has mismatched x and y lengths.
std::string render_xlsx(
    const report_document& document,
    const xlsx_render_options& options = {});

}  // namespace fincore
