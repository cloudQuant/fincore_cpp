#include <fincore/calendar.hpp>
#include <fincore/factor/exceptions.hpp>
#include <fincore/factor/research_detailed_export.hpp>
#include <fincore/factor/serialization.hpp>

#include <gtest/gtest.h>

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <limits>
#include <map>
#include <optional>
#include <stdexcept>
#include <string>
#include <system_error>
#include <vector>

namespace {

std::filesystem::path unique_temp_path(const char* prefix) {
    for (int attempt = 0; attempt < 8; ++attempt) {
        const auto stamp = std::chrono::steady_clock::now()
                               .time_since_epoch()
                               .count();
        auto candidate = std::filesystem::temp_directory_path() /
                         (std::string{prefix} + std::to_string(stamp) +
                          "-" + std::to_string(attempt));
        std::error_code error;
        if (!std::filesystem::exists(candidate, error) && !error) {
            return candidate;
        }
    }
    throw std::runtime_error{"could not allocate a unique test directory"};
}

std::string read_file(const std::filesystem::path& path) {
    std::ifstream stream(path, std::ios::binary);
    EXPECT_TRUE(stream) << path.string();
    return {std::istreambuf_iterator<char>(stream),
            std::istreambuf_iterator<char>()};
}

std::string shortest_double(const double value) {
    char text[64];
    const auto converted = std::to_chars(
        text, text + sizeof(text), value, std::chars_format::general);
    if (converted.ec != std::errc{}) {
        throw std::runtime_error{"could not format expected test number"};
    }
    return {text, converted.ptr};
}

void expect_header(const std::filesystem::path& directory,
                   const char* filename, const char* header) {
    const auto contents = read_file(directory / filename);
    const auto line_end = contents.find('\n');
    ASSERT_NE(line_end, std::string::npos) << filename;
    EXPECT_EQ(contents.substr(0U, line_end + 1U),
              std::string{header} + "\n")
        << filename;
}

void expect_manifest_columns(
    const fincore::factor_value& manifest, const char* table_name,
    const std::vector<std::string>& expected) {
    const auto& values = manifest.at("tables")
                             .at(table_name)
                             .at("columns")
                             .as_array();
    std::vector<std::string> actual;
    actual.reserve(values.size());
    for (const auto& value : values) {
        actual.push_back(value.as_string());
    }
    EXPECT_EQ(actual, expected) << table_name;
}

void remove_known_export_files(const std::filesystem::path& directory) {
    constexpr const char* files[] = {
        "signals.csv",       "labels.csv",      "ic.csv",
        "ic_summary.csv",    "quantile_returns.csv", "turnover.csv",
        "spreads.csv",       "stability.csv",   "coverage.csv",
        "diagnostics.csv",   "manifest.json"};
    for (const auto* file : files) {
        std::error_code ignored;
        std::filesystem::remove(directory / file, ignored);
    }
}

struct temporary_export_directory {
    std::filesystem::path path = unique_temp_path("fincore-detailed-export-");
    bool owns_directory{false};

    void create() {
        owns_directory = std::filesystem::create_directory(path);
    }

