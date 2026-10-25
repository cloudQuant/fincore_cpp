#include <fincore/factor/research_detailed_export.hpp>

#include <fincore/detail/json.hpp>
#include <fincore/factor/exceptions.hpp>

#include "research_detailed_internal.hpp"

#include <charconv>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <limits>
#include <memory>
#include <string>
#include <string_view>
#include <system_error>
#include <utility>
#include <vector>

namespace fincore {
namespace {

constexpr std::size_t csv_buffer_limit = 1U << 20U;
constexpr std::size_t detailed_csv_count = 10U;
constexpr std::size_t detailed_export_file_count = detailed_csv_count + 1U;

void validate_utf8(const std::string_view text) {
    for (std::size_t position = 0U; position < text.size();) {
        const auto lead = static_cast<unsigned char>(text[position]);
        if (lead < 0x80U) {
            ++position;
            continue;
        }

        std::size_t length = 0U;
        std::uint32_t code_point = 0U;
        if (lead >= 0xC2U && lead <= 0xDFU) {
            length = 2U;
            code_point = lead & 0x1FU;
        } else if (lead >= 0xE0U && lead <= 0xEFU) {
            length = 3U;
            code_point = lead & 0x0FU;
        } else if (lead >= 0xF0U && lead <= 0xF4U) {
            length = 4U;
            code_point = lead & 0x07U;
        } else {
            throw factor_data_error{
                "detailed research CSV text must be valid UTF-8"};
        }
        if (position + length > text.size()) {
            throw factor_data_error{
                "detailed research CSV text must be valid UTF-8"};
        }
        for (std::size_t offset = 1U; offset < length; ++offset) {
            const auto continuation =
                static_cast<unsigned char>(text[position + offset]);
            if ((continuation & 0xC0U) != 0x80U) {
                throw factor_data_error{
                    "detailed research CSV text must be valid UTF-8"};
            }
            code_point = (code_point << 6U) | (continuation & 0x3FU);
        }
        if ((length == 3U && code_point < 0x800U) ||
            (length == 4U && code_point < 0x10000U) ||
            (code_point >= 0xD800U && code_point <= 0xDFFFU) ||
            code_point > 0x10FFFFU) {
            throw factor_data_error{
                "detailed research CSV text must be valid UTF-8"};
        }
        position += length;
    }
}

void append_csv_field(std::string& output, const std::string_view field) {
    validate_utf8(field);
    if (field.empty()) {
        return;
    }
    if (field.find_first_of(",\"\r\n") == std::string_view::npos) {
        output.append(field.data(), field.size());
        return;
    }

    output.push_back('"');
    for (const char character : field) {
        if (character == '"') {
            output.push_back('"');
        }
        output.push_back(character);
    }
    output.push_back('"');
}

class output_cleanup {
public:
    explicit output_cleanup(std::filesystem::path directory)
        : directory_(std::move(directory)) {
        files_.reserve(detailed_export_file_count);
    }

    output_cleanup(const output_cleanup&) = delete;
    output_cleanup& operator=(const output_cleanup&) = delete;

    ~output_cleanup() {
        if (!active_) {
            return;
        }
        for (const auto& file : files_) {
            std::error_code ignored;
            std::filesystem::remove(directory_ / file, ignored);
        }
        if (owns_directory_) {
            std::error_code ignored;
            std::filesystem::remove(directory_, ignored);
        }
    }

    void mark_directory_created() noexcept { owns_directory_ = true; }

    void mark_file_created(std::string file) {
        files_.push_back(std::move(file));
    }

