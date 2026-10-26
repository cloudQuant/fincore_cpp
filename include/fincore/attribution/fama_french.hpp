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

/// Standard three-factor Fama-French definitions, matching the pinned source.
inline constexpr std::array<std::string_view, 3U> FF3_FACTORS{"MKT", "SMB",
                                                              "HML"};
/// Standard five-factor Fama-French definitions, matching the pinned source.
inline constexpr std::array<std::string_view, 5U> FF5_FACTORS{
    "MKT", "SMB", "HML", "RMW", "CMA"};
/// Four-factor Fama-French definitions with the momentum factor.
inline constexpr std::array<std::string_view, 4U> FF4MOM_FACTORS{
    "MKT", "SMB", "HML", "MOM"};

/// Result of the idiosyncratic-risk calculation.
struct idiosyncratic_risk_result {
    std::vector<std::string> assets;          ///< Asset names in column order.
    std::vector<double> specific_volatility;  ///< Specific volatility per asset.
    double market_beta = 0.0;                 ///< Beta applied to every asset.
};

/// Calculates asset-specific volatility from factor data.
///
/// When ``market_beta`` is absent, the beta is fitted by ordinary least
/// squares of the first asset's excess returns on the five Fama-French columns
/// (MKT, SMB, HML, RMW, CMA) and then applied to every asset, matching the
/// pinned routine. Specific volatility is the square root of total variance
/// minus ``beta^2`` times market variance, using risk-free-shifted population
/// variances; a non-positive residual variance yields zero.
///
/// @param returns Frame with at least one double asset-return column.
/// @param factor_data Factor frame with the same row count; it must contain a
///     double ``MKT`` column and, when fitting the beta, all five Fama-French
///     columns.
/// @param market_beta Optional beta applied to every asset; fitted from the
///     first asset when absent.
/// @param risk_free Finite risk-free rate subtracted before variance and beta
///     calculations.
/// @return The asset names, their specific volatilities and the beta used.
/// @throws input_error If the risk-free rate or beta is not finite, the frames
///     are empty or have different row counts, a required factor column is
///     missing, there are too few rows to fit the beta, the beta regression is
///     singular, or no asset column exists.
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
    double alpha{};                      ///< Estimated intercept.
    std::map<std::string, double> betas; ///< Factor name to fitted beta.
    double r_squared{};                  ///< Regression R-squared from the native fit.
    dataframe fitted_returns;            ///< Fitted values on the returns index.
    dataframe residuals;                 ///< Observed minus fitted values.
};

/// Injected provider boundary for Fama-French data.
///
/// The provider is supplied explicitly on every call; there is no
/// process-global provider or cache state. The returned frame is validated and
/// detached (deep-copied) by ``fetch_ff_factors``.
using fama_french_provider = std::function<dataframe(
    std::string_view start, std::string_view end, std::string_view library)>;

/// Fetches Fama-French factors through an explicit provider.
///
/// ``copy`` is retained for source-signature compatibility. Native results
/// always own their buffers; a provider cannot hand ownership of cached data
/// to the caller, so both values of ``copy`` return a detached value.
///
/// @param start Non-empty start date passed to the provider.
/// @param end Non-empty end date passed to the provider.
/// @param library Non-empty factor library name.
/// @param provider Callable returning a non-empty frame with at least one
///     supported column.
/// @param copy Retained for source compatibility; ignored.
/// @return The provider's validated frame, owned by the caller.
/// @throws fincore_error If the provider is not callable, a date or library is
///     empty, or the provider returns an empty or column-less frame.
dataframe fetch_ff_factors(std::string_view start, std::string_view end,
                           std::string_view library,
                           const fama_french_provider& provider,
                           bool copy = true);