    ~temporary_export_directory() {
        if (!owns_directory) {
            return;
        }
        remove_known_export_files(path);
        std::error_code ignored;
        std::filesystem::remove(path / "sentinel.txt", ignored);
        if (owns_directory || std::filesystem::exists(path, ignored)) {
            ignored.clear();
            std::filesystem::remove(path, ignored);
        }
    }
};

const std::vector<std::string>& detailed_export_filenames() {
    static const std::vector<std::string> filenames{
        "signals.csv", "labels.csv", "ic.csv", "ic_summary.csv",
        "quantile_returns.csv", "turnover.csv", "spreads.csv",
        "stability.csv", "coverage.csv", "diagnostics.csv",
        "manifest.json"};
    return filenames;
}

void expect_same_diagnostics(
    const std::vector<fincore::research_detailed_diagnostic>& actual,
    const std::vector<fincore::research_detailed_diagnostic>& expected) {
    ASSERT_EQ(actual.size(), expected.size());
    for (std::size_t index = 0; index < expected.size(); ++index) {
        SCOPED_TRACE(index);
        EXPECT_EQ(actual[index].factor, expected[index].factor);
        EXPECT_EQ(actual[index].stage, expected[index].stage);
        EXPECT_EQ(actual[index].severity, expected[index].severity);
        EXPECT_EQ(actual[index].code, expected[index].code);
        EXPECT_EQ(actual[index].message, expected[index].message);
    }
}

void expect_export_bytes_equal(const std::filesystem::path& left,
                               const std::filesystem::path& right) {
    for (const auto& filename : detailed_export_filenames()) {
        SCOPED_TRACE(filename);
        EXPECT_EQ(read_file(left / filename), read_file(right / filename));
    }
}

struct combined_export_inputs {
    std::string factor_name;
    fincore::factor_series factors;
    fincore::dataframe prices;
    fincore::research_spec spec;
};

combined_export_inputs make_combined_export_inputs() {
    using fincore::utc_timestamp;
    constexpr fincore::timestamp day = 86'400'000'000'000LL;
    const auto start = utc_timestamp(2024, 6, 3);
    const double nan = std::numeric_limits<double>::quiet_NaN();
    const std::string chinese{"\xE4\xB8\xAD", 3U};

    combined_export_inputs inputs;
    inputs.factor_name = std::string{"factor,\""} + chinese + "\"";
    inputs.factors.dates = {start, start, start, start + day, start + day,
                            start + 2 * day, start + 2 * day,
                            start + 2 * day};
    inputs.factors.assets = {"A", "B", "C", "A", "C", "A", "B", "C"};
    inputs.factors.values = {1.0, 1.0, nan, 2.0, 4.0, 3.0, 5.0, 6.0};

    inputs.prices.load_index(std::vector<fincore::timestamp>{
        start, start + day, start + 2 * day, start + 3 * day,
        start + 4 * day, start + 5 * day});
    inputs.prices.load_column<double>(
        "A", {10.0, 11.0, 12.0, 13.0, 14.0, 15.0},
        hmdf::nan_policy::dont_pad_with_nans);
    inputs.prices.load_column<double>(
        "B", {20.0, 21.0, 22.0, 23.0, 24.0, 25.0},
        hmdf::nan_policy::dont_pad_with_nans);
    inputs.prices.load_column<double>(
        "C", {30.0, 31.0, 32.0, nan, 34.0, 35.0},
        hmdf::nan_policy::dont_pad_with_nans);

    inputs.spec.targets = {"A", "B", "C"};
    inputs.spec.start = start;
    inputs.spec.end = start + 2 * day;
    inputs.spec.periods = {2, 1};
    inputs.spec.quantiles = 2;
    inputs.spec.min_assets = 2;
    inputs.spec.min_quantile_assets = 1;
    inputs.spec.max_loss = 1.0;
    inputs.spec.stability_lags = {5};
    inputs.spec.label = "next_open_to_open";
    inputs.spec.preprocess = "cross_section_rank";
    inputs.spec.price_basis = "open";
    inputs.spec.snapshot_id = "p124-combined-parity";
    inputs.spec.groups = {{"A", "north,blue"},
                          {"B", "west\"quoted"},
                          {"C", std::string{"south-"} + chinese}};
    inputs.spec.direction_overrides.emplace(inputs.factor_name, -1);
    return inputs;
}

fincore::research_detailed_result populated_result() {
    using fincore::utc_timestamp;
    const auto second = utc_timestamp(2026, 10, 25, 1, 2, 3);
    const auto nanosecond = utc_timestamp(2026, 10, 25, 1, 2, 3, 1);

    fincore::research_detailed_result result;
    result.status = "PARTIAL";
    result.manifest = {{"factor", "alpha,003"},
                       {"periods", "1D,2D"},
                       {"snapshot_id", "p112-export-test"}};
    result.signals = {
        {second, "A,1", "F\"x", -1, -0.0,
         std::numeric_limits<double>::infinity(), "none", "west\r\nzone"},
        {nanosecond, std::string{"\xE4\xB8\xAD", 3U}, "F\"x", 1, 1e-20,
         -std::numeric_limits<double>::infinity(), "none", ""}};
    result.labels = {{second, "A,1", "1D",
                      std::numeric_limits<double>::quiet_NaN()},
                     {nanosecond, std::string{"\xE4\xB8\xAD", 3U}, "2D",
                      -0.0}};
    result.ic = {{nanosecond, "F\"x", "2D",
                  std::numeric_limits<double>::infinity(),
                  -std::numeric_limits<double>::infinity(), 2U},
                 {second, "F\"x", "1D", -0.0, 0.125, 3U}};
    result.ic_summary = {{"F\"x", "1D", 0.25, 0.5, 0.5,
                          std::numeric_limits<double>::quiet_NaN(),
                          0.125, 0.0, 4U}};
    result.quantile_returns = {
        {second, "F\"x", "1D", 3, 1e-20, 6U}};
    result.turnover = {{nanosecond, "F\"x", "2D", 1, -0.0}};
    result.spreads = {{second, "F\"x", "1D", 0.125, 3, 1}};
    result.stability = {{"F\"x", 7, 0.75,
                         std::numeric_limits<double>::quiet_NaN(), 3U}};
    result.coverage = {{"F\"x", "factor", -1, 12U, 10U, 8U, 7U,
                        "PARTIAL"}};
    result.diagnostics = {
        {std::nullopt, "labels,build", "warning", "bad\"code",
         "first line\nsecond line"}};
    return result;
}

TEST(FactorResearchDetailedExport,
     WritesTenCanonicalTablesWithEscapingPrecisionAndManifestRows) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    const auto result = populated_result();

    const auto exported = fincore::export_factor_research_detailed(
        result, directory.path.string());
    directory.owns_directory = true;

