#pragma once

#include <fincore/attribution/fama_french.hpp>

namespace fincore {

/// Source-compatible compatibility facade for
/// ``fincore.attribution.fama_french_patch.FamaFrenchModelPatch``.
///
/// The patch module documented the same model specification fields as the
/// canonical Fama-French model (``model_type``, ``risk_free_rate``, selected
/// ``factors`` and fitted ``betas``). It adds no independent computation in
/// the pinned source, so the native alias deliberately maps to the owning
/// :class:`FamaFrenchModel` rather than duplicating estimation state.
using FamaFrenchModelPatch = FamaFrenchModel;

}  // namespace fincore
