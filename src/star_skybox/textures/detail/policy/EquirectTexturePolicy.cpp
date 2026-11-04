#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"

#include "star_skybox/textures/detail/SelectSkyboxTextureFormat.hpp"

#include "starlight/core/Exceptions.hpp"

#include <cstdint>

namespace star_skybox::TransferRequest
{
SkyboxTextureSpec EquirectTexturePolicy::describeTexture(const SkyboxInfo &info,
                                                         const star::ImageFileInfo &imageFileInfo) const
{
    (void)imageFileInfo;

    const std::uint32_t faceSize = info.textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    return SkyboxTextureSpec{
        .extent = vk::Extent3D{faceSize, faceSize, 1},
        .format = star_skybox::detail::SelectFormat(info.layout, info.textureOptions.formatScaling),
        .viewType = vk::ImageViewType::eCube,
        .layerCount = 6,
        .imageFlags = vk::ImageCreateFlags{vk::ImageCreateFlagBits::eCubeCompatible},
    };
}

std::expected<star::ExrData, star::io::ImageLoadException> EquirectTexturePolicy::readSkybox(
    const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
{
    (void)transformInfo;

    return std::unexpected(star::io::ImageLoadException(path, "EquirectTexturePolicy::readSkybox is not implemented"));
}

void EquirectTexturePolicy::copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer,
                                                      star::StarTextures::Texture &dst,
                                                      vk::CommandBuffer &commandBuffer,
                                                      const io::SkyboxTransformInfo &transformInfo) const
{
    (void)srcBuffer;
    (void)dst;
    (void)commandBuffer;
    (void)transformInfo;

    STAR_THROW("EquirectTexturePolicy::copySkyboxBufferToTexture is not implemented");
}
} // namespace star_skybox::TransferRequest