    EXPECT_EQ(exported.directory, directory.path.string());
    EXPECT_EQ(exported.files,
              (std::vector<std::string>{
                  "signals.csv", "labels.csv", "ic.csv", "ic_summary.csv",
                  "quantile_returns.csv", "turnover.csv", "spreads.csv",
                  "stability.csv", "coverage.csv", "diagnostics.csv",
                  "manifest.json"}));

    expect_header(directory.path, "signals.csv",
                  "time,asset,factor,direction,value,processed_value,preprocess,group");
    expect_header(directory.path, "labels.csv", "time,asset,period,value");
    expect_header(directory.path, "ic.csv",
                  "time,factor,period,ic,rank_ic,n_assets");
    expect_header(directory.path, "ic_summary.csv",
                  "factor,period,mean_ic,std_ic,icir,mean_rank_ic,std_rank_ic,rank_icir,n_dates");
    expect_header(directory.path, "quantile_returns.csv",
                  "time,factor,period,quantile,mean_return,n_assets");
    expect_header(directory.path, "turnover.csv",
                  "time,factor,period,quantile,turnover");
    expect_header(directory.path, "spreads.csv",
                  "time,factor,period,spread,top_quantile,bottom_quantile");
    expect_header(directory.path, "stability.csv",
                  "factor,lag,mean_rank_autocorrelation,std_rank_autocorrelation,n_dates");
    expect_header(directory.path, "coverage.csv",
                  "factor,kind,direction,input_cells,finite_cells,label_cells,evaluable_cells,status");
    expect_header(directory.path, "diagnostics.csv",
                  "factor,stage,severity,code,message");

    EXPECT_EQ(
        read_file(directory.path / "signals.csv"),
        "time,asset,factor,direction,value,processed_value,preprocess,group\n"
        "2026-10-25T01:02:03Z,\"A,1\",\"F\"\"x\",-1,-0.0,inf,none,"
        "\"west\r\nzone\"\n"
        "2026-10-25T01:02:03.000000001Z,\xE4\xB8\xAD,\"F\"\"x\",1,1e-20,-inf,none,\n");
    EXPECT_EQ(read_file(directory.path / "labels.csv"),
              "time,asset,period,value\n"
              "2026-10-25T01:02:03Z,\"A,1\",1D,\n"
              "2026-10-25T01:02:03.000000001Z,\xE4\xB8\xAD,2D,-0.0\n");
    EXPECT_EQ(read_file(directory.path / "ic.csv"),
              "time,factor,period,ic,rank_ic,n_assets\n"
              "2026-10-25T01:02:03.000000001Z,\"F\"\"x\",2D,inf,-inf,2\n"
              "2026-10-25T01:02:03Z,\"F\"\"x\",1D,-0.0,0.125,3\n");
    EXPECT_EQ(read_file(directory.path / "quantile_returns.csv"),
              "time,factor,period,quantile,mean_return,n_assets\n"
              "2026-10-25T01:02:03Z,\"F\"\"x\",1D,3,1e-20,6\n");
    EXPECT_EQ(read_file(directory.path / "turnover.csv"),
              "time,factor,period,quantile,turnover\n"
              "2026-10-25T01:02:03.000000001Z,\"F\"\"x\",2D,1,-0.0\n");
    EXPECT_EQ(read_file(directory.path / "ic_summary.csv"),
              "factor,period,mean_ic,std_ic,icir,mean_rank_ic,std_rank_ic,rank_icir,n_dates\n"
              "\"F\"\"x\",1D,0.25,0.5,0.5,,0.125,0.0,4\n");
    EXPECT_EQ(read_file(directory.path / "spreads.csv"),
              "time,factor,period,spread,top_quantile,bottom_quantile\n"
              "2026-10-25T01:02:03Z,\"F\"\"x\",1D,0.125,3,1\n");
    EXPECT_EQ(read_file(directory.path / "stability.csv"),
              "factor,lag,mean_rank_autocorrelation,std_rank_autocorrelation,n_dates\n"
              "\"F\"\"x\",7,0.75,,3\n");
    EXPECT_EQ(read_file(directory.path / "coverage.csv"),
              "factor,kind,direction,input_cells,finite_cells,label_cells,evaluable_cells,status\n"
              "\"F\"\"x\",factor,-1,12,10,8,7,PARTIAL\n");
    EXPECT_EQ(read_file(directory.path / "diagnostics.csv"),
              "factor,stage,severity,code,message\n"
              ",\"labels,build\",warning,\"bad\"\"code\",\"first line\n"
              "second line\"\n");

