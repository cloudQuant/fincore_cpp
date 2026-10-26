#pragma once

#include <fincore/factor/performance.hpp>
#include <fincore/factor/portfolio.hpp>

#include <cstddef>
#include <span>
#include <string_view>
#include <vector>

namespace fincore::detail {

/// Per-thread counts for expensive factor kernels; intended for diagnostics
/// and deterministic reuse tests, not as a global performance monitor.
struct factor_compute_diagnostics {
    std::size_t weight_kernel_calls = 0U;
    std::size_t information_coefficient_kernel_calls = 0U;
    std::size_t quantile_view_builds = 0U;
    std::size_t panel_preprocessings = 0U;
    std::size_t position_builds = 0U;
    std::size_t portfolio_input_builds = 0U;
};

void reset_factor_compute_diagnostics() noexcept;
factor_compute_diagnostics get_factor_compute_diagnostics() noexcept;
void set_factor_compute_diagnostics(
    const factor_compute_diagnostics& diagnostics) noexcept;

void note_quantile_view_build() noexcept;
void note_panel_preprocessing() noexcept;
void note_position_build() noexcept;
void note_portfolio_input_build() noexcept;

/// Scoped counter isolation for nested/concurrent tests. The constructor saves
/// the current thread's counters and resets them; the destructor restores the
/// saved values. Read snapshot() inside the scope to observe the inner work.
class factor_compute_diagnostics_scope {
public:
    factor_compute_diagnostics_scope() noexcept
        : saved_(get_factor_compute_diagnostics()) {
        reset_factor_compute_diagnostics();
    }
    ~factor_compute_diagnostics_scope() {
        set_factor_compute_diagnostics(saved_);
    }
    factor_compute_diagnostics_scope(
        const factor_compute_diagnostics_scope&) = delete;
    factor_compute_diagnostics_scope& operator=(
        const factor_compute_diagnostics_scope&) = delete;
    [[nodiscard]] factor_compute_diagnostics snapshot() const noexcept {
        return get_factor_compute_diagnostics();
    }

private:
    factor_compute_diagnostics saved_;
};

factor_ic_summary summarize_information_coefficient(
    const factor_ic_table& information);

struct quantile_return_views {
    quantile_return_table aggregate;
    quantile_date_return_table by_date;
};

/// Build the aggregate and/or by-date quantile views from one pass of
/// per-date cells. At least one output flag must be true.
quantile_return_views build_quantile_return_views(
    const prepared_factor_data& factor_data, bool by_group, bool demeaned,
    bool group_adjust, bool need_aggregate, bool need_by_date);

factor_return_table factor_returns_from_weights(
    const prepared_factor_data& factor_data, std::span<const double> weights);
factor_return_table factor_returns_from_weights(
    const prepared_factor_data& factor_data,
    std::span<const factor_weight_row> weights);

std::vector<factor_alpha_beta_row> factor_alpha_beta_from_returns(
    const prepared_factor_data& factor_data,
    const factor_return_table& portfolio_returns);

factor_series_result factor_cumulative_returns_from_table(
    const factor_return_table& returns, std::string_view period_label);

factor_portfolio_inputs build_factor_portfolio_inputs_from_tables(
    const prepared_factor_data& filtered_factor_data,
    const prepared_factor_data& benchmark_source_data,
    std::string_view period_label, bool long_short, bool group_neutral,
    bool equal_weight, const factor_return_table& returns,
    std::span<const factor_weight_row> weights);

factor_portfolio_inputs build_factor_portfolio_inputs_from_outputs(
    const prepared_factor_data& filtered_factor_data,
    const prepared_factor_data& benchmark_source_data,
    std::string_view period_label, const factor_return_table& returns,
    std::span<const factor_weight_row> positions);

}  // namespace fincore::detail
