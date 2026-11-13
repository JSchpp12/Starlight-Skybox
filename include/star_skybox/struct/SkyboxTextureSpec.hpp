#pragma once

#include <cstdint>

#include <vulkan/vulkan.hpp>

namespace star::skybox
{
/// @brief Description of the GPU texture a skybox layout produces.
/// @details Policies fill this in from the skybox description and the source image; the transfer request uses it to
/// create the image, view, and sampler without knowing which layout is in play.
struct SkyboxTextureSpec
{
    /// Image extent of the target texture, in texels.
    vk::Extent3D extent{};
    /// Vulkan format of the target texture.
    vk::Format format = vk::Format::eUndefined;
    /// View type used to sample the texture (cube for skybox textures).
    vk::ImageViewType viewType = vk::ImageViewType::eCube;
    /// Number of array layers the target texture has.
    std::uint32_t layerCount = 1;
    /// Extra image creation flags (e.g. cube-compatible for cube layouts).
    vk::ImageCreateFlags imageFlags{};
};
} // namespace star::skybox