    const auto manifest = fincore::parse_json_value(
        read_file(directory.path / "manifest.json"));
    EXPECT_EQ(manifest.at("schema_id").as_string(),
              "fincore.factor_research_detailed.v2");
    EXPECT_EQ(manifest.at("schema_version").as_integer(), 2);
    EXPECT_EQ(manifest.at("api_version").as_string(), "2.0");
    EXPECT_EQ(manifest.at("status").as_string(), "PARTIAL");
    EXPECT_EQ(manifest.at("engine").at("name").as_string(), "fincore_cpp");
    const auto& omitted =
        manifest.at("engine").at("omitted_empty_tables").as_array();
    ASSERT_EQ(omitted.size(), 2U);
    EXPECT_EQ(omitted[0].as_string(), "correlation");
    EXPECT_EQ(omitted[1].as_string(), "comparisons");
    EXPECT_EQ(manifest.at("result_manifest").at("factor").as_string(),
              "alpha,003");
    const auto& tables = manifest.at("tables").as_object();
    ASSERT_EQ(tables.size(), 10U);
    EXPECT_EQ(tables.at("signals").at("file").as_string(), "signals.csv");
    EXPECT_EQ(tables.at("signals").at("rows").as_integer(), 2);
    EXPECT_EQ(tables.at("labels").at("rows").as_integer(), 2);
    EXPECT_EQ(tables.at("ic").at("rows").as_integer(), 2);
    EXPECT_EQ(tables.at("ic_summary").at("rows").as_integer(), 1);
    EXPECT_EQ(tables.at("quantile_returns").at("rows").as_integer(), 1);
    EXPECT_EQ(tables.at("turnover").at("rows").as_integer(), 1);
    EXPECT_EQ(tables.at("spreads").at("rows").as_integer(), 1);
    EXPECT_EQ(tables.at("stability").at("rows").as_integer(), 1);
    EXPECT_EQ(tables.at("coverage").at("rows").as_integer(), 1);
    EXPECT_EQ(tables.at("diagnostics").at("rows").as_integer(), 1);
    expect_manifest_columns(
        manifest, "signals",
        {"time", "asset", "factor", "direction", "value",
         "processed_value", "preprocess", "group"});
    expect_manifest_columns(manifest, "labels",
                            {"time", "asset", "period", "value"});
    expect_manifest_columns(
        manifest, "ic",
        {"time", "factor", "period", "ic", "rank_ic", "n_assets"});
    expect_manifest_columns(
        manifest, "ic_summary",
        {"factor", "period", "mean_ic", "std_ic", "icir",
         "mean_rank_ic", "std_rank_ic", "rank_icir", "n_dates"});
    expect_manifest_columns(
        manifest, "quantile_returns",
        {"time", "factor", "period", "quantile", "mean_return",
         "n_assets"});
    expect_manifest_columns(
        manifest, "turnover",
        {"time", "factor", "period", "quantile", "turnover"});
    expect_manifest_columns(
        manifest, "spreads",
        {"time", "factor", "period", "spread", "top_quantile",
         "bottom_quantile"});
    expect_manifest_columns(
        manifest, "stability",
        {"factor", "lag", "mean_rank_autocorrelation",
         "std_rank_autocorrelation", "n_dates"});
    expect_manifest_columns(
        manifest, "coverage",
        {"factor", "kind", "direction", "input_cells", "finite_cells",
         "label_cells", "evaluable_cells", "status"});
    expect_manifest_columns(
        manifest, "diagnostics",
        {"factor", "stage", "severity", "code", "message"});
}

TEST(FactorResearchDetailedExport,
     ReusesEscapedTextByColumnContentAndKeepsWriterStateLocal) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    fincore::research_detailed_result result;
    const std::string chinese{"\xE4\xB8\xAD", 3U};
    const std::string escaped_factor =
        std::string{"factor,\"quoted\"\r\n"} + chinese;
    const std::string same_factor{escaped_factor};
    ASSERT_NE(escaped_factor.data(), same_factor.data());

    result.signals = {
        {0, "asset-one", escaped_factor, 1, 1.0, 2.0, "", "group,west"},
        {1, "asset-two", same_factor, 1, 1.0, 2.0, "", "group,west"},
        {2, "asset-three", chinese, 1, 1.0, 2.0, "step,\"two\"\r\n", ""},
        {3, "asset-four", "", 1, 1.0, 2.0, "step,\"two\"\r\n",
         std::string{"zone,"} + chinese},
        {4, "asset-five", "", 1, 1.0, 2.0, "", std::string{"zone,"} + chinese}};
    result.labels = {
        {0, "label-one", "period,\"daily\"\r\n", 1.0},
        {1, "label-two", std::string{"period,\"daily\"\r\n"}, 2.0},
        {2, "label-three", chinese, 3.0}};

    static_cast<void>(fincore::export_factor_research_detailed(
        result, directory.path.string()));
    directory.owns_directory = true;

    const std::string escaped_factor_csv =
        std::string{"\"factor,\"\"quoted\"\"\r\n"} + chinese + "\"";
    const std::string escaped_step_csv = "\"step,\"\"two\"\"\r\n\"";
    const std::string escaped_zone_csv =
        std::string{"\"zone,"} + chinese + "\"";
    EXPECT_EQ(
        read_file(directory.path / "signals.csv"),
        "time,asset,factor,direction,value,processed_value,preprocess,group\n"
        "1970-01-01T00:00:00Z,asset-one," + escaped_factor_csv +
            ",1,1,2,,\"group,west\"\n" +
            "1970-01-01T00:00:00.000000001Z,asset-two," +
            escaped_factor_csv + ",1,1,2,,\"group,west\"\n" +
            "1970-01-01T00:00:00.000000002Z,asset-three," + chinese +
            ",1,1,2," + escaped_step_csv + ",\n" +
            "1970-01-01T00:00:00.000000003Z,asset-four,,1,1,2," +
            escaped_step_csv + "," + escaped_zone_csv + "\n" +
            "1970-01-01T00:00:00.000000004Z,asset-five,,1,1,2,," +
            escaped_zone_csv + "\n");
    EXPECT_EQ(
        read_file(directory.path / "labels.csv"),
        "time,asset,period,value\n"
        "1970-01-01T00:00:00Z,label-one,\"period,\"\"daily\"\"\r\n\",1\n"
        "1970-01-01T00:00:00.000000001Z,label-two,\"period,\"\"daily\"\"\r\n\",2\n"
        "1970-01-01T00:00:00.000000002Z,label-three," + chinese + ",3\n");
}

