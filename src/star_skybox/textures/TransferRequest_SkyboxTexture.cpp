#include "star_skybox/textures/TransferRequest_SkyboxTexture.hpp"

#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/structs/SkyboxTextureSpec.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/maker/buffer/MakeBuffer.hpp"
#include "starlight/maker/texture/MakeTexture.hpp"
#include "starlight/wrappers/Allocator.hpp"

#include <cassert>
#include <string>
#include <utility>
#include <variant>
#include <vector>

namespace starlight::skybox::TransferRequest
{
using starlight::skybox::SkyboxInfo;
using starlight::skybox::SkyboxLayout;

static std::string GetTextureName(const SkyboxInfo &info)
{
    return info.name.empty() ? "skybox" : info.name;
}

SkyboxTexture::SkyboxTexture(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                             std::filesystem::path texturePath)
    : m_skyboxInfo(std::move(skyboxInfo)), m_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex),
      m_texturePath(std::move(texturePath))
{
}

void SkyboxTexture::prep()
{
    if (!m_info.has_value())
        m_info = m_skyboxInfo->get();

    if (!m_transformInfo.has_value())
    {
        io::SkyboxReader<> reader(m_texturePath);
        auto imageInfo = reader.getInfo();
        if (!imageInfo)
            STAR_THROW(imageInfo.error().what());

        m_transformInfo.emplace(io::SkyboxTransformInfo{.imageFileInfo = *imageInfo, .skyboxInfo = *m_info});
    }
}

std::string SkyboxTexture::getDebugDescription() const
{
    return m_info.has_value() ? GetTextureName(*m_info) : "skybox";
}

std::unique_ptr<star::StarBuffers::Buffer> SkyboxTexture::createStagingBuffer(
    star::core::device::StarDevice &device) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before createStagingBuffer()");

    const SkyboxInfo &info = m_transformInfo->skyboxInfo;

    // The staging buffer holds the policy's transformed pixel data, so the policy reports its own output size.
    const PolicyVariant policy = MakePolicy(info.layout);
    const vk::DeviceSize size =
        std::visit([&](const auto &p) { return p.getSize(info, m_transformInfo->imageFileInfo); }, policy);

    star::StarBuffers::Buffer builtBuffer;
    const auto makeResult = star::maker::buffer::MakeBuffer(device)
                                .setCreateInfo(star::Allocator::AllocationBuilder()
                                                   .setFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT |
                                                             VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
                                                   .setUsage(VMA_MEMORY_USAGE_AUTO)
                                                   .build(),
                                               vk::BufferCreateInfo()
                                                   .setSharingMode(vk::SharingMode::eExclusive)
                                                   .setSize(size)
                                                   .setUsage(vk::BufferUsageFlagBits::eTransferSrc),
                                               GetTextureName(m_transformInfo->skyboxInfo) + "_SkyboxTransferSRCBuffer")
                                .setInstanceCount(1)
                                .setInstanceSize(size)
                                .make(&builtBuffer, 1);
    if (!makeResult)
        STAR_THROW("Failed to create staging buffer for skybox: " + makeResult.error().detail);

    return std::make_unique<star::StarBuffers::Buffer>(std::move(builtBuffer));
}

std::unique_ptr<star::StarTextures::Texture> SkyboxTexture::createFinal(
    star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before createFinal()");

    const SkyboxInfo &info = m_transformInfo->skyboxInfo;
    // The sidecar's layout is the single authority for how the image is transferred; build the matching policy on the
    // stack here and in the other phases rather than storing it on the request.
    const PolicyVariant policy = MakePolicy(info.layout);
    const SkyboxTextureSpec spec =
        std::visit([&](const auto &p) { return p.describeTexture(info, m_transformInfo->imageFileInfo); }, policy);

    if (spec.format == vk::Format::eUndefined)
        STAR_THROW("Unsupported skybox layout for texture creation");
    if (spec.layerCount == 0 || spec.extent.width == 0 || spec.extent.height == 0)
        STAR_THROW("Skybox texture spec is incomplete");

    std::vector<uint32_t> indices{this->m_graphicsQueueFamilyIndex};
    indices.reserve(transferQueueFamilyIndex.size() + 1);
    for (const auto &index : transferQueueFamilyIndex)
        indices.push_back(index);

    const vk::ImageViewCreateInfo textureView =
        vk::ImageViewCreateInfo()
            .setViewType(spec.viewType)
            .setFormat(spec.format)
            .setSubresourceRange(vk::ImageSubresourceRange()
                                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                     .setBaseArrayLayer(0)
                                     .setLayerCount(spec.layerCount)
                                     .setBaseMipLevel(0)
                                     .setLevelCount(1));

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

    star::StarTextures::Texture texture;
    const auto makeResult =
        star::maker::texture::MakeTexture(device)
            .setCreateInfo(star::Allocator::AllocationBuilder()
                               .setFlags(VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
                               .setUsage(VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO)
                               .build(),
                           vk::ImageCreateInfo()
                               .setFlags(spec.imageFlags)
                               .setExtent(spec.extent)
                               .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
                               .setImageType(vk::ImageType::e2D)
                               .setMipLevels(1)
                               .setArrayLayers(spec.layerCount)
                               .setTiling(vk::ImageTiling::eOptimal)
                               .setInitialLayout(vk::ImageLayout::eUndefined)
                               .setSamples(vk::SampleCountFlagBits::e1)
                               .setSharingMode(vk::SharingMode::eConcurrent)
                               .setPQueueFamilyIndices(indices.data())
                               .setQueueFamilyIndexCount(static_cast<uint32_t>(indices.size())),
                           GetTextureName(info) + "_SkyboxTexture")
            .setBaseFormat(spec.format)
            .addViewInfo(textureView)
            .setSamplerInfo(samplerInfo)
            .make(&texture, 1);
    if (!makeResult)
        STAR_THROW("Failed to create skybox texture: " + makeResult.error().detail);

    return std::make_unique<star::StarTextures::Texture>(std::move(texture));
}

void SkyboxTexture::copyFromTransferSRCToDST(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                             vk::CommandBuffer &commandBuffer) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before copyFromTransferSRCToDST()");

    const PolicyVariant policy = MakePolicy(m_transformInfo->skyboxInfo.layout);
    std::visit([&](const auto &p) { p.copySkyboxBufferToTexture(srcBuffer, dst, commandBuffer, *m_transformInfo); },
               policy);
}

void SkyboxTexture::writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const
{
    assert(m_info.has_value() && "prep() must resolve the skybox info before writeDataToStageBuffer()");

    const PolicyVariant policy = MakePolicy(m_info->layout);
    auto skyboxData = std::visit([&](const auto &p) { return p.readSkybox(m_texturePath, *m_transformInfo); }, policy);
    if (!skyboxData)
        STAR_THROW(skyboxData.error().what());

    const auto pixels = skyboxData->data();

    void *mapped = nullptr;
    stagingBuffer.map(&mapped);
    stagingBuffer.writeToBuffer(pixels.data(), mapped, static_cast<vk::DeviceSize>(pixels.size_bytes()));
    stagingBuffer.unmap();
}

SkyboxTexture::PolicyVariant SkyboxTexture::MakePolicy(SkyboxLayout layout)
{
    switch (layout)
    {
    case SkyboxLayout::Linear:
        return LinearTexturePolicy{};
    case SkyboxLayout::Equirect:
        return EquirectTexturePolicy{};
    }

    STAR_THROW("Unsupported skybox layout");
}
} // namespace starlight::skybox::TransferRequest
