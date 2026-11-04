#pragma once

#include <cstdint>

namespace starlight::skybox
{
/// @brief Precision scale applied to the skybox texture's format.
enum class SkyboxFormatScaling : std::uint8_t
{
    Full,
    Half
};

/// @brief Creation options for the texture backing a skybox.
struct SkyboxTextureOptions
{
    /// Edge length of a single cube face, in texels. Used by cube layouts; linear transfers size the texture to the
    /// source image instead.
    std::uint32_t faceSize = 0;
    /// Precision scale applied to the texture format when the texture is created.
    SkyboxFormatScaling formatScaling = SkyboxFormatScaling::Full;
};
} // namespace starlight::skybox