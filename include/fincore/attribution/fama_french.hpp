#pragma once

#include <fincore/dataframe.hpp>

#include <array>
#include <cstddef>
#include <functional>
#include <map>
#include <optional>
#include <span>
#include <string>
#include <string_view>
#include <vector>

namespace fincore {

/// Standard Fama-French factor definitions, matching the pinned source.
inline constexpr std::array<std::string_view, 3U> FF3_FACTORS{"MKT", "SMB",
                                                              "HML"};
inline constexpr std::array<std::string_view, 5U> FF5_FACTORS{
    "MKT", "SMB", "HML", "RMW", "CMA"};
inline constexpr std::array<std::string_view, 4U> FF4MOM_FACTORS{
    "MKT", "SMB", "HML", "MOM"};

struct idiosyncratic_risk_result {
    std::vector<std::string> assets;
    std::vector<double> specific_volatility;
    double market_beta = 0.0;
};

/// Asset-specific volatility using the source's first-asset market beta.
///
/// When ``market_beta`` is absent, the beta is fitted by OLS of the first
/// asset's excess returns on the five Fama-French columns (MKT, SMB, HML,
/// RMW, CMA) and then applied to every asset, matching the pinned routine.
idiosyncratic_risk_result calculate_idiosyncratic_risk(
    const dataframe& returns, const dataframe& factor_data,
    std::optional<double> market_beta = std::nullopt,
    double risk_free = 0.0);

/// Result of the native OLS Fama-French fit.
///
/// The native kernel is the canonical least-squares factor regression. The
/// Python Newey-West/WLS covariance options are not part of that kernel, so
/// ``std_errors``/``p_values`` are deliberately absent rather than fabricated.
struct fama_french_fit_result {
    double alpha{};
    std::map<std::string, double> betas;
    double r_squared{};
    dataframe fitted_returns;
    dataframe residuals;
};

/// Injected provider boundary for Fama-French data.
///
/// The provider is supplied explicitly on every call; there is no
/// process-global provider or cache state. The returned frame is validated and
/// detached (deep-copied) by :func:`fetch_ff_factors`.
using fama_french_provider = std::function<dataframe(
    std::string_view start, std::string_view end, std::string_view library)>;

/// Fetch Fama-French factors through an explicit provider.
///
/// ``copy`` is retained for source-signature compatibility. Native results
/// always own their buffers; a provider cannot hand ownership of cached data
/// to the caller.
dataframe fetch_ff_factors(std::string_view start, std::string_view end,
                           std::string_view library,
                           const fama_french_provider& provider,
                           bool copy = true);

/// Convenience overload mirroring the keyword-only provider in the source.
dataframe fetch_ff_factors(std::string_view start, std::string_view end,
                           const fama_french_provider& provider,
                           std::string_view library = "french",
                           bool copy = true);

/// Native owning Fama-French model facade.
///
/// The model stores its specification (``3factor``, ``5factor`` or
/// ``4factor_mom``), risk-free rate, fitted alpha and betas. Estimation
/// delegates to the existing native factor-regression kernel. All returned
/// frames own their data.
class FamaFrenchModel {
  public:
    explicit FamaFrenchModel(std::string model_type = "5factor",
                             double risk_free_rate = 0.0);

    [[nodiscard]] const std::string& model_type() const noexcept;
    [[nodiscard]] double risk_free_rate() const noexcept;
    [[nodiscard]] std::span<const std::string_view> factors() const noexcept;
    [[nodiscard]] const std::map<std::string, double>& betas() const noexcept;
    [[nodiscard]] std::optional<double> alpha() const noexcept;

    /// Fit the model. An empty ``returns_column`` selects the first double
    /// column, matching the source's single-asset DataFrame behaviour.
    fama_french_fit_result fit(const dataframe& returns,
                               const dataframe& factor_data,
                               std::string_view returns_column = {});

    [[nodiscard]] dataframe predict(const dataframe& factor_data) const;

    [[nodiscard]] dataframe get_factor_exposures(
        const dataframe& returns, const dataframe& factor_data,
        std::optional<std::size_t> rolling_window = std::nullopt,
        std::string_view returns_column = {}) const;

    [[nodiscard]] std::map<std::string, double> attribution_decomposition(
        const dataframe& returns, const dataframe& factor_data,
        std::string_view returns_column = {});

  private:
    [[nodiscard]] std::string resolve_returns_column(
        const dataframe& returns, std::string_view returns_column) const;

    std::string model_type_;
    double risk_free_rate_{};
    std::vector<std::string_view> factors_;
    std::optional<double> alpha_;
    std::map<std::string, double> betas_;
};

}  // namespace fincore