TEST(FactorResearchDetailedExport,
     RejectsChangedInvalidUtf8AfterCachedHitAndCleansOutput) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    fincore::research_detailed_result result;
    const std::string invalid_utf8(1U, static_cast<char>(0xFF));
    result.signals = {
        {0, "asset-one", "valid-factor-value", 1, 1.0, 1.0, "none", ""},
        {1, "asset-two", std::string{"valid-factor-value"}, 1, 1.0, 1.0,
         "none", ""},
        {2, "asset-three", invalid_utf8, 1, 1.0, 1.0, "none", ""}};

    EXPECT_THROW(fincore::export_factor_research_detailed(
                     result, directory.path.string()),
                 fincore::factor_data_error);
    EXPECT_FALSE(std::filesystem::exists(directory.path));
}

TEST(FactorResearchDetailedExport,
     PreservesNegativeEpochNanosecondsAndInputRowOrder) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    fincore::research_detailed_result result;
    result.signals = {
        {-1, "negative-ns", "F", 1, 1.0, 1.0, "none", ""},
        {-1, "repeated-negative-ns", "F", 1, 1.0, 1.0, "none", ""},
        {1000, "positive-micro", "F", 1, 1.0, 1.0, "none", ""},
        {-1000, "negative-micro", "F", 1, 1.0, 1.0, "none", ""},
        {1, "positive-ns", "F", 1, 1.0, 1.0, "none", ""},
        {0, "epoch", "F", 1, 1.0, 1.0, "none", ""}};

    static_cast<void>(fincore::export_factor_research_detailed(
        result, directory.path.string()));
    directory.owns_directory = true;

    EXPECT_EQ(
        read_file(directory.path / "signals.csv"),
        "time,asset,factor,direction,value,processed_value,preprocess,group\n"
        "1969-12-31T23:59:59.999999999Z,negative-ns,F,1,1,1,none,\n"
        "1969-12-31T23:59:59.999999999Z,repeated-negative-ns,F,1,1,1,none,\n"
        "1970-01-01T00:00:00.000001Z,positive-micro,F,1,1,1,none,\n"
        "1969-12-31T23:59:59.999999Z,negative-micro,F,1,1,1,none,\n"
        "1970-01-01T00:00:00.000000001Z,positive-ns,F,1,1,1,none,\n"
        "1970-01-01T00:00:00Z,epoch,F,1,1,1,none,\n");
}

TEST(FactorResearchDetailedExport,
     FormatsIntegerAndFiniteFloatBoundariesWithoutRounding) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    fincore::research_detailed_result result;
    result.quantile_returns = {
        {0, "F", "1D", std::numeric_limits<int>::min(),
         std::numeric_limits<double>::max(),
         std::numeric_limits<std::size_t>::max()},
        {1, "F", "1D", std::numeric_limits<int>::max(),
         std::numeric_limits<double>::denorm_min(), 0U},
        {0, "F", "1D", 0, 0.0, 0U}};

    static_cast<void>(fincore::export_factor_research_detailed(
        result, directory.path.string()));
    directory.owns_directory = true;

    const auto expected_header =
        "time,factor,period,quantile,mean_return,n_assets\n";
    const auto expected_minimum_row =
        "1970-01-01T00:00:00Z,F,1D," +
        std::to_string(std::numeric_limits<int>::min()) + "," +
        shortest_double(std::numeric_limits<double>::max()) + "," +
        std::to_string(std::numeric_limits<std::size_t>::max()) + "\n";
    const auto expected_denormal_row =
        "1970-01-01T00:00:00.000000001Z,F,1D," +
        std::to_string(std::numeric_limits<int>::max()) + "," +
        shortest_double(std::numeric_limits<double>::denorm_min()) +
        ",0\n";
    EXPECT_EQ(read_file(directory.path / "quantile_returns.csv"),
              expected_header + expected_minimum_row + expected_denormal_row +
                  "1970-01-01T00:00:00Z,F,1D,0,0.0,0\n");
}

