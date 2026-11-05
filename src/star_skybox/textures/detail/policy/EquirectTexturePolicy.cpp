#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"

#include "star_skybox/textures/detail/SelectSkyboxTextureFormat.hpp"
#include "star_skybox/textures/detail/policy/EquirectTextureTransformer.hpp"

#include "starlight/core/Exceptions.hpp"

#include <cstdint>

namespace starlight::skybox::TransferRequest
{
using starlight::skybox::SkyboxInfo;

constexpr std::uint32_t kCubeFaceCount = 6u;

SkyboxTextureSpec EquirectTexturePolicy::describeTexture(const SkyboxInfo &info,
                                                         const star::ImageFileInfo &imageFileInfo) const
{
    (void)imageFileInfo;

    const std::uint32_t faceSize = info.textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    return SkyboxTextureSpec{
        .extent = vk::Extent3D{faceSize, faceSize, 1},
        .format = starlight::skybox::detail::SelectFormat(info.layout, info.textureOptions.formatScaling),
        .viewType = vk::ImageViewType::eCube,
        .layerCount = kCubeFaceCount,
        .imageFlags = vk::ImageCreateFlags{vk::ImageCreateFlagBits::eCubeCompatible},
    };
}

vk::DeviceSize EquirectTexturePolicy::getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const
{
    (void)imageFileInfo;

    const std::uint32_t faceSize = info.textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    // Six faceSize x faceSize faces, each holding four float RGBA channels.
    return static_cast<vk::DeviceSize>(faceSize) * static_cast<vk::DeviceSize>(faceSize) * 6u *
           static_cast<vk::DeviceSize>(4u * sizeof(float));
}

std::expected<star::ExrData, star::io::ImageLoadException> EquirectTexturePolicy::readSkybox(
    const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
{
    // The reader decodes the EXR file and hands its RGBA float pixels to the transformer, which reshapes them into
    // the cube faces the target texture expects.
    io::SkyboxReader<EquirectTextureTransformer> reader(path, EquirectTextureTransformer{}, transformInfo);
    return reader.read();
}

static vk::ImageMemoryBarrier2 MakeImageBarrier(const star::StarTextures::Texture &dst) noexcept
{
    // A cube image stores one face per array layer, so the range covers all six layers.
    return vk::ImageMemoryBarrier2()
        .setImage(dst.getVulkanImage())
        .setSrcQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setDstQueueFamilyIndex(vk::QueueFamilyIgnored)
        .setSubresourceRange(vk::ImageSubresourceRange()
                                 .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                 .setBaseMipLevel(0)
                                 .setLevelCount(dst.getMipmapLevels())
                                 .setBaseArrayLayer(0)
                                 .setLayerCount(kCubeFaceCount));
}

static void ApplyToTransfer(vk::CommandBuffer cmdBuff, const star::StarTextures::Texture &dst) noexcept
{
    const vk::ImageMemoryBarrier2 toTransferDst = MakeImageBarrier(dst)
                                                      .setSrcStageMask(vk::PipelineStageFlagBits2::eTopOfPipe)
                                                      .setDstStageMask(vk::PipelineStageFlagBits2::eTransfer)
                                                      .setSrcAccessMask(vk::AccessFlagBits2::eNone)
                                                      .setOldLayout(vk::ImageLayout::eUndefined)
                                                      .setNewLayout(vk::ImageLayout::eTransferDstOptimal)
                                                      .setDstAccessMask(vk::AccessFlagBits2::eTransferWrite);

    cmdBuff.pipelineBarrier2(
        vk::DependencyInfo().setPImageMemoryBarriers(&toTransferDst).setImageMemoryBarrierCount(1));
}

static void ApplyToShaderRead(vk::CommandBuffer cmdBuff, const star::StarTextures::Texture &dst) noexcept
{
    const vk::ImageMemoryBarrier2 toShaderRead = MakeImageBarrier(dst)
                                                     .setSrcStageMask(vk::PipelineStageFlagBits2::eTransfer)
                                                     .setDstStageMask(vk::PipelineStageFlagBits2::eFragmentShader)
                                                     .setOldLayout(vk::ImageLayout::eTransferDstOptimal)
                                                     .setNewLayout(vk::ImageLayout::eShaderReadOnlyOptimal)
                                                     .setSrcAccessMask(vk::AccessFlagBits2::eTransferWrite)
                                                     .setDstAccessMask(vk::AccessFlagBits2::eShaderRead);
    cmdBuff.pipelineBarrier2(vk::DependencyInfo().setPImageMemoryBarriers(&toShaderRead).setImageMemoryBarrierCount(1));
}

void EquirectTexturePolicy::copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer,
                                                      star::StarTextures::Texture &dst,
                                                      vk::CommandBuffer &commandBuffer,
                                                      const io::SkyboxTransformInfo &transformInfo) const
{
    const std::uint32_t faceSize = transformInfo.skyboxInfo.textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    ApplyToTransfer(commandBuffer, dst);

    // The transformer writes the six faces back-to-back, each faceSize x faceSize, so the buffer holds the layers
    // tightly packed in layer order.
    vk::BufferImageCopy region{};
    region.bufferOffset = 0;
    region.bufferRowLength = faceSize;
    region.bufferImageHeight = faceSize;

    region.imageSubresource.aspectMask = vk::ImageAspectFlagBits::eColor;
    region.imageSubresource.mipLevel = 0;
    region.imageSubresource.baseArrayLayer = 0;
    region.imageSubresource.layerCount = kCubeFaceCount;
    region.imageOffset = vk::Offset3D{};
    region.imageExtent = vk::Extent3D{faceSize, faceSize, 1};

    commandBuffer.copyBufferToImage(srcBuffer.getVulkanBuffer(), dst.getVulkanImage(),
                                    vk::ImageLayout::eTransferDstOptimal, region);

    ApplyToShaderRead(commandBuffer, dst);
}
} // namespace starlight::skybox::TransferRequest
