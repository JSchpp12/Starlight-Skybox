#pragma once

#include <cstdint>

namespace starlight::skybox
{
/// @brief GPU resource a skybox image is transferred into.
enum class SkyboxGPUTextureStorageApproach : std::uint8_t
{
    /// Transfer the skybox into a sampled cube texture.
    SkyCubeTexture,
    /// Transfer the skybox into a storage buffer holding the policy-transformed pixel data.
    SkyBuffer
};
} // namespace starlight::skybox