TEST(FactorResearchDetailedExport, EmptyTablesHaveHeadersAndZeroManifestRows) {
    temporary_export_directory directory;
    directory.create();
    ASSERT_TRUE(directory.owns_directory);
    fincore::research_detailed_result result;
    result.status = "UNEVALUABLE";

    static_cast<void>(fincore::export_factor_research_detailed(
        result, directory.path.string()));
    directory.owns_directory = true;

    EXPECT_EQ(read_file(directory.path / "signals.csv"),
              "time,asset,factor,direction,value,processed_value,preprocess,group\n");
    EXPECT_EQ(read_file(directory.path / "diagnostics.csv"),
              "factor,stage,severity,code,message\n");
    const auto manifest = fincore::parse_json_value(
        read_file(directory.path / "manifest.json"));
    ASSERT_EQ(manifest.at("tables").as_object().size(), 10U);
    for (const auto& [name, table] : manifest.at("tables").as_object()) {
        static_cast<void>(name);
        EXPECT_EQ(table.at("rows").as_integer(), 0);
    }
}

TEST(FactorResearchDetailedExport,
     RejectsNonEmptyDestinationsWithoutChangingExistingFiles) {
    temporary_export_directory directory;
    directory.create();
    ASSERT_TRUE(directory.owns_directory);
    {
        std::ofstream sentinel(directory.path / "sentinel.txt",
                              std::ios::binary);
        sentinel << "preserve\n";
    }
    fincore::research_detailed_result result;

    EXPECT_THROW(fincore::export_factor_research_detailed(
                     result, directory.path.string()),
                 fincore::factor_data_error);
    EXPECT_EQ(read_file(directory.path / "sentinel.txt"), "preserve\n");
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.path),
                            std::filesystem::directory_iterator{}),
              1);
}

TEST(FactorResearchDetailedExport,
     RejectsUnsupportedSchemaBeforeCreatingTheDestination) {
    temporary_export_directory directory;
    fincore::research_detailed_result result;
    result.schema_version = 1;

    EXPECT_THROW(fincore::export_factor_research_detailed(
                     result, directory.path.string()),
                 fincore::factor_data_error);
    EXPECT_FALSE(std::filesystem::exists(directory.path));
}

TEST(FactorResearchDetailedExport,
     RejectsNonEmptyUnexportedSingleFactorTablesBeforeCreatingOutput) {
    temporary_export_directory directory;
    fincore::research_detailed_result result;
    result.correlation.push_back({"F", "G", 0.5, 0.1, 3U});

    EXPECT_THROW(fincore::export_factor_research_detailed(
                     result, directory.path.string()),
                 fincore::factor_data_error);
    EXPECT_FALSE(std::filesystem::exists(directory.path));
}

TEST(FactorResearchDetailedExport,
     RejectsInvalidUtf8AndRemovesOnlyThisAttemptOutput) {
    const std::vector<std::string> invalid_utf8 = {
        std::string{"\xFF", 1U},
        std::string{"\x80", 1U},
        std::string{"\xC0\xAF", 2U},
        std::string{"\xE0\x80\x80", 3U},
        std::string{"\xED\xA0\x80", 3U},
        std::string{"\xF4\x90\x80\x80", 4U},
        std::string{"\xF0\x9F", 2U}};
    for (const auto& invalid_text : invalid_utf8) {
        temporary_export_directory directory;
        directory.owns_directory = true;
        fincore::research_detailed_result result;
        result.signals.push_back(
            {0, invalid_text, "F", 1, 0.5, 0.5, "none", ""});

        EXPECT_THROW(fincore::export_factor_research_detailed(
                         result, directory.path.string()),
                     fincore::factor_data_error);
        EXPECT_FALSE(std::filesystem::exists(directory.path));
    }
}

TEST(FactorResearchDetailedExport,
     ManifestFailureRemovesCsvFilesAndNewDestination) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    fincore::research_detailed_result result;
    result.manifest.emplace("invalid_utf8", std::string{"\xFF", 1U});

    EXPECT_THROW(fincore::export_factor_research_detailed(
                     result, directory.path.string()),
                 std::exception);
    EXPECT_FALSE(std::filesystem::exists(directory.path));
}