/// Fetches Fama-French factors with the library defaulting to ``french``.
///
/// @param start Non-empty start date passed to the provider.
/// @param end Non-empty end date passed to the provider.
/// @param provider Callable returning a non-empty frame with at least one
///     supported column.
/// @param library Non-empty factor library name; defaults to ``french``.
/// @param copy Retained for source compatibility; ignored.
/// @return The provider's validated frame, owned by the caller.
/// @throws fincore_error If the provider is not callable, a date or library is
///     empty, or the provider returns an empty or column-less frame.
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
    /// Constructs a Fama-French model for a supported specification.
    ///
    /// @param model_type One of ``3factor``, ``5factor`` or ``4factor_mom``.
    /// @param risk_free_rate Finite risk-free rate stored with the model.
    /// @throws fincore_error If ``model_type`` is unsupported or the risk-free
    ///     rate is not finite.
    explicit FamaFrenchModel(std::string model_type = "5factor",
                             double risk_free_rate = 0.0);

    /// Returns the configured model type.
    ///
    /// @return One of ``3factor``, ``5factor`` or ``4factor_mom``.
    [[nodiscard]] const std::string& model_type() const noexcept;

    /// Returns the configured risk-free rate.
    ///
    /// @return The risk-free rate supplied at construction.
    [[nodiscard]] double risk_free_rate() const noexcept;

    /// Returns the factor names selected by the model type.
    ///
    /// @return A non-owning view valid for the lifetime of this model.
    [[nodiscard]] std::span<const std::string_view> factors() const noexcept;

    /// Returns the fitted factor betas.
    ///
    /// @return Factor-to-beta mapping, empty before a successful fit.
    [[nodiscard]] const std::map<std::string, double>& betas() const noexcept;

    /// Returns the fitted intercept.
    ///
    /// @return The alpha from the most recent fit, empty before fitting.
    [[nodiscard]] std::optional<double> alpha() const noexcept;

    /// Fits the configured model and stores its alpha and betas.
    ///
    /// An empty ``returns_column`` selects the first double column, matching
    /// the source's single-asset DataFrame behaviour. The fit delegates to the
    /// native least-squares factor regression.
    ///
    /// @param returns Frame containing the dependent return series.
    /// @param factor_data Frame containing the configured factor columns and
    ///     sharing ``returns``' index.
    /// @param returns_column Optional dependent column name.
    /// @return The native fit result and the factor betas for this model.
    /// @throws fincore_error If no double returns column can be selected.
    /// @throws input_error If the regression rejects the inputs or is rank
    ///     deficient.
    fama_french_fit_result fit(const dataframe& returns,
                               const dataframe& factor_data,
                               std::string_view returns_column = {});

    /// Predicts fitted returns from factor data using the stored model.
    ///
    /// The result is ``alpha + sum(beta_i * factor_i)`` for every input row and
    /// is stored under the ``fitted_returns`` column.
    ///
    /// @param factor_data Frame containing all configured factor columns.
    /// @return A frame indexed like ``factor_data`` with one
    ///     ``fitted_returns`` column.
    /// @throws fincore_error If the model has not been fitted (code
    ///     ``fincore.model_not_fit``) or a factor column length does not match
    ///     the frame index.
    [[nodiscard]] dataframe predict(const dataframe& factor_data) const;

    /// Computes alpha and factor betas over expanding or trailing windows.
    ///
    /// ``rolling_window`` selects a trailing window; ``std::nullopt`` or zero
    /// uses an expanding window from the first row. Rows with at most ``K``
    /// observations for ``K`` factors are NaN. This method does not update the
    /// model's stored alpha or betas.
    ///
    /// @param returns Frame containing the dependent return column.
    /// @param factor_data Frame containing the configured factor columns.
    /// @param rolling_window Optional trailing window length.
    /// @param returns_column Optional dependent column name; an empty value
    ///     selects the first double column.
    /// @return A frame indexed like ``returns`` with ``alpha`` followed by one
    ///     column per configured factor.
    /// @throws fincore_error If the frames do not share the same index or the
    ///     dependent column cannot be selected.
    /// @throws input_error If an eligible window regression fails.
    [[nodiscard]] dataframe get_factor_exposures(
        const dataframe& returns, const dataframe& factor_data,
        std::optional<std::size_t> rolling_window = std::nullopt,
        std::string_view returns_column = {}) const;

    /// Fits the model and decomposes its mean return.
    ///
    /// The returned map contains ``alpha``, one ``<factor>_attribution`` entry
    /// per configured factor (``beta * mean(factor)``), ``specific_return``
    /// (the sum of those contributions), ``common_return`` (the mean dependent
    /// return minus the configured risk-free rate) and ``unexplained``
    /// (``common_return - alpha - specific_return``). The fit updates the
    /// model's stored alpha and betas.
    ///
    /// @param returns Frame containing the dependent return series.
    /// @param factor_data Frame containing the configured factor columns.
    /// @param returns_column Optional dependent column name.
    /// @return A map from component name to contribution.
    /// @throws fincore_error If the dependent return column cannot be
    ///     selected.
    /// @throws input_error If the regression rejects the inputs.
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
