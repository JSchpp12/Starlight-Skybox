#pragma once

#include "star_skybox/structs/SkyboxTextureOptions.hpp"

#include <cstdint>
#include <string>

namespace star_skybox
{
/// @brief Projection layout of a skybox image.
enum class SkyboxLayout : std::uint8_t
{
    Equirect
};

/// @brief Description of a skybox image and how it is oriented.
struct SkyboxInfo
{
    /// Rotation of the skybox about the vertical axis, in degrees.
    float yawDegrees = 0.0f;
    /// Projection layout of the skybox image.
    SkyboxLayout layout = SkyboxLayout::Equirect;
    /// Creation options for the cube texture built from the skybox image.
    SkyboxTextureOptions textureOptions{};
    /// Name of the skybox texture file. Used as the source image and to derive debug names.
    std::string name;
};
} // namespace star_skybox