TEST(FactorResearchDetailedExport,
     CombinedApiMatchesOwningApiAndKeepsReturnedMetadataOwned) {
    temporary_export_directory owning_directory;
    owning_directory.owns_directory = true;
    temporary_export_directory combined_directory;
    combined_directory.owns_directory = true;

    std::optional<fincore::research_detailed_export_outcome> combined;
    std::string expected_status;
    std::map<std::string, std::string> expected_manifest;
    std::vector<fincore::research_detailed_diagnostic> expected_diagnostics;
    std::size_t expected_signal_rows = 0U;
    std::size_t expected_label_rows = 0U;
    std::vector<std::string> expected_files;
    const auto combined_directory_name = combined_directory.path.string();

    {
        auto inputs = make_combined_export_inputs();
        const auto owning_result = fincore::evaluate_factor_research_detailed(
            inputs.factor_name, inputs.factors, inputs.prices, inputs.spec);
        const auto owning_export = fincore::export_factor_research_detailed(
            owning_result, owning_directory.path.string());
        combined.emplace(
            fincore::evaluate_and_export_factor_research_detailed(
                inputs.factor_name, inputs.factors, inputs.prices, inputs.spec,
                combined_directory_name));
        const auto& combined_result = *combined;

        expected_status = owning_result.status;
        expected_manifest = owning_result.manifest;
        expected_diagnostics = owning_result.diagnostics;
        expected_signal_rows = owning_result.signals.size();
        expected_label_rows = owning_result.labels.size();
        expected_files = owning_export.files;

        EXPECT_EQ(combined_result.status, owning_result.status);
        EXPECT_EQ(combined_result.manifest, owning_result.manifest);
        expect_same_diagnostics(combined_result.diagnostics,
                                owning_result.diagnostics);
        EXPECT_EQ(combined_result.signal_rows, owning_result.signals.size());
        EXPECT_EQ(combined_result.label_rows, owning_result.labels.size());
        EXPECT_EQ(combined_result.exported.directory, combined_directory_name);
        EXPECT_EQ(combined_result.exported.files, owning_export.files);
        EXPECT_EQ(owning_export.files, detailed_export_filenames());
        expect_export_bytes_equal(owning_directory.path,
                                  combined_directory.path);

        EXPECT_EQ(combined_result.status, "OK");
        EXPECT_EQ(combined_result.manifest.at("periods"), "2D,1D");
        EXPECT_GT(combined_result.diagnostics.size(), 0U);
        EXPECT_EQ(combined_result.signal_rows, 8U);
        EXPECT_EQ(combined_result.label_rows, 16U);
        EXPECT_TRUE(std::any_of(
            owning_result.labels.begin(), owning_result.labels.end(),
            [](const auto& label) { return std::isnan(label.value); }));

        // The returned fields and files must not borrow from the input objects.
        inputs.factor_name = "mutated factor name";
        inputs.factors.values.front() = 99.0;
        inputs.spec.snapshot_id = "mutated snapshot";
        inputs.spec.groups.clear();
    }

    ASSERT_TRUE(combined.has_value());
    EXPECT_EQ(combined->status, expected_status);
    EXPECT_EQ(combined->manifest, expected_manifest);
    expect_same_diagnostics(combined->diagnostics, expected_diagnostics);
    EXPECT_EQ(combined->signal_rows, expected_signal_rows);
    EXPECT_EQ(combined->label_rows, expected_label_rows);
    EXPECT_EQ(combined->exported.directory, combined_directory_name);
    EXPECT_EQ(combined->exported.files, expected_files);
    expect_export_bytes_equal(owning_directory.path, combined_directory.path);
}

TEST(FactorResearchDetailedExport,
     CombinedApiMatchesOwningApiForDefaultGroupsAndSpecialValues) {
    temporary_export_directory owning_directory;
    owning_directory.owns_directory = true;
    temporary_export_directory combined_directory;
    combined_directory.owns_directory = true;

    auto inputs = make_combined_export_inputs();
    const double infinity = std::numeric_limits<double>::infinity();
    inputs.factors.values = {-0.0, infinity, -infinity, -0.0,
                             infinity, -infinity, -0.0, infinity};
    inputs.spec.preprocess = "none";
    inputs.spec.groups.clear();
    inputs.spec.direction_overrides.clear();

    const auto owning_result = fincore::evaluate_factor_research_detailed(
        inputs.factor_name, inputs.factors, inputs.prices, inputs.spec);
    const auto owning_export = fincore::export_factor_research_detailed(
        owning_result, owning_directory.path.string());
    const auto combined_result =
        fincore::evaluate_and_export_factor_research_detailed(
            inputs.factor_name, inputs.factors, inputs.prices, inputs.spec,
            combined_directory.path.string());

    EXPECT_EQ(combined_result.status, owning_result.status);
    EXPECT_EQ(combined_result.manifest, owning_result.manifest);
    expect_same_diagnostics(combined_result.diagnostics,
                            owning_result.diagnostics);
    EXPECT_EQ(combined_result.signal_rows, owning_result.signals.size());
    EXPECT_EQ(combined_result.label_rows, owning_result.labels.size());
    EXPECT_EQ(combined_result.exported.files, owning_export.files);
    expect_export_bytes_equal(owning_directory.path, combined_directory.path);

    const auto signals = read_file(combined_directory.path / "signals.csv");
    EXPECT_NE(signals.find(",1,-0.0,-0.0,none,default\n"),
              std::string::npos);
    EXPECT_NE(signals.find(",1,inf,inf,none,default\n"),
              std::string::npos);
    EXPECT_NE(signals.find(",1,-inf,-inf,none,default\n"),
              std::string::npos);
}

