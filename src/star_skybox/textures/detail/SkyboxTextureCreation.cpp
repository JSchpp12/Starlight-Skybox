#include "star_skybox/textures/detail/SkyboxTextureCreation.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/core/device/StarDevice.hpp"
#include "starlight/maker/texture/MakeTexture.hpp"
#include "starlight/wrappers/Allocator.hpp"

#include <utility>

namespace star::skybox::TransferRequest::detail
{
std::unique_ptr<star::StarTextures::Texture> CreateSampledTexture(
    star::core::device::StarDevice &device, const SkyboxTextureSpec &spec, vk::ImageUsageFlags usage,
    vk::SharingMode sharingMode, const std::vector<uint32_t> &queueFamilyIndices, const std::string &debugName)
{
    if (spec.format == vk::Format::eUndefined)
        STAR_THROW("Unsupported skybox layout for texture creation");
    if (spec.layerCount == 0 || spec.extent.width == 0 || spec.extent.height == 0)
        STAR_THROW("Skybox texture spec is incomplete");

    const vk::ImageSubresourceRange viewRange = vk::ImageSubresourceRange()
                                                    .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                                    .setBaseArrayLayer(0)
                                                    .setLayerCount(spec.layerCount)
                                                    .setBaseMipLevel(0)
                                                    .setLevelCount(1);

    const vk::ImageViewCreateInfo textureView =
        vk::ImageViewCreateInfo().setViewType(spec.viewType).setFormat(spec.format).setSubresourceRange(viewRange);

    // A storage image cannot use a cube view, so the compute conversion writes through a 2D-array view over the same
    // layers. Both views belong to the texture, which creates and destroys them together.
    const vk::ImageViewCreateInfo storageViewInfo =
        vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::e2DArray)
            .setFormat(spec.format)
            .setSubresourceRange(viewRange);

    const vk::SamplerCreateInfo samplerInfo = vk::SamplerCreateInfo()
                                                  .setMagFilter(vk::Filter::eLinear)
                                                  .setMinFilter(vk::Filter::eLinear)
                                                  .setAddressModeU(vk::SamplerAddressMode::eClampToEdge)
                                                  .setAddressModeV(vk::SamplerAddressMode::eClampToEdge)
                                                  .setAddressModeW(vk::SamplerAddressMode::eClampToEdge)
                                                  .setBorderColor(vk::BorderColor::eFloatOpaqueBlack)
                                                  .setUnnormalizedCoordinates(VK_FALSE)
                                                  .setCompareEnable(VK_FALSE)
                                                  .setCompareOp(vk::CompareOp::eAlways)
                                                  .setMipmapMode(vk::SamplerMipmapMode::eLinear)
                                                  .setMipLodBias(0.0f)
                                                  .setMinLod(0.0f)
                                                  .setMaxLod(0.0f);

    vk::ImageCreateInfo createInfo = vk::ImageCreateInfo()
                                         .setFlags(spec.imageFlags)
                                         .setExtent(spec.extent)
                                         .setUsage(usage)
                                         .setImageType(vk::ImageType::e2D)
                                         .setMipLevels(1)
                                         .setArrayLayers(spec.layerCount)
                                         .setTiling(vk::ImageTiling::eOptimal)
                                         .setInitialLayout(vk::ImageLayout::eUndefined)
                                         .setSamples(vk::SampleCountFlagBits::e1)
                                         .setSharingMode(sharingMode);

    if (sharingMode == vk::SharingMode::eConcurrent || !queueFamilyIndices.empty())
    {
        createInfo.setQueueFamilyIndexCount(static_cast<uint32_t>(queueFamilyIndices.size()))
            .setPQueueFamilyIndices(queueFamilyIndices.data());
    }

    star::StarTextures::Texture texture;
    auto builder = star::maker::texture::MakeTexture(device);
    builder.setCreateInfo(
               star::Allocator::AllocationBuilder()
                   .setFlags(VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
                   .setUsage(VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO)
                   .build(),
               createInfo, debugName + "_SkyboxTexture")
        .setBaseFormat(spec.format)
        .addViewInfo(textureView)
        .setSamplerInfo(samplerInfo);

    if (usage & vk::ImageUsageFlagBits::eStorage)
        builder.addViewInfo(storageViewInfo);

    const auto makeResult = builder.make(&texture, 1);
    if (!makeResult)
        STAR_THROW("Failed to create skybox texture: " + makeResult.error().detail);

    return std::make_unique<star::StarTextures::Texture>(std::move(texture));
}
} // namespace star::skybox::TransferRequest::detail
