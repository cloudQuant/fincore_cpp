#include <fincore/attribution.hpp>
#include <fincore/calendar.hpp>
#include <fincore/dataframe.hpp>
#include <fincore/factor/calendar.hpp>
#include <fincore/metrics.hpp>
#include <fincore/portfolio.hpp>
#include <fincore/portfolio/models.hpp>
#include <fincore/report/html.hpp>
#include <fincore/report/models.hpp>
#include <fincore/report/xlsx.hpp>
#include <fincore/runtime.hpp>
#include <fincore/runtime/serialization.hpp>
#include <fincore/runtime/utilities.hpp>
#include <fincore/simulation.hpp>
#include <fincore/viz.hpp>
#include <fincore/viz/render_model.hpp>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <map>
#include <optional>
#include <span>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <system_error>
#include <vector>

namespace {

struct temporary_directory {
    temporary_directory() {
        const auto base = std::filesystem::canonical(
            std::filesystem::temp_directory_path());
        const auto token = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        path = base / ("fincore-package-consumer-" + std::to_string(token));
        if (!std::filesystem::create_directory(path)) {
            throw std::runtime_error{
                "could not create consumer scratch directory"};
        }
    }

    ~temporary_directory() {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }

    std::filesystem::path path;
};

std::string read_bytes(const std::filesystem::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input.is_open()) {
        throw std::runtime_error{"could not read installed artifact"};
    }
    std::ostringstream bytes;
    bytes << input.rdbuf();
    return bytes.str();
}

std::string manifest_file_record(const std::string& manifest,
                                 const std::string& path) {
    const auto path_field = "\"path\": \"" + path + "\"";
    const auto field_position = manifest.find(path_field);
    if (field_position == std::string::npos) {
        return {};
    }
    const auto record_start = manifest.rfind("    {", field_position);
    const auto record_end = manifest.find("\n    }", field_position);
    if (record_start == std::string::npos || record_end == std::string::npos) {
        return {};
    }
    return manifest.substr(record_start, record_end + 6U - record_start);
}

bool verify_manifest_file(const std::string& manifest,
                          const std::filesystem::path& output_directory,
                          const std::string& path,
                          const std::string& media_type,
                          const std::string& format,
                          const std::string& expected_bytes) {
    const auto persisted_bytes = read_bytes(output_directory / path);
    if (persisted_bytes != expected_bytes) {
        std::cerr << "artifact bytes changed while writing: " << path << '\n';
        return false;
    }
    const auto record = manifest_file_record(manifest, path);
    const auto expected_provenance =
        "\"provenance\": {\n        \"format\": \"" + format +
        "\",\n        \"producer\": \"headless-package-consumer\"\n      }";
    const std::string_view sha_prefix{"\"sha256\": \""};
    const auto sha_position = record.find(sha_prefix);
    bool sha_is_64_lowercase_hex = false;
    if (sha_position != std::string::npos) {
        const auto sha_begin = sha_position + sha_prefix.size();
        if (sha_begin + 64U < record.size() &&
            record[sha_begin + 64U] == '\"') {
            const auto sha = std::string_view{record}.substr(sha_begin, 64U);
            sha_is_64_lowercase_hex = std::all_of(
                sha.begin(), sha.end(), [](const char value) {
                    return (value >= '0' && value <= '9') ||
                           (value >= 'a' && value <= 'f');
                });
        }
    }
    if (record.empty() ||
        record.find("\"media_type\": \"" + media_type + "\"") ==
            std::string::npos ||
        record.find("\"size_bytes\": " +
                    std::to_string(persisted_bytes.size())) ==
            std::string::npos ||
        !sha_is_64_lowercase_hex ||
        record.find(expected_provenance) == std::string::npos) {
        std::cerr << "manifest v2 descriptor mismatch for " << path << '\n';
        return false;
    }
    return true;
}

}  // namespace

static_assert(fincore::report_document_schema_version == 2);

