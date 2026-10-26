#include <fincore/attribution/fama_french_patch.hpp>

#include <type_traits>

namespace fincore {

static_assert(std::is_same_v<FamaFrenchModelPatch, FamaFrenchModel>,
              "FamaFrenchModelPatch is a source-compatibility alias for the "
              "canonical native Fama-French model");

}  // namespace fincore
