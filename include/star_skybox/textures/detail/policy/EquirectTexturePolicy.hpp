#pragma once

#include "StarBuffers/Buffer.hpp"
#include "StarTextures/Texture.hpp"
#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/structs/SkyboxInfo.hpp"
#include "star_skybox/structs/SkyboxTextureSpec.hpp"
#include "starlight/structs/ExrData.hpp"

#include <expected>
#include <filesystem>

#include <vulkan/vulkan.hpp>

namespace starlight::skybox::TransferRequest
{
using starlight::skybox::SkyboxInfo;
/// @brief Skybox transfer policy for equirectangular textures.
/// @details The equirectangular skybox image must be reshaped into the cube texture's six faces, both when decoding
/// pixel data and when copying it to the GPU. Implementations are pending; each op errors until then.
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
    /// @details The transformer resamples the source into six faceSize x faceSize RGBA float faces.
    vk::DeviceSize getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Reads the skybox image file's pixel data reshaped for equirectangular sampling.
    std::expected<star::ExrData, star::io::ImageLoadException> readSkybox(
        const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const;

    /// @brief Copies the staged equirectangular image into the skybox texture's cube faces.
    void copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                   vk::CommandBuffer &commandBuffer,
                                   const io::SkyboxTransformInfo &transformInfo) const;
};
} // namespace starlight::skybox::TransferRequest
