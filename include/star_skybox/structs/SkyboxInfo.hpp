#pragma once

#include "star_skybox/structs/SkyboxTextureOptions.hpp"

#include <cstdint>
#include <string>

namespace starlight::skybox
{
/// @brief Projection layout of a skybox image.
enum class SkyboxLayout : std::uint8_t
{
    /// The image is transferred into the target texture as-is; no cube face transform is applied.
    Linear,
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
    /// Label of the skybox: the source image's file name without extension or media-relative path. Used only for
    /// debug names; the transferred image is always the one the code selected through MakeSkybox::setTexture.
    std::string name;
};
} // namespace starlight::skybox