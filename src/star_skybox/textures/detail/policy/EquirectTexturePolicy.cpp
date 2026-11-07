#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"

#include "starlight/core/Exceptions.hpp"

#include <cstdint>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;

constexpr std::uint32_t kCubeFaceCount = 6u;

SkyboxTextureSpec EquirectTexturePolicy::describeTexture(const SkyboxInfo &info,
                                                         const star::ImageFileInfo &imageFileInfo) const
{
    (void)imageFileInfo;

    const std::uint32_t faceSize = info.textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    // The cube image is written by equirect2cube.comp, whose storage-image format qualifier is rgba32f. Keep the
    // conversion format fixed to the one the shader declares rather than deriving it from formatScaling.
    return SkyboxTextureSpec{
        .extent = vk::Extent3D{faceSize, faceSize, 1},
        .format = vk::Format::eR32G32B32A32Sfloat,
        .viewType = vk::ImageViewType::eCube,
        .layerCount = kCubeFaceCount,
        .imageFlags = vk::ImageCreateFlags{vk::ImageCreateFlagBits::eCubeCompatible},
    };
}

vk::DeviceSize EquirectTexturePolicy::getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const
{
    (void)info;

    // The raw equirectangular pixels are staged unchanged, four float RGBA channels per source texel. The GPU
    // conversion dispatch reads them and writes the cube faces.
    return static_cast<vk::DeviceSize>(imageFileInfo.resolution.x) *
           static_cast<vk::DeviceSize>(imageFileInfo.resolution.y) * static_cast<vk::DeviceSize>(4u * sizeof(float));
}

std::expected<star::ExrData, star::io::ImageLoadException> EquirectTexturePolicy::readSkybox(
    const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
{
    // No transform: the conversion dispatch performs the reshape on the GPU.
    io::SkyboxReader<> reader(path, transformInfo);
    return reader.read();
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

    // Intentionally empty. The transfer queue never touches the cube image; the staging buffer is uploaded and
    // converted by a graphics-family compute dispatch recorded by the render phase. The transfer submission still
    // happens (with an empty command buffer), keeping the existing transfer-completion sync chain intact.
}
} // namespace star::skybox::TransferRequest
