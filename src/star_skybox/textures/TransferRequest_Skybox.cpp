#include "star_skybox/textures/TransferRequest_Skybox.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/maker/buffer/MakeBuffer.hpp"
#include "starlight/maker/texture/MakeTexture.hpp"
#include "starlight/wrappers/Allocator.hpp"

#include <cassert>
#include <string>
#include <utility>

namespace star_skybox::TransferRequest
{
static constexpr uint32_t CubeFaceCount = 6;

static std::string GetTextureName(const SkyboxInfo &info)
{
    return info.name.empty() ? "skybox" : info.name;
}

static vk::Format SelectFormat(const SkyboxLayout &layout, const SkyboxFormatScaling &formatScaling)
{
    switch (layout)
    {
    case SkyboxLayout::Equirect:
        // Equirectangular skyboxes are sourced from HDR data, so the format is always floating point.
        switch (formatScaling)
        {
        case SkyboxFormatScaling::Full:
            return vk::Format::eR32G32B32A32Sfloat;
        case SkyboxFormatScaling::Half:
            return vk::Format::eR16G16B16A16Sfloat;
        }
        break;
    }

    return vk::Format::eUndefined;
}

template <SkyboxTexturePolicy Policy> void Skybox<Policy>::prep()
{
    if (!m_info.has_value())
        m_info = m_skyboxInfo->get();

    if (m_info->name.empty())
        STAR_THROW("Skybox info is missing name");

    if (!m_transformInfo.has_value())
    {
        io::SkyboxReader<> reader(m_info->name);
        auto imageInfo = reader.getInfo();
        if (!imageInfo)
            STAR_THROW(imageInfo.error().what());

        m_transformInfo.emplace(io::SkyboxTransformInfo{.imageFileInfo = *imageInfo, .skyboxInfo = *m_info});
    }
}

template <SkyboxTexturePolicy Policy> std::string Skybox<Policy>::getDebugDescription() const
{
    return m_info.has_value() ? GetTextureName(*m_info) : "skybox";
}

template <SkyboxTexturePolicy Policy>
std::unique_ptr<star::StarBuffers::Buffer> Skybox<Policy>::createStagingBuffer(
    star::core::device::StarDevice &device) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before createStagingBuffer()");

    const auto &imageFileInfo = m_transformInfo->imageFileInfo;

    // LoadEXR always expands the file to four float channels per pixel, so the staging buffer must match that
    // layout rather than the file's native channel width.
    const vk::DeviceSize size = static_cast<vk::DeviceSize>(imageFileInfo.resolution.x) *
                                static_cast<vk::DeviceSize>(imageFileInfo.resolution.y) *
                                static_cast<vk::DeviceSize>(4u * sizeof(float));

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

template <SkyboxTexturePolicy Policy>
std::unique_ptr<star::StarTextures::Texture> Skybox<Policy>::createFinal(
    star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before createFinal()");

    const SkyboxInfo &info = m_transformInfo->skyboxInfo;
    const SkyboxTextureOptions &options = info.textureOptions;
    if (options.faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    const vk::Format format = SelectFormat(info.layout, options.formatScaling);
    if (format == vk::Format::eUndefined)
        STAR_THROW("Unsupported skybox layout for texture creation");

    std::vector<uint32_t> indices{this->m_graphicsQueueFamilyIndex};
    indices.reserve(transferQueueFamilyIndex.size() + 1);
    for (const auto &index : transferQueueFamilyIndex)
        indices.push_back(index);

    const vk::ImageViewCreateInfo cubeView =
        vk::ImageViewCreateInfo()
            .setViewType(vk::ImageViewType::eCube)
            .setFormat(format)
            .setSubresourceRange(vk::ImageSubresourceRange()
                                     .setAspectMask(vk::ImageAspectFlagBits::eColor)
                                     .setBaseArrayLayer(0)
                                     .setLayerCount(CubeFaceCount)
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
            .setCreateInfo(
                star::Allocator::AllocationBuilder()
                    .setFlags(VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
                    .setUsage(VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO)
                    .build(),
                vk::ImageCreateInfo()
                    .setExtent(vk::Extent3D().setWidth(options.faceSize).setHeight(options.faceSize).setDepth(1))
                    .setUsage(vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst)
                    .setImageType(vk::ImageType::e2D)
                    .setMipLevels(1)
                    .setArrayLayers(CubeFaceCount)
                    .setTiling(vk::ImageTiling::eOptimal)
                    .setInitialLayout(vk::ImageLayout::eUndefined)
                    .setSamples(vk::SampleCountFlagBits::e1)
                    .setSharingMode(vk::SharingMode::eConcurrent)
                    .setPQueueFamilyIndices(indices.data())
                    .setQueueFamilyIndexCount(static_cast<uint32_t>(indices.size())),
                GetTextureName(info) + "_SkyboxTexture")
            .setBaseFormat(format)
            .addViewInfo(cubeView)
            .setSamplerInfo(samplerInfo)
            .make(&texture, 1);
    if (!makeResult)
        STAR_THROW("Failed to create skybox texture: " + makeResult.error().detail);

    return std::make_unique<star::StarTextures::Texture>(std::move(texture));
}

template <SkyboxTexturePolicy Policy>
void Skybox<Policy>::copyFromTransferSRCToDST(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                              vk::CommandBuffer &commandBuffer) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before copyFromTransferSRCToDST()");

    m_policy.copySkyboxBufferToTexture(srcBuffer, dst, commandBuffer, *m_transformInfo);
}

template <SkyboxTexturePolicy Policy>
void Skybox<Policy>::writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const
{
    assert(m_info.has_value() && "prep() must resolve the skybox info before writeDataToStageBuffer()");

    auto skyboxData = m_policy.readSkybox(m_info->name, *m_transformInfo);
    if (!skyboxData)
        STAR_THROW(skyboxData.error().what());

    const auto pixels = skyboxData->data();

    void *mapped = nullptr;
    stagingBuffer.map(&mapped);
    stagingBuffer.writeToBuffer(pixels.data(), mapped, static_cast<vk::DeviceSize>(pixels.size_bytes()));
    stagingBuffer.unmap();
}
} // namespace star_skybox::TransferRequest

// Explicit instantiations: skybox transfers use LinearTexturePolicy for the current data flows; other policies are
// added here once their data flows land.
template class star_skybox::TransferRequest::Skybox<star_skybox::TransferRequest::LinearTexturePolicy>;