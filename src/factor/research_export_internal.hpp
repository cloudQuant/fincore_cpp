#pragma once

#include <fincore/factor/research.hpp>

#include <filesystem>
#include <functional>
#include <string>
#include <vector>

namespace fincore::research_export_detail {

using row_emitter = std::function<void(const std::vector<std::string>&)>;
using table_generator = std::function<void(const row_emitter&)>;
using table_writer = std::function<void(
    const std::filesystem::path&, const std::vector<std::string>&,
    const table_generator&)>;

void write_table_rows(
    const std::filesystem::path& path,
    const std::vector<std::string>& header,
    const std::vector<std::vector<std::string>>& rows);

research_export_result export_factor_research_with_writer(
    const research_result& result, const std::string& directory,
    bool charts, const table_writer& writer);

}  // namespace fincore::research_export_detail
