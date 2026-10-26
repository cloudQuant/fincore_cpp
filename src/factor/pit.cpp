#include <fincore/factor/pit.hpp>

#include <fincore/factor/exceptions.hpp>

#include <algorithm>
#include <cmath>
#include <map>
#include <set>
#include <tuple>
#include <utility>
#include <vector>

namespace fincore {
namespace {

void validate_point(const pit_point& point) {
    if (!std::isfinite(point.value)) {
        throw factor_data_error{"PIT value must be a finite real number"};
    }
    if (point.known_at < point.as_of) {
        throw factor_data_error{
            "known_at must not precede as_of"};
    }
    if (point.effective_from < point.known_at) {
        throw factor_data_error{
            "effective_from must not precede known_at"};
    }
}

}  // namespace

void validate_pit_alignment(const std::vector<pit_point>& points) {
    for (const auto& point : points) {
        validate_point(point);
    }
}

factor_series materialize_pit_factor(
    const std::vector<pit_observation>& observations,
    const std::vector<timestamp>& evaluation_dates) {
    for (std::size_t index = 1; index < evaluation_dates.size(); ++index) {
        if (!(evaluation_dates[index] > evaluation_dates[index - 1U])) {
            throw factor_data_error{
                "evaluation_dates must be sorted and duplicate-free"};
        }
    }
    std::set<std::tuple<std::string, timestamp, timestamp, timestamp>>
        revisions;
    // The uniqueness key is asset plus the full point-in-time triple, so two
    // observations that claim the same (as_of, known_at, effective_from) state
    // for one asset are rejected rather than resolved arbitrarily later.
    for (const auto& observation : observations) {
        if (observation.asset.empty()) {
            throw factor_data_error{
                "observation assets must be non-empty"};
        }
        validate_point(observation.point);
        if (!revisions
                 .emplace(observation.asset, observation.point.as_of,
                          observation.point.known_at,
                          observation.point.effective_from)
                 .second) {
            throw factor_data_error{
                "PIT observations must not contain duplicate revisions"};
        }
    }
    factor_series factor;
    for (const timestamp date : evaluation_dates) {
        std::map<std::string, const pit_observation*> latest;
        for (const auto& observation : observations) {
            if (observation.point.known_at > date ||
                observation.point.effective_from > date) {
                continue;
            }
            const auto found = latest.find(observation.asset);
            if (found == latest.end()) {
                latest[observation.asset] = &observation;
                continue;
            }
            const auto& current = *found->second;
            // Comparison order is significant: effective_from dominates, then
            // known_at, then as_of, so a revision that became effective later
            // wins even when it was known earlier.
            const auto key = std::tie(observation.point.effective_from,
                                      observation.point.known_at,
                                      observation.point.as_of);
            const auto current_key = std::tie(current.point.effective_from,
                                              current.point.known_at,
                                              current.point.as_of);
            if (key > current_key) {
                found->second = &observation;
            }
        }
        for (const auto& entry : latest) {
            if (!entry.second->in_universe) {
                continue;
            }
            factor.dates.push_back(date);
            factor.assets.push_back(entry.first);
            factor.values.push_back(entry.second->point.value);
        }
    }
    return factor;
}

prepared_factor_data prepare_pit_factor_data(
    const std::vector<pit_observation>& observations,
    const dataframe& prices, const std::vector<timestamp>& evaluation_dates,
    const prepare_factor_config& config) {
    if (config.filter_zscore.has_value()) {
        throw factor_data_error{
            "PIT factor preparation does not allow full-sample filter_zscore"};
    }
    const auto factor = materialize_pit_factor(observations, evaluation_dates);
    if (factor.dates.empty()) {
        throw factor_data_error{
            "PIT observations produced no eligible factor values for "
            "evaluation_dates"};
    }
    return prepare_factor_data(factor, prices, config);
}

}  // namespace fincore
