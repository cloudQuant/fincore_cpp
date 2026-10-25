#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Schema version accepted by report_document validation and serialization.
inline constexpr int report_document_schema_version = 2;

/// One typed cell in a report_table.
struct report_cell {
    /// Cell value kind.
    ///
    /// `missing` carries no value and serializes as null, `number` uses
    /// `number`, `text` uses `text`, and `boolean` uses `boolean`.
    enum class kind { missing, number, text, boolean };
    kind type = kind::missing;  ///< Value kind that selects the active field.
    double number = 0.0;        ///< Numeric value used when `type` is number.
    bool boolean = false;       ///< Boolean value used when `type` is boolean.
    std::string text;           ///< Text value used when `type` is text.

    /// Constructs a missing cell.
    /// @return Cell whose type is kind::missing.
    static report_cell missing() { return {}; }
    /// Constructs a numeric cell.
    /// @param value Numeric value to store.
    /// @return Cell whose type is kind::number.
    static report_cell number_cell(double value) {
        report_cell cell;
        cell.type = kind::number;
        cell.number = value;
        return cell;
    }
    /// Constructs a text cell.
    /// @param value Text to move into the cell.
    /// @return Cell whose type is kind::text.
    static report_cell text_cell(std::string value) {
        report_cell cell;
        cell.type = kind::text;
        cell.text = std::move(value);
        return cell;
    }
    /// Constructs a boolean cell.
    /// @param value Boolean value to store.
    /// @return Cell whose type is kind::boolean.
    static report_cell boolean_cell(bool value) {
        report_cell cell;
        cell.type = kind::boolean;
        cell.boolean = value;
        return cell;
    }
};

/// Named table of typed cells.
struct report_table {
    /// Stable identifier used by section metadata and renderers.
    std::string key;
    std::string title;                 ///< Display title.
    std::vector<std::string> columns;  ///< Column headers.
    /// Rows; each row must have `columns.size()` cells when serialized.
    std::vector<std::vector<report_cell>> rows;
};

/// Named numeric series for chart rendering.
struct report_series {
    std::string key;      ///< Stable identifier used by legends and artifacts.
    std::string title;    ///< Display title.
    std::string x_label;  ///< X-axis label.
    std::string y_label;  ///< Y-axis label.
    std::string unit;     ///< Value unit, such as "ratio" or "growth_multiple".
    /// X values; must have the same length as `y`.
    std::vector<double> x;
    std::vector<double> y;  ///< Y values.
};

/// Named scalar metric with one optional typed value.
struct report_metric {
    std::string key;    ///< Stable identifier used by section metadata.
    std::string label;  ///< Display label.
    std::string unit;   ///< Value unit, such as "ratio" or "count".
    /// Numeric value; takes precedence over `boolean` and `text`.
    std::optional<double> number;
    /// Boolean value used when `number` is unset.
    std::optional<bool> boolean;
    /// Text value used when `number` and `boolean` are unset.
    std::optional<std::string> text;
};

/// One titled group of metrics, tables, series and notes.
struct report_section {
    std::string key;    ///< Document-unique section key.
    std::string title;  ///< Display title.
    std::vector<report_metric> metrics;  ///< Scalar metrics.
    std::vector<report_table> tables;    ///< Tables.
    std::vector<report_series> series;   ///< Chart series.
    std::vector<std::string> notes;      ///< Free-form notes.
    /// Unit overrides keyed by a metric, table or series key in this section.
    std::map<std::string, std::string> units;
    /// Chart legends keyed by a series key in this section.
    std::map<std::string, std::string> legends;

    /// Serializes this section as deterministic JSON.
    ///
    /// Every `units` and `legends` key must match a metric, table or series
    /// key in the section, and their values must be non-empty.
    ///
    /// @return JSON text terminated by a newline.
    /// @throws input_error If section metadata references an unknown key or
    ///     has an empty value, or if a number, table row or series is
    ///     malformed.
    std::string semantic_payload() const;
};

/// Versioned report document shared by report builders and renderers.
struct report_document {
    /// Report domain, such as "portfolio", "risk" or "factor".
    std::string domain;
    std::string title;  ///< Document title.
    /// Ordered sections; keys must be unique.
    std::vector<report_section> sections;
    std::map<std::string, std::string> metadata;  ///< Free-form metadata.
    /// Schema version; must equal report_document_schema_version to serialize.
    int schema_version = report_document_schema_version;
    /// Embedded asset name to content mapping; `to_json` writes the content
    /// as string values while `semantic_payload` writes its SHA-256 digest.
    std::map<std::string, std::string> offline_assets;

    /// Finds a section by key.
    /// @param key Section key to search for.
    /// @return Const reference to the matching section.
    /// @throws input_error If the document is invalid or no section has `key`.
    const report_section& section(std::string_view key) const;
    /// Finds a section by key for in-place modification.
    /// @param key Section key to search for.
    /// @return Mutable reference to the matching section.
    /// @throws input_error If the document is invalid or no section has `key`.
    report_section& section(std::string_view key);
    /// Serializes the validated document as deterministic JSON.
    ///
    /// Offline asset content is embedded as JSON string values and map keys
    /// are emitted in sorted order.
    ///
    /// @return JSON text terminated by a newline.
    /// @throws input_error If the schema version is unsupported, section keys
    ///     are duplicated, section metadata is invalid, or a value is
    ///     non-finite or malformed.
    std::string to_json() const;
    /// Serializes the validated document with offline asset content hashed.
    ///
    /// Each asset value is replaced by its SHA-256 hexadecimal digest, so the
    /// payload reflects asset identity without embedding the bytes.
    ///
    /// @return JSON text terminated by a newline.
    /// @throws input_error For the same validation failures as to_json().
    std::string semantic_payload() const;
    /// Computes the provenance digest of the semantic payload.
    ///
    /// The digest is SHA-256 over a versioned frame and `semantic_payload()`;
    /// report renderers expose it as the `report_digest` bundle metadata.
    ///
    /// @return Lowercase hexadecimal SHA-256 digest.
    /// @throws input_error If `semantic_payload()` fails validation.
    std::string semantic_digest() const;
};

}  // namespace fincore
