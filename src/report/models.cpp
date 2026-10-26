#include <fincore/report/models.hpp>

#include <fincore/detail/digest.hpp>
#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <cstdint>
#include <set>
#include <span>
#include <string>
#include <string_view>

namespace fincore {
namespace {

void write_cell(detail::json_writer& writer, const report_cell& cell) {
    switch (cell.type) {
        case report_cell::kind::missing:
            writer.value_null();
            break;
        case report_cell::kind::number:
            writer.value_number(cell.number, "report cell");
            break;
        case report_cell::kind::text:
            writer.value_string(cell.text);
            break;
        case report_cell::kind::boolean:
            writer.value_boolean(cell.boolean);
            break;
    }
}

// The kind is decided by precedence rather than validated: number wins over
// boolean wins over text, and a metric carrying two of them is written as the
// first with the other silently dropped, which is why the producers set exactly
// one. Every branch emits the same five keys so a consumer can select on "kind"
// without probing for optional fields.
void write_metric(detail::json_writer& writer,
                  const report_metric& metric) {
    writer.begin_object();
    writer.key("key");
    writer.value_string(metric.key);
    if (metric.number.has_value()) {
        writer.key("kind");
        writer.value_string("number");
        writer.key("label");
        writer.value_string(metric.label);
        writer.key("unit");
        writer.value_string(metric.unit);
        writer.key("value");
        writer.value_number(*metric.number, "report metric");
    } else if (metric.boolean.has_value()) {
        writer.key("kind");
        writer.value_string("boolean");
        writer.key("label");
        writer.value_string(metric.label);
        writer.key("unit");
        writer.value_string(metric.unit);
        writer.key("value");
        writer.value_boolean(*metric.boolean);
    } else if (metric.text.has_value()) {
        writer.key("kind");
        writer.value_string("text");
        writer.key("label");
        writer.value_string(metric.label);
        writer.key("unit");
        writer.value_string(metric.unit);
        writer.key("value");
        writer.value_string(*metric.text);
    } else {
        writer.key("kind");
        writer.value_string("missing");
        writer.key("label");
        writer.value_string(metric.label);
        writer.key("unit");
        writer.value_string(metric.unit);
        writer.key("value");
        writer.value_null();
    }
    writer.end_object();
}

void write_table(detail::json_writer& writer, const report_table& table) {
    writer.begin_object();
    writer.key("columns");
    writer.begin_array();
    for (const auto& column : table.columns) {
        writer.value_string(column);
    }
    writer.end_array();
    writer.key("key");
    writer.value_string(table.key);
    writer.key("rows");
    writer.begin_array();
    for (const auto& row : table.rows) {
        if (row.size() != table.columns.size()) {
            throw input_error{
                "report table rows must match the column count"};
        }
        writer.begin_array();
        for (const auto& cell : row) {
            write_cell(writer, cell);
        }
        writer.end_array();
    }
    writer.end_array();
    writer.key("title");
    writer.value_string(table.title);
    writer.end_object();
}

void write_series(detail::json_writer& writer,
                  const report_series& series) {
    if (series.x.size() != series.y.size()) {
        throw input_error{
            "report series x and y must have equal lengths"};
    }
    writer.begin_object();
    writer.key("key");
    writer.value_string(series.key);
    writer.key("title");
    writer.value_string(series.title);
    writer.key("unit");
    writer.value_string(series.unit);
    writer.key("x");
    writer.begin_array();
    for (const double value : series.x) {
        writer.value_number(value, "report series x");
    }
    writer.end_array();
    writer.key("x_label");
    writer.value_string(series.x_label);
    writer.key("y");
    writer.begin_array();
    for (const double value : series.y) {
        writer.value_number(value, "report series y");
    }
    writer.end_array();
    writer.key("y_label");
    writer.value_string(series.y_label);
    writer.end_object();
}

void write_string_map(detail::json_writer& writer,
                      const std::map<std::string, std::string>& values) {
    writer.begin_object();
    for (const auto& [key, value] : values) {
        writer.key(key);
        writer.value_string(value);
    }
    writer.end_object();
}

std::string sha256_text(const std::string_view value) {
    const auto* bytes = reinterpret_cast<const std::uint8_t*>(value.data());
    return detail::sha256_hex(
        std::span<const std::uint8_t>{bytes, value.size()});
}

void validate_section_metadata(const report_section& section) {
    const auto references_known_value = [&section](const std::string& key) {
        for (const auto& metric : section.metrics) {
            if (metric.key == key) {
                return true;
            }
        }
        for (const auto& table : section.tables) {
            if (table.key == key) {
                return true;
            }
        }
        for (const auto& series : section.series) {
            if (series.key == key) {
                return true;
            }
        }
        return false;
    };
    const auto validate_map = [&references_known_value](
                                  const std::map<std::string, std::string>& values,
                                  const std::string_view name) {
        for (const auto& [key, value] : values) {
            if (!references_known_value(key)) {
                throw input_error{"report section " + std::string(name) +
                                  " references an unknown value: " + key};
            }
            if (value.empty()) {
                throw input_error{"report section " + std::string(name) +
                                  " values must be non-empty"};
            }
        }
    };
    validate_map(section.units, "units");
    validate_map(section.legends, "legends");
}

void validate_document(const report_document& document) {
    if (document.schema_version != report_document_schema_version) {
        throw input_error{"unsupported report document schema version: " +
                          std::to_string(document.schema_version)};
    }
    std::set<std::string> section_keys;
    for (const auto& section : document.sections) {
        if (!section_keys.insert(section.key).second) {
            throw input_error{"report section keys must be unique: " +
                              section.key};
        }
        validate_section_metadata(section);
    }
}

void write_section(detail::json_writer& writer,
                   const report_section& section) {
    writer.begin_object();
    writer.key("key");
    writer.value_string(section.key);
    writer.key("legends");
    write_string_map(writer, section.legends);
    writer.key("metrics");
    writer.begin_array();
    for (const auto& metric : section.metrics) {
        write_metric(writer, metric);
    }
    writer.end_array();
    writer.key("notes");
    writer.begin_array();
    for (const auto& note : section.notes) {
        writer.value_string(note);
    }
    writer.end_array();
    writer.key("series");
    writer.begin_array();
    for (const auto& series : section.series) {
        write_series(writer, series);
    }
    writer.end_array();
    writer.key("tables");
    writer.begin_array();
    for (const auto& table : section.tables) {
        write_table(writer, table);
    }
    writer.end_array();
    writer.key("title");
    writer.value_string(section.title);
    writer.key("units");
    write_string_map(writer, section.units);
    writer.end_object();
}

void write_document(detail::json_writer& writer,
                    const report_document& document,
                    const bool hash_asset_contents) {
    writer.begin_object();
    writer.key("domain");
    writer.value_string(document.domain);
    writer.key("metadata");
    write_string_map(writer, document.metadata);
    writer.key("offline_assets");
    writer.begin_object();
    for (const auto& [name, content] : document.offline_assets) {
        writer.key(name);
        writer.value_string(hash_asset_contents ? sha256_text(content)
                                                : content);
    }
    writer.end_object();
    writer.key("schema_version");
    writer.value_integer(document.schema_version);
    writer.key("sections");
    writer.begin_array();
    for (const auto& section : document.sections) {
        write_section(writer, section);
    }
    writer.end_array();
    writer.key("title");
    writer.value_string(document.title);
    writer.end_object();
}

}  // namespace

std::string report_section::semantic_payload() const {
    validate_section_metadata(*this);
    std::string output;
    detail::json_writer writer(output);
    write_section(writer, *this);
    output.push_back('\n');
    return output;
}

const report_section& report_document::section(
    const std::string_view key) const {
    validate_document(*this);
    for (const auto& value : sections) {
        if (value.key == key) {
            return value;
        }
    }
    throw input_error{"report section not found: " + std::string(key)};
}

report_section& report_document::section(const std::string_view key) {
    validate_document(*this);
    for (auto& value : sections) {
        if (value.key == key) {
            return value;
        }
    }
    throw input_error{"report section not found: " + std::string(key)};
}

std::string report_document::to_json() const {
    validate_document(*this);
    std::string output;
    detail::json_writer writer(output);
    write_document(writer, *this, false);
    output.push_back('\n');
    return output;
}

std::string report_document::semantic_payload() const {
    validate_document(*this);
    std::string output;
    detail::json_writer writer(output);
    write_document(writer, *this, true);
    output.push_back('\n');
    return output;
}

std::string report_document::semantic_digest() const {
    std::string framed{"fincore.report_document.semantic_digest:v1"};
    framed.push_back('\0');
    framed += semantic_payload();
    return sha256_text(framed);
}

}  // namespace fincore
