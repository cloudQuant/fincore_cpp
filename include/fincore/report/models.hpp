#pragma once

#include <cstddef>
#include <map>
#include <optional>
#include <utility>
#include <string>
#include <vector>

namespace fincore {

inline constexpr int report_document_schema_version = 1;

struct report_cell {
    enum class kind { missing, number, text, boolean };
    kind type = kind::missing;
    double number = 0.0;
    bool boolean = false;
    std::string text;

    static report_cell missing() { return {}; }
    static report_cell number_cell(double value) {
        report_cell cell;
        cell.type = kind::number;
        cell.number = value;
        return cell;
    }
    static report_cell text_cell(std::string value) {
        report_cell cell;
        cell.type = kind::text;
        cell.text = std::move(value);
        return cell;
    }
    static report_cell boolean_cell(bool value) {
        report_cell cell;
        cell.type = kind::boolean;
        cell.boolean = value;
        return cell;
    }
};

struct report_table {
    std::string key;
    std::string title;
    std::vector<std::string> columns;
    std::vector<std::vector<report_cell>> rows;
};

struct report_series {
    std::string key;
    std::string title;
    std::string x_label;
    std::string y_label;
    std::string unit;
    std::vector<double> x;
    std::vector<double> y;
};

struct report_metric {
    std::string key;
    std::string label;
    std::string unit;
    std::optional<double> number;
    std::optional<bool> boolean;
    std::optional<std::string> text;
};

struct report_section {
    std::string key;
    std::string title;
    std::vector<report_metric> metrics;
    std::vector<report_table> tables;
    std::vector<report_series> series;
    std::vector<std::string> notes;
};

struct report_document {
    std::string domain;
    std::string title;
    std::vector<report_section> sections;
    std::map<std::string, std::string> metadata;
    int schema_version = report_document_schema_version;

    std::string to_json() const;
};

}  // namespace fincore