    void release() noexcept { active_ = false; }

private:
    std::filesystem::path directory_;
    std::vector<std::string> files_;
    bool owns_directory_{false};
    bool active_{true};
};

void prepare_output_directory(const std::filesystem::path& target,
                              output_cleanup& cleanup) {
    std::error_code error;
    auto status = std::filesystem::symlink_status(target, error);
    if (error == std::errc::no_such_file_or_directory) {
        error.clear();
        status = std::filesystem::file_status{
            std::filesystem::file_type::not_found};
    } else if (error) {
        throw factor_data_error{
            "could not inspect detailed research export directory"};
    }

    if (status.type() != std::filesystem::file_type::not_found) {
        if (std::filesystem::is_symlink(status) ||
            !std::filesystem::is_directory(status)) {
            throw factor_data_error{
                "detailed research export target must be a directory"};
        }
        if (!std::filesystem::is_empty(target, error)) {
            if (error) {
                throw factor_data_error{
                    "could not inspect detailed research export directory"};
            }
            throw factor_data_error{
                "detailed research export directory is not empty"};
        }
        if (error) {
            throw factor_data_error{
                "could not inspect detailed research export directory"};
        }
        return;
    }

    const auto parent = target.parent_path();
    if (!parent.empty()) {
        std::filesystem::create_directories(parent, error);
        if (error) {
            throw factor_data_error{
                "could not create detailed research export parent directory"};
        }
    }

    const bool created = std::filesystem::create_directory(target, error);
    if (error || !created) {
        throw factor_data_error{
            "could not create detailed research export directory"};
    }
    cleanup.mark_directory_created();
}

void require_new_file_path(const std::filesystem::path& path) {
    std::error_code error;
    const auto status = std::filesystem::symlink_status(path, error);
    if (error == std::errc::no_such_file_or_directory) {
        return;
    }
    if (error) {
        throw factor_data_error{
            "could not inspect detailed research export file"};
    }
    if (status.type() != std::filesystem::file_type::not_found) {
        throw factor_data_error{
            "detailed research export file already exists"};
    }
}

class csv_writer {
public:
    csv_writer(const std::filesystem::path& path, std::string filename,
               const std::vector<std::string>& columns,
               output_cleanup& cleanup)
        : path_(path), column_count_(columns.size()),
          cached_text_fields_(columns.size()) {
        require_new_file_path(path_);
        stream_.open(path_, std::ios::binary | std::ios::out |
                                std::ios::trunc);
        if (!stream_) {
            throw factor_data_error{
                "could not create detailed research CSV file"};
        }
        cleanup.mark_file_created(std::move(filename));
        buffer_.reserve(csv_buffer_limit + 65536U);
        write_header(columns);
    }

    csv_writer(const csv_writer&) = delete;
    csv_writer& operator=(const csv_writer&) = delete;

    template <typename EmitFields>
    void write_row(EmitFields&& emit_fields) {
        if (rows_ == std::numeric_limits<std::size_t>::max()) {
            throw factor_data_error{
                "detailed research CSV row count overflow"};
        }
        if (row_open_) {
            throw factor_data_error{
                "detailed research CSV row is already open"};
        }
        row_open_ = true;
        columns_written_ = 0U;
        std::forward<EmitFields>(emit_fields)(*this);
        if (columns_written_ != column_count_) {
            throw factor_data_error{
                "detailed research CSV row does not match its schema"};
        }
        buffer_.push_back('\n');
        row_open_ = false;
        ++rows_;
        flush_if_needed();
    }

    void append_text(const std::string_view value) {
        begin_field();
        append_csv_field(buffer_, value);
    }

    void append_cached_text(const std::string_view value) {
        begin_field();
        auto& cached = cached_text_fields_[columns_written_ - 1U];
        if (cached.has_value && cached.value.size() == value.size() &&
            (value.empty() ||
             std::char_traits<char>::compare(
                 cached.value.data(), value.data(), value.size()) == 0)) {
            buffer_.append(cached.csv_field);
            return;
        }

        std::string csv_field;
        append_csv_field(csv_field, value);
        std::string owned_value;
        if (!value.empty()) {
            owned_value.assign(value.data(), value.size());
        }
        cached.value.swap(owned_value);
        cached.csv_field.swap(csv_field);
        cached.has_value = true;
        buffer_.append(cached.csv_field);
    }

    void append_integer(const int value) { append_integral(value); }

    void append_size(const std::size_t value) { append_integral(value); }

