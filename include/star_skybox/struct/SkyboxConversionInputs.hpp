#pragma once

#include "star_skybox/struct/EquirectPushConstants.hpp"

#include <string>

namespace star::skybox
{
/// @brief Transfer-worker metadata the render-thread conversion prep needs.
/// @details SkyboxInfo is resolved on the main thread through the SkyboxService; the source buffer is resolved from
/// its Buffer-manager handle. Nothing here stores either.
struct SkyboxConversionInputs
{
    EquirectPushConstants pushConstants{};
    std::string debugName;
};
} // namespace star::skybox
