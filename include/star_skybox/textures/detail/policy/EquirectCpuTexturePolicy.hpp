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
/// @brief Skybox transfer policy for equirectangular images converted into cube faces on the CPU.
/// @details The source pixels are resampled into the six cube faces while decoding, so the transfer queue only has to
/// copy the tightly packed faces into the cube image. No compute pipeline or render-side setup is involved.
class EquirectCpuTexturePolicy
{
  public:
    EquirectCpuTexturePolicy() = default;
    ~EquirectCpuTexturePolicy() = default;
    EquirectCpuTexturePolicy(EquirectCpuTexturePolicy &&) = default;
    EquirectCpuTexturePolicy &operator=(EquirectCpuTexturePolicy &&) = default;
    EquirectCpuTexturePolicy(const EquirectCpuTexturePolicy &) = default;
    EquirectCpuTexturePolicy &operator=(const EquirectCpuTexturePolicy &) = default;

    /// @brief Describes the target texture: a square cube-compatible image with six face layers.
    SkyboxTextureSpec describeTexture(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Byte size of the pixel data readSkybox() produces, i.e. the staging buffer size.
    /// @details The transformer resamples the source into six faceSize x faceSize RGBA float faces.
    vk::DeviceSize getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const;

    /// @brief Reads the skybox image file's pixel data reshaped for cube sampling.
    std::expected<star::ExrData, star::io::ImageLoadException> readSkybox(
        const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const;

    /// @brief Copies the staged cube faces into the skybox texture's six layers.
    void copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                   vk::CommandBuffer &commandBuffer,
                                   const io::SkyboxTransformInfo &transformInfo) const;
};
} // namespace star::skybox::TransferRequest