    void append_number(const double value) {
        if (std::isnan(value)) {
            begin_field();
            return;
        }
        if (std::isinf(value)) {
            append_ascii(std::signbit(value) ? "-inf" : "inf");
            return;
        }
        if (value == 0.0) {
            append_ascii(std::signbit(value) ? "-0.0" : "0.0");
            return;
        }

        char text[64];
        const auto converted = std::to_chars(
            text, text + sizeof(text), value, std::chars_format::general);
        if (converted.ec != std::errc{}) {
            throw factor_data_error{
                "could not format detailed research CSV number"};
        }
        append_ascii(std::string_view{
            text, static_cast<std::size_t>(converted.ptr - text)});
    }

    void append_timestamp(const timestamp value) {
        if (!has_cached_timestamp_ || value != cached_timestamp_) {
            std::string formatted = detail::format_timestamp(value);
            formatted.push_back('Z');
            cached_timestamp_text_.swap(formatted);
            cached_timestamp_ = value;
            has_cached_timestamp_ = true;
        }
        // Append synchronously: no view into the mutable timestamp cache escapes.
        append_ascii(cached_timestamp_text_);
    }

    std::size_t row_count() const noexcept { return rows_; }

    void finish() {
        flush_buffer();
        stream_.flush();
        if (!stream_) {
            throw factor_data_error{
                "could not write detailed research CSV file"};
        }
        stream_.close();
        if (!stream_) {
            throw factor_data_error{
                "could not close detailed research CSV file"};
        }
    }

private:
    struct cached_text_field {
        std::string value;
        std::string csv_field;
        bool has_value{false};
    };

    void begin_field() {
        if (!row_open_ || columns_written_ >= column_count_) {
            throw factor_data_error{
                "detailed research CSV row does not match its schema"};
        }
        if (columns_written_ != 0U) {
            buffer_.push_back(',');
        }
        ++columns_written_;
    }

    void append_ascii(const std::string_view value) {
        begin_field();
        buffer_.append(value.data(), value.size());
    }

    template <typename Integer>
    void append_integral(const Integer value) {
        char text[64];
        const auto converted = std::to_chars(text, text + sizeof(text), value);
        if (converted.ec != std::errc{}) {
            throw factor_data_error{
                "could not format detailed research CSV integer"};
        }
        append_ascii(std::string_view{
            text, static_cast<std::size_t>(converted.ptr - text)});
    }

    void write_header(const std::vector<std::string>& columns) {
        for (std::size_t index = 0; index < columns.size(); ++index) {
            if (index != 0U) {
                buffer_.push_back(',');
            }
            append_csv_field(buffer_, columns[index]);
        }
        buffer_.push_back('\n');
        flush_if_needed();
    }

    void flush_if_needed() {
        if (buffer_.size() >= csv_buffer_limit) {
            flush_buffer();
        }
    }

    void flush_buffer() {
        if (buffer_.empty()) {
            return;
        }
        stream_.write(buffer_.data(),
                      static_cast<std::streamsize>(buffer_.size()));
        if (!stream_) {
            throw factor_data_error{
                "could not write detailed research CSV file"};
        }
        buffer_.clear();
    }

