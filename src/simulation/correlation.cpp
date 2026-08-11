#include <fincore/simulation/correlation.hpp>

#include <fincore/error.hpp>

#include <cmath>
#include <cstddef>
#include <limits>
#include <span>
#include <string>
#include <string_view>
#include <unordered_set>
#include <utility>
#include <vector>

namespace fincore {
namespace {

void validate_target_correlation(const double target_correlation) {
    if (!std::isfinite(target_correlation)) {
        throw input_error{"target correlation must be finite"};
    }
}

void validate_matrix_count(const std::size_t asset_count) {
    const auto maximum_timestamp_count =
        static_cast<std::size_t>(std::numeric_limits<timestamp>::max());
    if (asset_count > maximum_timestamp_count) {
        throw input_error{"correlation breakdown index is too large"};
    }
    if (asset_count != 0U &&
        asset_count > std::numeric_limits<std::size_t>::max() / asset_count) {
        throw input_error{"correlation breakdown matrix is too large"};
    }
}

std::vector<std::string> checked_asset_names(
    const std::span<const std::string_view> asset_names) {
    std::unordered_set<std::string> seen;
    seen.reserve(asset_names.size());
    std::vector<std::string> names;
    names.reserve(asset_names.size());
    for (const std::string_view name : asset_names) {
        if (name.empty()) {
            throw input_error{"asset names must not be empty"};
        }
        if (name.find('\0') != std::string_view::npos) {
            throw input_error{"asset names must not contain an embedded NUL"};
        }

        const std::string owned_name{name};
        if (!seen.insert(owned_name).second) {
            throw input_error{"asset names must be distinct"};
        }
        names.push_back(owned_name);
    }
    return names;
}

std::vector<timestamp> make_ordinal_index(const std::size_t asset_count) {
    std::vector<timestamp> index;
    index.reserve(asset_count);
    for (std::size_t position = 0; position < asset_count; ++position) {
        index.push_back(static_cast<timestamp>(position));
    }
    return index;
}

}  // namespace

dataframe generate_correlation_breakdown(
    const std::span<const std::string_view> asset_names,
    const double target_correlation) {
    validate_target_correlation(target_correlation);
    validate_matrix_count(asset_names.size());
    const auto names = checked_asset_names(asset_names);

    dataframe result;
    result.load_index(make_ordinal_index(names.size()));
    for (std::size_t column = 0; column < names.size(); ++column) {
        std::vector<double> values(names.size(), target_correlation);
        values[column] = 1.0;
        result.load_column<double>(names[column].c_str(), std::move(values),
                                   hmdf::nan_policy::dont_pad_with_nans);
    }
    return result;
}

}  // namespace fincore
