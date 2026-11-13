#pragma once

#include <cstdint>

namespace star::skybox
{
/// @brief Push constants of equirect2cube.comp. Padded to 16 bytes to match the pipeline layout range.
struct EquirectPushConstants
{
    std::uint32_t sourceWidth = 0;
    std::uint32_t sourceHeight = 0;
    float yawRadians = 0.0f;
    float padding = 0.0f;
};
} // namespace star::skybox