    std::filesystem::path path_;
    std::ofstream stream_;
    std::string buffer_;
    std::size_t column_count_;
    std::size_t columns_written_{0U};
    std::size_t rows_{0U};
    timestamp cached_timestamp_{0};
    std::string cached_timestamp_text_;
    std::vector<cached_text_field> cached_text_fields_;
    bool has_cached_timestamp_{false};
    bool row_open_{false};
};

struct exported_table {
    std::string name;
    std::string file;
    std::vector<std::string> columns;
    std::size_t rows{0U};
};

template <typename GenerateRows>
exported_table write_table(const std::filesystem::path& target,
                           output_cleanup& cleanup, std::string name,
                           std::vector<std::string> columns,
                           GenerateRows&& generate_rows) {
    std::string filename = name + ".csv";
    csv_writer writer(target / filename, filename, columns, cleanup);
    generate_rows(writer);
    writer.finish();
    return {std::move(name), std::move(filename), std::move(columns),
            writer.row_count()};
}

std::int64_t json_row_count(const std::size_t rows) {
    if (rows > static_cast<std::size_t>(
                   std::numeric_limits<std::int64_t>::max())) {
        throw factor_data_error{
            "detailed research export row count exceeds manifest range"};
    }
    return static_cast<std::int64_t>(rows);
}

factor_value detailed_manifest(
    const research_detailed_result& result,
    const std::vector<exported_table>& exported_tables) {
    factor_value::object_type config;
    for (const auto& [key, value] : result.manifest) {
        config.emplace(key, factor_value{value});
    }

    factor_value::object_type engine;
    engine.emplace("name", factor_value{"fincore_cpp"});
    engine.emplace("result_schema_version",
                   factor_value{result.schema_version});
    factor_value::array_type omitted_empty_tables;
    omitted_empty_tables.emplace_back("correlation");
    omitted_empty_tables.emplace_back("comparisons");
    engine.emplace("omitted_empty_tables",
                   factor_value{std::move(omitted_empty_tables)});

    factor_value::object_type tables;
    for (const auto& table : exported_tables) {
        factor_value::array_type columns;
        columns.reserve(table.columns.size());
        for (const auto& column : table.columns) {
            columns.emplace_back(column);
        }

        factor_value::object_type descriptor;
        descriptor.emplace("file", factor_value{table.file});
        descriptor.emplace("columns", factor_value{std::move(columns)});
        descriptor.emplace("rows", factor_value{json_row_count(table.rows)});
        tables.emplace(table.name,
                       factor_value{std::move(descriptor)});
    }

    factor_value::object_type manifest;
    manifest.emplace("schema_id",
                     factor_value{research_detailed_export_schema_id});
    manifest.emplace("schema_version",
                     factor_value{research_detailed_export_schema_version});
    manifest.emplace("api_version",
                     factor_value{research_detailed_api_version});
    manifest.emplace("status", factor_value{result.status});
    manifest.emplace("engine", factor_value{std::move(engine)});
    manifest.emplace("result_manifest", factor_value{std::move(config)});
    manifest.emplace("tables", factor_value{std::move(tables)});
    return factor_value{std::move(manifest)};
}

void write_manifest(const std::filesystem::path& path,
                    std::string filename, const std::string& text,
                    output_cleanup& cleanup) {
    require_new_file_path(path);
    std::ofstream stream(path,
                         std::ios::binary | std::ios::out | std::ios::trunc);
    if (!stream) {
        throw factor_data_error{
            "could not create detailed research manifest"};
    }
    cleanup.mark_file_created(std::move(filename));
    stream.write(text.data(), static_cast<std::streamsize>(text.size()));
    stream.flush();
    if (!stream) {
        throw factor_data_error{
            "could not write detailed research manifest"};
    }
    stream.close();
    if (!stream) {
        throw factor_data_error{
            "could not close detailed research manifest"};
    }
}

}  // namespace

namespace {

void validate_export_directory(const std::string& directory) {
    if (directory.empty()) {
        throw factor_data_error{
            "detailed research export directory must not be empty"};
    }
    if (directory.find('\0') != std::string::npos) {
        throw factor_data_error{
            "detailed research export directory must not contain a null byte"};
    }
}

void validate_export_result(const research_detailed_result& result) {
    if (result.schema_version != research_detailed_result_schema_version) {
        throw factor_data_error{
            "unsupported detailed research result schema version"};
    }
    if (!result.correlation.empty() || !result.comparisons.empty()) {
        throw factor_data_error{
            "single-factor detailed export requires empty correlation and "
            "comparison tables"};
    }
}

void write_signal_fields(csv_writer& csv, const timestamp date,
                         const std::string_view asset,
                         const std::string_view factor, const int direction,
                         const double value, const double processed_value,
                         const std::string_view preprocess,
                         const std::string_view group) {
    csv.append_timestamp(date);
    csv.append_text(asset);
    csv.append_cached_text(factor);
    csv.append_integer(direction);
    csv.append_number(value);
    csv.append_number(processed_value);
    csv.append_cached_text(preprocess);
    csv.append_cached_text(group);
}

void write_label_fields(csv_writer& csv, const timestamp date,
                        const std::string_view asset,
                        const std::string_view period, const double value) {
    csv.append_timestamp(date);
    csv.append_text(asset);
    csv.append_cached_text(period);
    csv.append_number(value);
}

void write_remaining_tables(
    const std::filesystem::path& target, output_cleanup& cleanup,
    research_export_result& exported, std::vector<exported_table>& tables,
    const research_detailed_result& result) {
    auto add_table = [&](std::string name,
                         std::vector<std::string> columns,
                         auto&& generate_rows) {
        auto table = write_table(target, cleanup, std::move(name),
                                 std::move(columns),
                                 std::forward<decltype(generate_rows)>(
                                     generate_rows));
        exported.files.push_back(table.file);
        tables.push_back(std::move(table));
    };

    add_table("ic", {"time", "factor", "period", "ic", "rank_ic",
                      "n_assets"},
              [&](csv_writer& writer) {
        for (const auto& row : result.ic) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_timestamp(row.time);
                csv.append_cached_text(row.factor);
                csv.append_cached_text(row.period);
                csv.append_number(row.ic);
                csv.append_number(row.rank_ic);
                csv.append_size(row.n_assets);
            });
        }
    });

    add_table("ic_summary",
              {"factor", "period", "mean_ic", "std_ic", "icir",
               "mean_rank_ic", "std_rank_ic", "rank_icir", "n_dates"},
              [&](csv_writer& writer) {
        for (const auto& row : result.ic_summary) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_cached_text(row.factor);
                csv.append_cached_text(row.period);
                csv.append_number(row.mean_ic);
                csv.append_number(row.std_ic);
                csv.append_number(row.icir);
                csv.append_number(row.mean_rank_ic);
                csv.append_number(row.std_rank_ic);
                csv.append_number(row.rank_icir);
                csv.append_size(row.n_dates);
            });
        }
    });

    add_table("quantile_returns",
              {"time", "factor", "period", "quantile", "mean_return",
               "n_assets"},
              [&](csv_writer& writer) {
        for (const auto& row : result.quantile_returns) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_timestamp(row.time);
                csv.append_cached_text(row.factor);
                csv.append_cached_text(row.period);
                csv.append_integer(row.quantile);
                csv.append_number(row.mean_return);
                csv.append_size(row.n_assets);
            });
        }
    });

    add_table("turnover",
              {"time", "factor", "period", "quantile", "turnover"},
              [&](csv_writer& writer) {
        for (const auto& row : result.turnover) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_timestamp(row.time);
                csv.append_cached_text(row.factor);
                csv.append_cached_text(row.period);
                csv.append_integer(row.quantile);
                csv.append_number(row.turnover);
            });
        }
    });

    add_table("spreads",
              {"time", "factor", "period", "spread", "top_quantile",
               "bottom_quantile"},
              [&](csv_writer& writer) {
        for (const auto& row : result.spreads) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_timestamp(row.time);
                csv.append_cached_text(row.factor);
                csv.append_cached_text(row.period);
                csv.append_number(row.spread);
                csv.append_integer(row.top_quantile);
                csv.append_integer(row.bottom_quantile);
            });
        }
    });

    add_table("stability",
              {"factor", "lag", "mean_rank_autocorrelation",
               "std_rank_autocorrelation", "n_dates"},
              [&](csv_writer& writer) {
        for (const auto& row : result.stability) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_cached_text(row.factor);
                csv.append_integer(row.lag);
                csv.append_number(row.mean_rank_autocorrelation);
                csv.append_number(row.std_rank_autocorrelation);
                csv.append_size(row.n_dates);
            });
        }
    });

    add_table("coverage",
              {"factor", "kind", "direction", "input_cells", "finite_cells",
               "label_cells", "evaluable_cells", "status"},
              [&](csv_writer& writer) {
        for (const auto& row : result.coverage) {
            writer.write_row([&](csv_writer& csv) {
                csv.append_cached_text(row.factor);
                csv.append_text(row.kind);
                csv.append_integer(row.direction);
                csv.append_size(row.input_cells);
                csv.append_size(row.finite_cells);
                csv.append_size(row.label_cells);
                csv.append_size(row.evaluable_cells);
                csv.append_text(row.status);
            });
        }
    });

    add_table("diagnostics",
              {"factor", "stage", "severity", "code", "message"},
              [&](csv_writer& writer) {
        for (const auto& row : result.diagnostics) {
            writer.write_row([&](csv_writer& csv) {
                if (row.factor) {
                    csv.append_cached_text(*row.factor);
                } else {
                    csv.append_cached_text({});
                }
                csv.append_text(row.stage);
                csv.append_text(row.severity);
                csv.append_text(row.code);
                csv.append_text(row.message);
            });
        }
    });

    const std::string manifest_text =
        to_json_string(detailed_manifest(result, tables)) + "\n";
    const std::string manifest_file = "manifest.json";
    write_manifest(target / manifest_file, manifest_file, manifest_text,
                   cleanup);
    exported.files.push_back(manifest_file);
}

