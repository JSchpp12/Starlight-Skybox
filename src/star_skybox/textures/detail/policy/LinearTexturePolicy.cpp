#include "star_skybox/textures/detail/policy/LinearTexturePolicy.hpp"

#include "star_skybox/textures/detail/SelectSkyboxTextureFormat.hpp"

#include <cstdint>

namespace star_skybox::TransferRequest
{
SkyboxTextureSpec LinearTexturePolicy::describeTexture(const SkyboxInfo &info,
                                                       const star::ImageFileInfo &imageFileInfo) const
{
    return SkyboxTextureSpec{
        .extent = vk::Extent3D{static_cast<std::uint32_t>(imageFileInfo.resolution.x),
                               static_cast<std::uint32_t>(imageFileInfo.resolution.y), 1},
        .format = star_skybox::detail::SelectFormat(info.layout, info.textureOptions.formatScaling),
        .viewType = vk::ImageViewType::e2D,
        .layerCount = 1,
        .imageFlags = vk::ImageCreateFlags{},
    };
}

std::expected<star::ExrData, star::io::ImageLoadException> LinearTexturePolicy::readSkybox(
    const std::filesystem::path &path, const io::SkyboxTransformInfo &transformInfo) const
{
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
} // namespace star_skybox::TransferRequest