int main() {
    auto frame = fincore::make_series({1, 2, 3}, "returns", {0.1, 0.2, 0.3});
    const auto values = fincore::require_double_column(frame, "returns");
    if (values.size() != 3 || values[1] != 0.2) {
        return 1;
    }

    const auto forward_columns = fincore::get_forward_returns_columns(
        {"1D", "5D", "invalid"});
    if (forward_columns != std::vector<std::string>{"1D", "5D"}) {
        return 2;
    }

    const auto bootstrap_frame =
        fincore::make_series({1, 2}, "returns", {0.0, 0.5});
    const std::vector<std::vector<std::size_t>> draw_indices{
        {0U, 1U}, {1U, 0U}};
    const auto fixed_bootstrap = fincore::bootstrap_distribution(
        bootstrap_frame,
        std::span<const std::vector<std::size_t>>{draw_indices},
        fincore::bootstrap_statistic::mean);
    const auto fixed_values =
        fincore::require_double_column(fixed_bootstrap, "bootstrap");
    if (fixed_values.size() != 2U || fixed_values[0] != 0.25 ||
        fixed_values[1] != 0.25) {
        std::cerr << "installed fixed-index bootstrap overload mismatch\n";
        return 8;
    }
    const auto seeded_bootstrap = fincore::bootstrap_distribution(
        bootstrap_frame, 2U, fincore::bootstrap_statistic::mean, 42U);
    if (fincore::require_double_column(seeded_bootstrap, "bootstrap").size() !=
        2U) {
        std::cerr << "installed seeded bootstrap overload mismatch\n";
        return 8;
    }

    const fincore::portfolio_inputs inputs{
        frame, std::nullopt, std::nullopt, std::nullopt};
    frame.get_column<double>("returns")[0] = 99.0;
    auto materialized = inputs.materialize();
    const auto copied_values = fincore::require_double_column(
        materialized.at("returns"), "returns");
    const std::vector<double> expected_values{0.1, 0.2, 0.3};
    if (materialized.size() != 1U ||
        materialized.count("returns") != 1U ||
        copied_values.size() != expected_values.size() ||
        !std::equal(copied_values.begin(), copied_values.end(),
                    expected_values.begin())) {
        return 3;
    }
    materialized.at("returns").get_column<double>("returns")[0] = -99.0;
    const auto rematerialized = inputs.materialize();
    if (fincore::require_double_column(rematerialized.at("returns"),
                                       "returns")[0] != 0.1) {
        return 3;
    }

    const auto catalog = fincore::builtin_catalog();
    const auto operation_ids = catalog.operation_ids();
    for (const auto* operation_id : {
             "report.portfolio.build_portfolio_report",
             "report.risk.build_risk_report",
             "risk.report.build_risk_validation_report",
             "risk.diagnostics.walk_forward_var"}) {
        if (std::find(operation_ids.begin(), operation_ids.end(), operation_id) ==
            operation_ids.end()) {
            return 4;
        }
    }

    fincore::report_metric metric;
    metric.key = "mean_return";
    metric.label = "Mean return";
    metric.number = 0.2;
    metric.unit = "fraction";
    fincore::report_table table;
    table.key = "observations";
    table.title = "Observations";
    table.columns = {"return"};
    table.rows = {{fincore::report_cell::number_cell(0.1)},
                  {fincore::report_cell::number_cell(0.2)},
                  {fincore::report_cell::number_cell(0.3)}};
    fincore::report_section section;
    section.key = "summary";
    section.title = "Summary";
    section.metrics = {metric};
    section.tables = {table};
    fincore::report_document document;
    document.domain = "consumer-check";
    document.title = "Headless package consumer";
    document.sections = {section};
    if (document.schema_version != 2) {
        return 5;
    }

    const auto html = fincore::render_html(document);
    if (html.rfind("<!doctype html>", 0) != 0U) {
        std::cerr << "HTML renderer did not emit a document doctype\n";
        return 6;
    }
    const auto title_begin = html.find("<title");
    const auto title_open_end = title_begin == std::string::npos
                                    ? std::string::npos
                                    : html.find('>', title_begin);
    const auto title_end = title_open_end == std::string::npos
                               ? std::string::npos
                               : html.find("</title>", title_open_end + 1U);
    if (title_open_end == std::string::npos || title_end == std::string::npos ||
        std::string_view{html}.substr(
            title_open_end + 1U, title_end - title_open_end - 1U) !=
            "Headless package consumer") {
        std::cerr << "HTML renderer did not preserve the report title content\n";
        return 6;
    }
    if (html != fincore::render_html(document)) {
        std::cerr << "HTML rendering was not deterministic\n";
        return 6;
    }
    const auto workbook = fincore::render_xlsx(document);
    if (workbook.size() < 4U ||
        workbook.substr(0, 4) != std::string("PK\x03\x04", 4) ||
        workbook != fincore::render_xlsx(document)) {
        return 7;
    }

    fincore::artifact_bundle bundle;
    bundle.artifacts["report.html"] = html;
    bundle.artifacts["report.xlsx"] = workbook;
    bundle.descriptors["report.html"] = {
        "text/html",
        {{"format", "html"}, {"producer", "headless-package-consumer"}}};
    bundle.descriptors["report.xlsx"] = {
        "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
        {{"format", "xlsx"}, {"producer", "headless-package-consumer"}}};

    temporary_directory temporary;
    const auto output_directory = temporary.path / "artifacts";
    fincore::write_artifact_bundle(bundle, output_directory);
    const auto manifest =
        read_bytes(output_directory / ".fincore-manifest.json");
    if (manifest.find("\"schema_version\": 2") == std::string::npos ||
        !verify_manifest_file(manifest, output_directory, "report.html",
                              "text/html", "html", html) ||
        !verify_manifest_file(
            manifest, output_directory, "report.xlsx",
            "application/vnd.openxmlformats-officedocument.spreadsheetml.sheet",
            "xlsx", workbook)) {
        std::cerr
            << "installed artifact bundle did not produce a valid v2 manifest\n";
        return 9;
    }
    std::cout << "fincore_headless_consumer: ok\n";
    return 0;
}
