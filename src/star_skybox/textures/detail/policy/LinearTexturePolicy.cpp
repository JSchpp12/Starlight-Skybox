#include "star_skybox/textures/detail/policy/LinearTexturePolicy.hpp"

#include "star_skybox/textures/detail/SelectSkyboxTextureFormat.hpp"

#include <cstdint>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;
SkyboxTextureSpec LinearTexturePolicy::describeTexture(const SkyboxInfo &info,
                                                       const star::ImageFileInfo &imageFileInfo) const
{
    return SkyboxTextureSpec{
        .extent = vk::Extent3D{static_cast<std::uint32_t>(imageFileInfo.resolution.x),
                               static_cast<std::uint32_t>(imageFileInfo.resolution.y), 1},
        .format = star::skybox::detail::SelectFormat(info.layout, info.textureOptions.formatScaling),
        .viewType = vk::ImageViewType::e2D,
        .layerCount = 1,
        .imageFlags = vk::ImageCreateFlags{},
    };
}

vk::DeviceSize LinearTexturePolicy::getSize(const SkyboxInfo &info, const star::ImageFileInfo &imageFileInfo) const
{
    (void)info;

    // The linear policy copies the decoded EXR pixels through unchanged, so the staging buffer holds one pixel per
    // source texel expanded to four float RGBA channels.
    return static_cast<vk::DeviceSize>(imageFileInfo.resolution.x) *
           static_cast<vk::DeviceSize>(imageFileInfo.resolution.y) * static_cast<vk::DeviceSize>(4u * sizeof(float));
}

std::expected<star::ExrData, star::io::ImageLoadException> LinearTexturePolicy::readSkybox(
    const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
{
    // No transform is installed: the raw EXR pixels are already the layout the target texture expects.
    io::SkyboxReader<> reader(path, transformInfo);
    return reader.read();
}

void LinearTexturePolicy::copySkyboxBufferToTexture(star::StarBuffers::Buffer &srcBuffer,
                                                    star::StarTextures::Texture &dst, vk::CommandBuffer &commandBuffer,
                                                    const io::SkyboxTransformInfo &transformInfo) const
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
} // namespace star::skybox::TransferRequest
