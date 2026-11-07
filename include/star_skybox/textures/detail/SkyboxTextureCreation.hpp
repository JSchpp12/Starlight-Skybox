#pragma once

#include "star_skybox/struct/SkyboxTextureSpec.hpp"

#include "StarTextures/Texture.hpp"

#include <memory>
#include <string>
#include <vector>
#include <vulkan/vulkan.hpp>

namespace star::core::device
{
class StarDevice;
}

namespace star::skybox::TransferRequest::detail
{
/// @brief Creates a sampled skybox texture with the given image usage and sharing.
/// @details The texture owns its sampling view and, when `usage` includes eStorage, a 2D-array view the conversion
/// dispatch writes through.
/// @param queueFamilyIndices Concurrent-sharing families; ignored when sharingMode is eExclusive.
[[nodiscard]] std::unique_ptr<star::StarTextures::Texture> CreateSampledTexture(
    star::core::device::StarDevice &device, const SkyboxTextureSpec &spec, vk::ImageUsageFlags usage,
    vk::SharingMode sharingMode, const std::vector<uint32_t> &queueFamilyIndices, const std::string &debugName);
} // namespace star::skybox::TransferRequest::detail
