#pragma once

#include "StarBuffers/Buffer.hpp"
#include "StarTextures/Texture.hpp"
#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"
#include "star_skybox/struct/SkyboxTextureSpec.hpp"
#include "starlight/structs/ExrData.hpp"

#include <expected>
#include <filesystem>

#include <vulkan/vulkan.hpp>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;
/// @brief Skybox transfer policy for linear textures.
/// @details The skybox image is transferred into the target texture as-is, one image copy. The final texture is a
/// single-layer 2D image matching the source image's resolution; no cube view is created.
class LinearTexturePolicy
{
  public:
    LinearTexturePolicy() = default;
    ~LinearTexturePolicy() = default;
    LinearTexturePolicy(LinearTexturePolicy &&) = default;
    LinearTexturePolicy &operator=(LinearTexturePolicy &&) = default;
    LinearTexturePolicy(const LinearTexturePolicy &) = default;
    LinearTexturePolicy &operator=(const LinearTexturePolicy &) = default;

    /// @brief Describes the target texture: a single-layer 2D image matching the source image's resolution.
    SkyboxTextureSpec describeTexture(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Byte size of the pixel data readSkybox() produces, i.e. the staging buffer size.
    /// @details The reader decodes every source pixel into four float RGBA channels and this policy copies them
    /// through unchanged, so the staging buffer matches the source image's resolution.
    vk::DeviceSize getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Reads the skybox image file's pixel data without applying a layout transform.
    /// @returns The raw EXR pixel data, or the reason the file could not be decoded.
    std::expected<star::ExrData, star::io::ImageLoadException> readSkybox(
        const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const;

    /// @brief Copies the staged skybox image into the skybox texture with a single image copy.
    /// @details Follows TransferRequest's default single-image copy: transition the target to
    /// transfer-optimal, copy the staged pixels, then transition it to shader-read.
    void copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                   vk::CommandBuffer &commandBuffer,
                                   const io::SkyboxTransformInfo &transformInfo) const;
};
} // namespace star::skybox::TransferRequest
