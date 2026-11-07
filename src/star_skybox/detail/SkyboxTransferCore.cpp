#include "star_skybox/detail/SkyboxTransferCore.hpp"

#include "star_skybox/io/SkyboxReader.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/maker/buffer/MakeBuffer.hpp"
#include "starlight/wrappers/Allocator.hpp"

#include <cassert>
#include <utility>

namespace star::skybox::detail
{
using star::skybox::SkyboxInfo;
using star::skybox::SkyboxLayout;

SkyboxTransferCore::SkyboxTransferCore(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo,
                                       uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath,
                                       bool computeEquirect)
    : m_skyboxInfo(std::move(skyboxInfo)), m_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex),
      m_computeEquirect(computeEquirect), m_texturePath(std::move(texturePath))
{
}

void SkyboxTransferCore::prep()
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

const SkyboxInfo &SkyboxTransferCore::info() const
{
    assert(m_info.has_value() && "prep() must resolve the skybox info first");
    return *m_info;
}

const io::SkyboxTransformInfo &SkyboxTransferCore::transformInfo() const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info first");
    return *m_transformInfo;
}

bool SkyboxTransferCore::isEquirect() const
{
    return info().layout == SkyboxLayout::Equirect;
}

bool SkyboxTransferCore::usesComputeConversion() const
{
    return isEquirect() && m_computeEquirect;
}

vk::DeviceSize SkyboxTransferCore::stagingSize() const
{
    const PolicyVariant policy = makePolicy();
    return std::visit([&](const auto &p) { return p.getSize(info(), transformInfo().imageFileInfo); }, policy);
}

vk::BufferUsageFlags SkyboxTransferCore::stagingUsage() const
{
    // Compute-converted pixels are read by the dispatch as a storage buffer; everything else is a transfer source.
    return usesComputeConversion() ? vk::BufferUsageFlags{vk::BufferUsageFlagBits::eStorageBuffer}
                                   : vk::BufferUsageFlags{vk::BufferUsageFlagBits::eTransferSrc};
}

SkyboxTextureSpec SkyboxTransferCore::textureSpec() const
{
    const PolicyVariant policy = makePolicy();
    return std::visit([&](const auto &p) { return p.describeTexture(info(), transformInfo().imageFileInfo); }, policy);
}

std::unique_ptr<star::StarBuffers::Buffer> SkyboxTransferCore::createStagingBuffer(
    star::core::device::StarDevice &device) const
{
    const vk::DeviceSize size = stagingSize();
    const vk::BufferUsageFlags usage = stagingUsage();

    star::StarBuffers::Buffer builtBuffer;
    const auto makeResult =
        star::maker::buffer::MakeBuffer(device)
            .setCreateInfo(
                star::Allocator::AllocationBuilder()
                    .setFlags(VMA_ALLOCATION_CREATE_MAPPED_BIT | VMA_ALLOCATION_CREATE_HOST_ACCESS_SEQUENTIAL_WRITE_BIT)
                    .setUsage(VMA_MEMORY_USAGE_AUTO)
                    .build(),
                vk::BufferCreateInfo().setSharingMode(vk::SharingMode::eExclusive).setSize(size).setUsage(usage),
                debugName() + "_SkyboxTransferSRCBuffer")
            .setInstanceCount(1)
            .setInstanceSize(size)
            .make(&builtBuffer, 1);
    if (!makeResult)
        STAR_THROW("Failed to create staging buffer for skybox: " + makeResult.error().detail);

    return std::make_unique<star::StarBuffers::Buffer>(std::move(builtBuffer));
}

void SkyboxTransferCore::writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const
{
    const PolicyVariant policy = makePolicy();
    auto skyboxData = std::visit([&](const auto &p) { return p.readSkybox(m_texturePath, transformInfo()); }, policy);
    if (!skyboxData)
        STAR_THROW(skyboxData.error().what());

    const auto pixels = skyboxData->data();

    void *mapped = nullptr;
    stagingBuffer.map(&mapped);
    stagingBuffer.writeToBuffer(pixels.data(), mapped, static_cast<vk::DeviceSize>(pixels.size_bytes()));
    stagingBuffer.unmap();
}

void SkyboxTransferCore::copyToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                       vk::CommandBuffer &commandBuffer) const
{
    const PolicyVariant policy = makePolicy();
    std::visit([&](const auto &p) { p.copySkyboxBufferToTexture(srcBuffer, dst, commandBuffer, transformInfo()); },
               policy);
}

std::string SkyboxTransferCore::debugName() const
{
    if (!m_info.has_value() || m_info->name.empty())
        return "skybox";

    return m_info->name;
}

SkyboxTransferCore::PolicyVariant SkyboxTransferCore::makePolicy() const
{
    return MakePolicy(info().layout, m_computeEquirect);
}

SkyboxTransferCore::PolicyVariant SkyboxTransferCore::MakePolicy(SkyboxLayout layout, bool computeEquirect)
{
    switch (layout)
    {
    case SkyboxLayout::Linear:
        return TransferRequest::LinearTexturePolicy{};
    case SkyboxLayout::Equirect:
        return computeEquirect ? PolicyVariant{TransferRequest::EquirectTexturePolicy{}}
                               : PolicyVariant{TransferRequest::EquirectCpuTexturePolicy{}};
    }

    STAR_THROW("Unsupported skybox layout");
}
} // namespace star::skybox::detail
