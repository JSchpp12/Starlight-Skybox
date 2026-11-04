#pragma once

#include "StarBuffers/Buffer.hpp"
#include "StarTextures/Texture.hpp"
#include "star_skybox/io/SkyboxReader.hpp"
#include "starlight/core/Exceptions.hpp"
#include "starlight/structs/ExrData.hpp"

#include <expected>
#include <filesystem>
#include <utility>

#include <vulkan/vulkan.hpp>

namespace star_skybox::TransferRequest
{
/// @brief Describes a policy a skybox transfer uses to interpret and transfer its source image.
/// @details A policy must provide two const operations:
/// - `readSkybox(path, transformInfo)`: decodes the source image's pixel data for the skybox's layout, returning the
///   data or the reason it could not be decoded.
/// - `copySkyboxBufferToTexture(srcBuffer, dst, commandBuffer, transformInfo)`: encodes the commands that copy one
///   staged image into the skybox texture, following TransferRequest's default single-image copy rules.
template <typename T>
concept SkyboxTexturePolicy = requires(const T policy, star::StarBuffers::Buffer buffer,
                                       star::StarTextures::Texture texture, vk::CommandBuffer commandBuffer,
                                       const std::filesystem::path path, const io::SkyboxTransformInfo &transformInfo) {
    {
        policy.readSkybox(path, transformInfo)
    } -> std::same_as<std::expected<star::ExrData, star::io::ImageLoadException>>;
    { policy.copySkyboxBufferToTexture(buffer, texture, commandBuffer, transformInfo) } -> std::same_as<void>;
};

/// @brief Skybox transfer policy for linear textures.
/// @details The skybox image is transferred into the cube texture as-is, one image copy.
class LinearTexturePolicy
{
  public:
    LinearTexturePolicy() = default;
    ~LinearTexturePolicy() = default;

    /// @brief Reads the skybox image file's pixel data without applying a layout transform.
    /// @returns The raw EXR pixel data, or the reason the file could not be decoded.
    std::expected<star::ExrData, star::io::ImageLoadException> readSkybox(
        const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
    {
        io::SkyboxReader<> reader(path, transformInfo);
        return reader.read();
    }

    /// @brief Copies the staged skybox image into the skybox texture with a single image copy.
    /// @details Follows TransferRequest's default single-image copy: transition the target to
    /// transfer-optimal, copy the staged pixels, then transition it to shader-read.
    void copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                   vk::CommandBuffer &commandBuffer, const io::SkyboxTransformInfo &transformInfo) const
    {
        star::StarTextures::Texture::TransitionImageLayout(
            dst, commandBuffer, dst.getBaseFormat(), vk::ImageLayout::eUndefined, vk::ImageLayout::eTransferDstOptimal);

        const auto &imageFileInfo = transformInfo.imageFileInfo;
        vk::BufferImageCopy region{};
        region.bufferOffset = 0;
        region.bufferRowLength = 0;
        region.bufferImageHeight = 0;

        region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
        region.imageSubresource.mipLevel = 0;
        region.imageSubresource.baseArrayLayer = 0;
        region.imageSubresource.layerCount = 1;
        region.imageOffset = vk::Offset3D{};
        region.imageExtent = vk::Extent3D{static_cast<uint32_t>(imageFileInfo.resolution.x),
                                          static_cast<uint32_t>(imageFileInfo.resolution.y), 1};

        commandBuffer.copyBufferToImage(srcBuffer.getVulkanBuffer(), dst.getVulkanImage(),
                                        vk::ImageLayout::eTransferDstOptimal, region);

        star::StarTextures::Texture::TransitionImageLayout(dst, commandBuffer, dst.getBaseFormat(),
                                                           vk::ImageLayout::eTransferDstOptimal,
                                                           vk::ImageLayout::eShaderReadOnlyOptimal);
    }
};

/// @brief Skybox transfer policy for equirectangular textures.
/// @details The equirectangular skybox image must be reshaped into the cube texture's six faces, both when decoding
/// pixel data and when copying it to the GPU. Implementations are pending; each op errors until then.
class EquirectTexturePolicy
{
  public:
    EquirectTexturePolicy() = default;
    ~EquirectTexturePolicy() = default;

    /// @brief Reads the skybox image file's pixel data reshaped for equirectangular sampling.
    std::expected<star::ExrData, star::io::ImageLoadException> readSkybox(
        const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
    {
        return std::unexpected(
            star::io::ImageLoadException(path, "EquirectTexturePolicy::readSkybox is not implemented"));
    }

    /// @brief Copies the staged equirectangular image into the skybox texture's cube faces.
    void copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                   vk::CommandBuffer &commandBuffer, const io::SkyboxTransformInfo &transformInfo) const
    {
        STAR_THROW("EquirectTexturePolicy::copySkyboxBufferToTexture is not implemented");
    }
};
} // namespace star_skybox::TransferRequest