class detailed_export_session final
    : public research_detailed_detail::detailed_export_sink {
public:
    explicit detailed_export_session(const std::string& directory)
        : directory_(directory), target_(directory), cleanup_(target_) {
        exported_.directory = directory_;
        exported_.files.reserve(detailed_export_file_count);
        tables_.reserve(detailed_csv_count);
    }

    void write_signals(
        const research_detailed_detail::detailed_signal_columns& columns)
        override {
        if (signals_written_) {
            throw factor_data_error{
                "detailed research signals table was already written"};
        }
        start_output();
        const std::size_t rows = columns.dates.size();
        if (columns.assets.size() != rows ||
            columns.source_rows.size() != rows ||
            columns.processed_values.size() != rows) {
            throw factor_data_error{
                "detailed research signal columns do not match"};
        }

        auto table = write_table(
            target_, cleanup_, "signals",
            {"time", "asset", "factor", "direction", "value",
             "processed_value", "preprocess", "group"},
            [&](csv_writer& writer) {
                for (std::size_t row = 0U; row < rows; ++row) {
                    if (columns.source_rows[row] >=
                        columns.source_values.size()) {
                        throw factor_data_error{
                            "detailed research signal source row is invalid"};
                    }
                    const auto& asset = columns.assets[row];
                    const std::string_view group =
                        columns.groups != nullptr &&
                                !columns.groups->empty()
                            ? std::string_view{columns.groups->at(asset)}
                            : std::string_view{"default"};
                    const double value =
                        columns.source_values[columns.source_rows[row]] *
                        static_cast<double>(columns.direction);
                    writer.write_row([&](csv_writer& csv) {
                        write_signal_fields(
                            csv, columns.dates[row], asset, columns.factor,
                            columns.direction, value,
                            columns.processed_values[row],
                            columns.preprocess, group);
                    });
                }
            });
        record_table(std::move(table));
        signals_written_ = true;
    }

    void write_signals_owned(
        const std::vector<research_signal_row>& rows) {
        if (signals_written_) {
            throw factor_data_error{
                "detailed research signals table was already written"};
        }
        start_output();
        auto table = write_table(
            target_, cleanup_, "signals",
            {"time", "asset", "factor", "direction", "value",
             "processed_value", "preprocess", "group"},
            [&](csv_writer& writer) {
                for (const auto& row : rows) {
                    writer.write_row([&](csv_writer& csv) {
                        write_signal_fields(
                            csv, row.date, row.asset, row.factor,
                            row.direction, row.value, row.processed_value,
                            row.preprocess, row.group);
                    });
                }
            });
        record_table(std::move(table));
        signals_written_ = true;
    }

    void begin_labels() override {
        if (!signals_written_ || labels_started_) {
            throw factor_data_error{
                "detailed research labels table is out of sequence"};
        }
        labels_started_ = true;
        start_output();
        label_writer_ = std::make_unique<csv_writer>(
            target_ / "labels.csv", "labels.csv",
            std::vector<std::string>{"time", "asset", "period", "value"},
            cleanup_);
    }

    void write_label_horizon(
        const std::span<const timestamp> dates,
        const std::span<const std::string> assets,
        const research_detailed_detail::horizon_input& horizon,
        const std::span<unsigned char> has_any_label) override {
        if (label_writer_ == nullptr || dates.size() != assets.size() ||
            dates.size() != horizon.label_values.size() ||
            dates.size() != has_any_label.size()) {
            throw factor_data_error{
                "each detailed label horizon must match the panel row count"};
        }
        for (std::size_t row = 0U; row < dates.size(); ++row) {
            const double value = horizon.label_values[row];
            if (std::isfinite(value)) {
                has_any_label[row] = 1U;
            }
            label_writer_->write_row([&](csv_writer& csv) {
                write_label_fields(csv, dates[row], assets[row],
                                   horizon.period, value);
            });
        }
    }

    void write_labels_owned(const std::vector<research_label_row>& rows) {
        begin_labels();
        for (const auto& row : rows) {
            label_writer_->write_row([&](csv_writer& csv) {
                write_label_fields(csv, row.date, row.asset, row.period,
                                   row.value);
            });
        }
        finish_labels();
    }

    void finish_labels() override {
        if (label_writer_ == nullptr || labels_finished_) {
            throw factor_data_error{
                "detailed research labels table is out of sequence"};
        }
        label_writer_->finish();
        exported_table table{
            "labels", "labels.csv",
            {"time", "asset", "period", "value"},
            label_writer_->row_count()};
        label_writer_.reset();
        record_table(std::move(table));
        labels_finished_ = true;
    }

    research_export_result finish(
        const research_detailed_result& result) override {
        if (!signals_written_ || !labels_finished_ || finished_) {
            throw factor_data_error{
                "detailed research export is incomplete"};
        }
        validate_export_result(result);
        write_remaining_tables(target_, cleanup_, exported_, tables_, result);
        cleanup_.release();
        finished_ = true;
        return std::move(exported_);
    }

    research_export_result finish_owned(
        const research_detailed_result& result) {
        return finish(result);
    }

private:
    void start_output() {
        if (started_) {
            return;
        }
        prepare_output_directory(target_, cleanup_);
        started_ = true;
    }

    void record_table(exported_table table) {
        exported_.files.push_back(table.file);
        tables_.push_back(std::move(table));
    }

    std::string directory_;
    std::filesystem::path target_;
    output_cleanup cleanup_;
    research_export_result exported_;
    std::vector<exported_table> tables_;
    std::unique_ptr<csv_writer> label_writer_;
    bool started_{false};
    bool signals_written_{false};
    bool labels_started_{false};
    bool labels_finished_{false};
    bool finished_{false};
};

}  // namespace

namespace research_detailed_detail {

std::unique_ptr<detailed_export_sink> make_detailed_export_sink(
    const std::string& directory) {
    validate_export_directory(directory);
    return std::make_unique<detailed_export_session>(directory);
}

}  // namespace research_detailed_detail

research_export_result export_factor_research_detailed(
    const research_detailed_result& result, const std::string& directory) {
    validate_export_directory(directory);
    validate_export_result(result);
    detailed_export_session session{directory};
    session.write_signals_owned(result.signals);
    session.write_labels_owned(result.labels);
    return session.finish_owned(result);
}

research_detailed_export_outcome
evaluate_and_export_factor_research_detailed(
    const std::string_view factor_name, const factor_series& factors,
    const dataframe& prices, const research_spec& spec,
    const std::string& directory) {
    auto sink = research_detailed_detail::make_detailed_export_sink(directory);
    auto evaluation =
        research_detailed_detail::evaluate_factor_research_detailed_for_export(
            factor_name, factors, prices, spec, *sink);
    return {std::move(evaluation.result.status),
            std::move(evaluation.result.manifest),
            std::move(evaluation.result.diagnostics),
            evaluation.signal_rows,
            evaluation.label_rows,
            std::move(evaluation.exported)};
}

}  // namespace fincore
