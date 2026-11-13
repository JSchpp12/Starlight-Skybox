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
/// @brief Skybox transfer policy for equirectangular textures.
/// @details The raw equirectangular pixels are staged unchanged; a graphics-side compute dispatch reshapes them into
/// the cube texture's six faces. No transfer-queue copy is performed, so the transfer queue never touches the image.
class EquirectTexturePolicy
{
  public:
    EquirectTexturePolicy() = default;
    ~EquirectTexturePolicy() = default;
    EquirectTexturePolicy(EquirectTexturePolicy &&) = default;
    EquirectTexturePolicy &operator=(EquirectTexturePolicy &&) = default;
    EquirectTexturePolicy(const EquirectTexturePolicy &) = default;
    EquirectTexturePolicy &operator=(const EquirectTexturePolicy &) = default;

    /// @brief Describes the target texture: a square cube-compatible image with six face layers.
    SkyboxTextureSpec describeTexture(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Byte size of the pixel data readSkybox() produces, i.e. the staging buffer size.
    /// @details The source pixels are staged unchanged, so the buffer matches the source image's resolution.
    vk::DeviceSize getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Reads the skybox image file's raw pixel data.
    /// @details No CPU reshape happens; the conversion dispatch maps these pixels onto the cube faces on the GPU.
    std::expected<star::ExrData, star::io::ImageLoadException> readSkybox(
        const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const;

    /// @brief No-op: the transfer queue never touches an equirect skybox image.
    /// @details The cube image is written by the graphics-family conversion dispatch, so the transfer command buffer
    /// stays empty. That empty submission is still what keeps the transfer-completion sync chain alive.
    void copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                   vk::CommandBuffer &commandBuffer,
                                   const io::SkyboxTransformInfo &transformInfo) const;
};
} // namespace star::skybox::TransferRequest
