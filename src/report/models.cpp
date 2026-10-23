#include <fincore/report/models.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

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

void write_section(detail::json_writer& writer,
                   const report_section& section) {
    writer.begin_object();
    writer.key("key");
    writer.value_string(section.key);
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
    writer.end_object();
}

}  // namespace

std::string report_document::to_json() const {
    std::string output;
    detail::json_writer writer(output);
    writer.begin_object();
    writer.key("domain");
    writer.value_string(domain);
    writer.key("metadata");
    writer.begin_object();
    for (const auto& entry : metadata) {
        writer.key(entry.first);
        writer.value_string(entry.second);
    }
    writer.end_object();
    writer.key("schema_version");
    writer.value_integer(schema_version);
    writer.key("sections");
    writer.begin_array();
    for (const auto& section : sections) {
        write_section(writer, section);
    }
    writer.end_array();
    writer.key("title");
    writer.value_string(title);
    writer.end_object();
    output.push_back('\n');
    return output;
}

}  // namespace fincore
