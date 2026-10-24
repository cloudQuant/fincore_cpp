#include <fincore/report/xlsx.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/error.hpp>

#include <array>
#include <cstdint>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

namespace fincore {
namespace {

struct xlsx_cell {
    enum class kind { missing, number, text, boolean };
    kind type = kind::missing;
    double number = 0.0;
    bool boolean = false;
    std::string text;
};

struct sheet_definition {
    std::string name;
    std::vector<std::vector<xlsx_cell>> rows;
};

xlsx_cell text_cell(std::string value) {
    xlsx_cell cell;
    cell.type = xlsx_cell::kind::text;
    cell.text = std::move(value);
    return cell;
}

xlsx_cell number_cell(const double value) {
    xlsx_cell cell;
    cell.type = xlsx_cell::kind::number;
    cell.number = value;
    return cell;
}

xlsx_cell boolean_cell(const bool value) {
    xlsx_cell cell;
    cell.type = xlsx_cell::kind::boolean;
    cell.boolean = value;
    return cell;
}

std::string escape_xml(const std::string_view text) {
    std::string output;
    output.reserve(text.size());
    for (std::size_t index = 0; index < text.size();) {
        const auto first = static_cast<unsigned char>(text[index]);
        std::size_t length = 0U;
        std::uint32_t codepoint = 0U;
        if (first < 0x80U) {
            length = 1U;
            codepoint = first;
        } else if (first >= 0xC2U && first <= 0xDFU) {
            length = 2U;
            codepoint = first & 0x1FU;
        } else if (first >= 0xE0U && first <= 0xEFU) {
            length = 3U;
            codepoint = first & 0x0FU;
        } else if (first >= 0xF0U && first <= 0xF4U) {
            length = 4U;
            codepoint = first & 0x07U;
        }

        bool valid_sequence = length != 0U && length <= text.size() - index;
        for (std::size_t offset = 1U; valid_sequence && offset < length;
             ++offset) {
            const auto continuation =
                static_cast<unsigned char>(text[index + offset]);
            valid_sequence = continuation >= 0x80U && continuation <= 0xBFU;
            if (valid_sequence) {
                codepoint = (codepoint << 6U) | (continuation & 0x3FU);
            }
        }
        if ((length == 3U && codepoint < 0x800U) ||
            (length == 4U && codepoint < 0x10000U) ||
            (codepoint >= 0xD800U && codepoint <= 0xDFFFU) ||
            codepoint > 0x10FFFFU) {
            valid_sequence = false;
        }
        if (!valid_sequence) {
            output.push_back(' ');
            ++index;
            continue;
        }

        // Keep emitted text valid UTF-8 and within XML 1.0's character set.
        const bool valid_xml_character =
            codepoint == 0x9U || codepoint == 0xAU || codepoint == 0xDU ||
            (codepoint >= 0x20U && codepoint <= 0xD7FFU) ||
            (codepoint >= 0xE000U && codepoint <= 0xFFFDU) ||
            (codepoint >= 0x10000U && codepoint <= 0x10FFFFU);
        if (!valid_xml_character) {
            output.push_back(' ');
            index += length;
            continue;
        }

        switch (codepoint) {
            case '&':
                output += "&amp;";
                break;
            case '<':
                output += "&lt;";
                break;
            case '>':
                output += "&gt;";
                break;
            case '"':
                output += "&quot;";
                break;
            case '\'':
                output += "&apos;";
                break;
            default:
                if (codepoint < 0x80U) {
                    output.push_back(static_cast<char>(codepoint));
                } else {
                    output.append(text.substr(index, length));
                }
                break;
        }
        index += length;
    }
    return output;
}

std::string column_name(std::size_t index) {
    std::string name;
    do {
        name.insert(name.begin(), static_cast<char>('A' + index % 26U));
        if (index < 26U) {
            break;
        }
        index = index / 26U - 1U;
    } while (true);
    return name;
}

std::string sheet_xml(const sheet_definition& sheet) {
    std::string xml =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<worksheet xmlns=\"http://schemas.openxmlformats.org/"
        "spreadsheetml/2006/main\"><sheetData>";
    for (std::size_t row = 0; row < sheet.rows.size(); ++row) {
        xml += "<row r=\"" + std::to_string(row + 1U) + "\">";
        for (std::size_t column = 0; column < sheet.rows[row].size();
             ++column) {
            const auto& cell = sheet.rows[row][column];
            if (cell.type == xlsx_cell::kind::missing) {
                continue;
            }
            const std::string reference =
                column_name(column) + std::to_string(row + 1U);
            switch (cell.type) {
                case xlsx_cell::kind::number:
                    xml += "<c r=\"" + reference + "\"><v>" +
                           detail::python_float(cell.number, "xlsx cell") +
                           "</v></c>";
                    break;
                case xlsx_cell::kind::text:
                    xml += "<c r=\"" + reference +
                           "\" t=\"inlineStr\"><is><t xml:space=\"preserve\">" +
                           escape_xml(cell.text) + "</t></is></c>";
                    break;
                case xlsx_cell::kind::boolean:
                    xml += "<c r=\"" + reference + "\" t=\"b\"><v>" +
                           (cell.boolean ? "1" : "0") + "</v></c>";
                    break;
                case xlsx_cell::kind::missing:
                    break;
            }
        }
        xml += "</row>";
    }
    xml += "</sheetData></worksheet>";
    return xml;
}

std::uint32_t crc32_of(const std::string& data) {
    static const std::array<std::uint32_t, 256> table = [] {
        std::array<std::uint32_t, 256> values{};
        for (std::uint32_t index = 0; index < 256U; ++index) {
            std::uint32_t value = index;
            for (int bit = 0; bit < 8; ++bit) {
                value = (value & 1U) != 0U ? (value >> 1U) ^ 0xEDB88320U
                                           : value >> 1U;
            }
            values[index] = value;
        }
        return values;
    }();
    std::uint32_t crc = 0xFFFFFFFFU;
    for (const unsigned char character : data) {
        crc = table[(crc ^ character) & 0xFFU] ^ (crc >> 8U);
    }
    return crc ^ 0xFFFFFFFFU;
}

void put_u16(std::string& output, const std::uint16_t value) {
    output.push_back(static_cast<char>(value & 0xFFU));
    output.push_back(static_cast<char>((value >> 8U) & 0xFFU));
}

void put_u32(std::string& output, const std::uint32_t value) {
    output.push_back(static_cast<char>(value & 0xFFU));
    output.push_back(static_cast<char>((value >> 8U) & 0xFFU));
    output.push_back(static_cast<char>((value >> 16U) & 0xFFU));
    output.push_back(static_cast<char>((value >> 24U) & 0xFFU));
}

std::string build_zip(
    const std::vector<std::pair<std::string, std::string>>& entries) {
    std::string output;
    std::string central;
    for (const auto& entry : entries) {
        const std::uint32_t crc = crc32_of(entry.second);
        const std::uint32_t size =
            static_cast<std::uint32_t>(entry.second.size());
        const std::uint32_t offset =
            static_cast<std::uint32_t>(output.size());
        put_u32(output, 0x04034b50U);
        put_u16(output, 20U);
        put_u16(output, 0x0800U);
        put_u16(output, 0U);
        put_u16(output, 0U);
        put_u16(output, 0x0021U);
        put_u32(output, crc);
        put_u32(output, size);
        put_u32(output, size);
        put_u16(output, static_cast<std::uint16_t>(entry.first.size()));
        put_u16(output, 0U);
        output += entry.first;
        output += entry.second;

        put_u32(central, 0x02014b50U);
        put_u16(central, 20U);
        put_u16(central, 20U);
        put_u16(central, 0x0800U);
        put_u16(central, 0U);
        put_u16(central, 0U);
        put_u16(central, 0x0021U);
        put_u32(central, crc);
        put_u32(central, size);
        put_u32(central, size);
        put_u16(central, static_cast<std::uint16_t>(entry.first.size()));
        put_u16(central, 0U);
        put_u16(central, 0U);
        put_u16(central, 0U);
        put_u16(central, 0U);
        put_u32(central, 0U);
        put_u32(central, offset);
        central += entry.first;
    }
    const std::uint32_t central_offset =
        static_cast<std::uint32_t>(output.size());
    output += central;
    put_u32(output, 0x06054b50U);
    put_u16(output, 0U);
    put_u16(output, 0U);
    put_u16(output, static_cast<std::uint16_t>(entries.size()));
    put_u16(output, static_cast<std::uint16_t>(entries.size()));
    put_u32(output, static_cast<std::uint32_t>(central.size()));
    put_u32(output, central_offset);
    put_u16(output, 0U);
    return output;
}

std::string sanitize_sheet_name(std::string name) {
    for (char& character : name) {
        if (character == '[' || character == ']' || character == ':' ||
            character == '*' || character == '?' || character == '/' ||
            character == '\\') {
            character = '_';
        }
    }
    if (name.size() > 31U) {
        name.resize(31U);
    }
    return name.empty() ? "sheet" : name;
}

std::vector<sheet_definition> build_sheets(
    const report_document& document, const xlsx_render_options& options) {
    std::vector<sheet_definition> sheets;
    std::set<std::string> used_names;
    for (const auto& section : document.sections) {
        sheet_definition sheet;
        sheet.name = sanitize_sheet_name(options.sheet_prefix + section.key);
        std::string candidate = sheet.name;
        int suffix = 2;
        while (!used_names.insert(candidate).second) {
            candidate = sanitize_sheet_name(
                sheet.name + "_" + std::to_string(suffix++));
        }
        sheet.name = candidate;

        sheet.rows.push_back({text_cell("metric"), text_cell("label"),
                              text_cell("value"), text_cell("unit")});
        for (const auto& metric : section.metrics) {
            std::vector<xlsx_cell> row{
                text_cell(metric.key), text_cell(metric.label)};
            if (metric.number.has_value()) {
                row.push_back(number_cell(*metric.number));
            } else if (metric.boolean.has_value()) {
                row.push_back(boolean_cell(*metric.boolean));
            } else if (metric.text.has_value()) {
                row.push_back(text_cell(*metric.text));
            } else {
                row.push_back({});
            }
            row.push_back(text_cell(metric.unit));
            sheet.rows.push_back(std::move(row));
        }
        sheet.rows.push_back({});
        for (const auto& table : section.tables) {
            sheet.rows.push_back(
                {text_cell(table.key), text_cell(table.title)});
            std::vector<xlsx_cell> header;
            header.reserve(table.columns.size());
            for (const auto& column : table.columns) {
                header.push_back(text_cell(column));
            }
            sheet.rows.push_back(std::move(header));
            for (const auto& table_row : table.rows) {
                if (table_row.size() != table.columns.size()) {
                    throw input_error{
                        "report table rows must match the column count"};
                }
                std::vector<xlsx_cell> row;
                row.reserve(table_row.size());
                for (const auto& cell : table_row) {
                    switch (cell.type) {
                        case report_cell::kind::missing:
                            row.push_back({});
                            break;
                        case report_cell::kind::number:
                            row.push_back(number_cell(cell.number));
                            break;
                        case report_cell::kind::text:
                            row.push_back(text_cell(cell.text));
                            break;
                        case report_cell::kind::boolean:
                            row.push_back(boolean_cell(cell.boolean));
                            break;
                    }
                }
                sheet.rows.push_back(std::move(row));
            }
            sheet.rows.push_back({});
        }
        for (const auto& series : section.series) {
            if (series.x.size() != series.y.size()) {
                throw input_error{
                    "report series x and y must have equal lengths"};
            }
            sheet.rows.push_back(
                {text_cell(series.key), text_cell(series.title)});
            sheet.rows.push_back(
                {text_cell(series.x_label), text_cell(series.y_label)});
            for (std::size_t index = 0; index < series.x.size(); ++index) {
                sheet.rows.push_back({number_cell(series.x[index]),
                                      number_cell(series.y[index])});
            }
            sheet.rows.push_back({});
        }
        sheets.push_back(std::move(sheet));
    }
    if (sheets.empty()) {
        sheets.push_back({sanitize_sheet_name(
                              options.sheet_prefix.empty()
                                  ? "report"
                                  : options.sheet_prefix),
                          {}});
    }
    return sheets;
}

}  // namespace

std::string render_xlsx(const report_document& document,
                        const xlsx_render_options& options) {
    const auto sheets = build_sheets(document, options);
    std::vector<std::pair<std::string, std::string>> entries;

    std::string content_types =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Types xmlns=\"http://schemas.openxmlformats.org/package/2006/"
        "content-types\"><Default Extension=\"rels\" ContentType=\"application/"
        "vnd.openxmlformats-package.relationships+xml\"/><Default "
        "Extension=\"xml\" ContentType=\"application/xml\"/><Override "
        "PartName=\"/xl/workbook.xml\" ContentType=\"application/"
        "vnd.openxmlformats-officedocument.spreadsheetml.sheet.main+xml\"/>"
        "<Override PartName=\"/xl/styles.xml\" ContentType=\"application/"
        "vnd.openxmlformats-officedocument.spreadsheetml.styles+xml\"/>";
    for (std::size_t index = 0; index < sheets.size(); ++index) {
        content_types +=
            "<Override PartName=\"/xl/worksheets/sheet" +
            std::to_string(index + 1U) +
            ".xml\" ContentType=\"application/vnd.openxmlformats-"
            "officedocument.spreadsheetml.worksheet+xml\"/>";
    }
    content_types += "</Types>";
    entries.emplace_back("[Content_Types].xml", std::move(content_types));

    entries.emplace_back(
        "_rels/.rels",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/"
        "2006/relationships\"><Relationship Id=\"rId1\" "
        "Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
        "relationships/officeDocument\" Target=\"xl/workbook.xml\"/>"
        "</Relationships>");

    std::string workbook =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<workbook xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/"
        "2006/main\" xmlns:r=\"http://schemas.openxmlformats.org/"
        "officeDocument/2006/relationships\"><sheets>";
    for (std::size_t index = 0; index < sheets.size(); ++index) {
        workbook += "<sheet name=\"" + escape_xml(sheets[index].name) +
                    "\" sheetId=\"" + std::to_string(index + 1U) +
                    "\" r:id=\"rId" + std::to_string(index + 1U) + "\"/>";
    }
    workbook += "</sheets></workbook>";
    entries.emplace_back("xl/workbook.xml", std::move(workbook));

    std::string workbook_rels =
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<Relationships xmlns=\"http://schemas.openxmlformats.org/package/"
        "2006/relationships\">";
    for (std::size_t index = 0; index < sheets.size(); ++index) {
        workbook_rels +=
            "<Relationship Id=\"rId" + std::to_string(index + 1U) +
            "\" Type=\"http://schemas.openxmlformats.org/officeDocument/"
            "2006/relationships/worksheet\" Target=\"worksheets/sheet" +
            std::to_string(index + 1U) + ".xml\"/>";
    }
    workbook_rels +=
        "<Relationship Id=\"rId" + std::to_string(sheets.size() + 1U) +
        "\" Type=\"http://schemas.openxmlformats.org/officeDocument/2006/"
        "relationships/styles\" Target=\"styles.xml\"/></Relationships>";
    entries.emplace_back("xl/_rels/workbook.xml.rels",
                         std::move(workbook_rels));

    entries.emplace_back(
        "xl/styles.xml",
        "<?xml version=\"1.0\" encoding=\"UTF-8\" standalone=\"yes\"?>"
        "<styleSheet xmlns=\"http://schemas.openxmlformats.org/spreadsheetml/"
        "2006/main\"><fonts count=\"1\"><font><sz val=\"11\"/><name "
        "val=\"Calibri\"/></font></fonts><fills count=\"1\"><fill>"
        "<patternFill patternType=\"none\"/></fill></fills><borders "
        "count=\"1\"><border/></borders><cellStyleXfs count=\"1\"><xf "
        "numFmtId=\"0\" fontId=\"0\" fillId=\"0\" borderId=\"0\"/></cellStyleXfs>"
        "<cellXfs count=\"1\"><xf numFmtId=\"0\" fontId=\"0\" fillId=\"0\" "
        "borderId=\"0\" xfId=\"0\"/></cellXfs><cellStyles count=\"1\">"
        "<cellStyle name=\"Normal\" xfId=\"0\" builtinId=\"0\"/>"
        "</cellStyles></styleSheet>");

    for (std::size_t index = 0; index < sheets.size(); ++index) {
        entries.emplace_back("xl/worksheets/sheet" +
                                 std::to_string(index + 1U) + ".xml",
                             sheet_xml(sheets[index]));
    }
    return build_zip(entries);
}

}  // namespace fincore