TEST(FactorResearchDetailedExport,
     CombinedApiKeepsUnevaluableEmptyTablesInOwningApiParity) {
    temporary_export_directory owning_directory;
    owning_directory.owns_directory = true;
    temporary_export_directory combined_directory;
    combined_directory.owns_directory = true;

    auto inputs = make_combined_export_inputs();
    for (double& value : inputs.factors.values) {
        value = std::numeric_limits<double>::quiet_NaN();
    }
    const auto owning_result = fincore::evaluate_factor_research_detailed(
        inputs.factor_name, inputs.factors, inputs.prices, inputs.spec);
    const auto owning_export = fincore::export_factor_research_detailed(
        owning_result, owning_directory.path.string());
    const auto combined_result =
        fincore::evaluate_and_export_factor_research_detailed(
            inputs.factor_name, inputs.factors, inputs.prices, inputs.spec,
            combined_directory.path.string());

    EXPECT_EQ(owning_result.status, "UNEVALUABLE");
    EXPECT_EQ(combined_result.status, owning_result.status);
    EXPECT_EQ(combined_result.manifest, owning_result.manifest);
    expect_same_diagnostics(combined_result.diagnostics,
                            owning_result.diagnostics);
    EXPECT_EQ(combined_result.signal_rows, owning_result.signals.size());
    EXPECT_EQ(combined_result.label_rows, owning_result.labels.size());
    EXPECT_EQ(combined_result.exported.files, owning_export.files);
    expect_export_bytes_equal(owning_directory.path, combined_directory.path);
    EXPECT_EQ(read_file(combined_directory.path / "ic.csv"),
              "time,factor,period,ic,rank_ic,n_assets\n");
    EXPECT_EQ(read_file(combined_directory.path / "quantile_returns.csv"),
              "time,factor,period,quantile,mean_return,n_assets\n");
}

TEST(FactorResearchDetailedExport,
     CombinedApiLateManifestFailureCleansOnlyItsNewDestination) {
    auto inputs = make_combined_export_inputs();
    inputs.spec.snapshot_id = std::string{1U, static_cast<char>(0xFF)};

    temporary_export_directory new_destination;
    new_destination.owns_directory = true;
    EXPECT_THROW(static_cast<void>(
                     fincore::evaluate_and_export_factor_research_detailed(
                         inputs.factor_name, inputs.factors, inputs.prices,
                         inputs.spec, new_destination.path.string())),
                 std::exception);
    EXPECT_FALSE(std::filesystem::exists(new_destination.path));

    temporary_export_directory existing_empty_destination;
    existing_empty_destination.create();
    ASSERT_TRUE(existing_empty_destination.owns_directory);
    EXPECT_THROW(static_cast<void>(
                     fincore::evaluate_and_export_factor_research_detailed(
                         inputs.factor_name, inputs.factors, inputs.prices,
                         inputs.spec, existing_empty_destination.path.string())),
                 std::exception);
    EXPECT_TRUE(std::filesystem::exists(existing_empty_destination.path));
    EXPECT_EQ(std::distance(
                  std::filesystem::directory_iterator(
                      existing_empty_destination.path),
                  std::filesystem::directory_iterator{}),
              0);
}

TEST(FactorResearchDetailedExport,
     CombinedApiPreservesExistingNonEmptyDestination) {
    temporary_export_directory directory;
    directory.create();
    ASSERT_TRUE(directory.owns_directory);
    {
        std::ofstream sentinel(directory.path / "sentinel.txt",
                              std::ios::binary);
        sentinel << "preserve combined target\n";
    }

    auto inputs = make_combined_export_inputs();
    EXPECT_THROW(static_cast<void>(
                     fincore::evaluate_and_export_factor_research_detailed(
                         inputs.factor_name, inputs.factors, inputs.prices,
                         inputs.spec, directory.path.string())),
                 fincore::factor_data_error);
    EXPECT_EQ(read_file(directory.path / "sentinel.txt"),
              "preserve combined target\n");
    EXPECT_EQ(std::distance(std::filesystem::directory_iterator(directory.path),
                            std::filesystem::directory_iterator{}),
              1);
}

TEST(FactorResearchDetailedExport,
     CombinedApiEvaluationFailureLeavesDestinationAbsent) {
    temporary_export_directory directory;
    directory.owns_directory = true;
    auto inputs = make_combined_export_inputs();
    inputs.spec.periods = {0};

    EXPECT_THROW(static_cast<void>(
                     fincore::evaluate_and_export_factor_research_detailed(
                         inputs.factor_name, inputs.factors, inputs.prices,
                         inputs.spec, directory.path.string())),
                 fincore::factor_data_error);
    EXPECT_FALSE(std::filesystem::exists(directory.path));
}

}  // namespace
