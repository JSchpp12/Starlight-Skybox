#include "star_skybox/buffers/TransferRequest_SkyboxBuffer.hpp"

#include "star_skybox/io/SkyboxReader.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/maker/buffer/MakeBuffer.hpp"
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

static std::string GetSkyboxName(const SkyboxInfo &info)
{
    return info.name.empty() ? "skybox" : info.name;
}

SkyboxBuffer::SkyboxBuffer(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                           std::filesystem::path texturePath)
    : m_skyboxInfo(std::move(skyboxInfo)), m_graphicsQueueFamilyIndex(graphicsQueueFamilyIndex),
      m_texturePath(std::move(texturePath))
{
}

void SkyboxBuffer::prep()
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

std::unique_ptr<star::StarBuffers::Buffer> SkyboxBuffer::createStagingBuffer(
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
                                               GetSkyboxName(info) + "_SkyboxTransferSRCBuffer")
                                .setInstanceCount(1)
                                .setInstanceSize(size)
                                .make(&builtBuffer, 1);
    if (!makeResult)
        STAR_THROW("Failed to create staging buffer for skybox: " + makeResult.error().detail);

    return std::make_unique<star::StarBuffers::Buffer>(std::move(builtBuffer));
}

std::unique_ptr<star::StarBuffers::Buffer> SkyboxBuffer::createFinal(
    star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before createFinal()");

    const SkyboxInfo &info = m_transformInfo->skyboxInfo;
    // The sidecar's layout is the single authority for how the image is transferred; build the matching policy on the
    // stack here and in the other phases rather than storing it on the request.
    const PolicyVariant policy = MakePolicy(info.layout);
    const vk::DeviceSize size =
        std::visit([&](const auto &p) { return p.getSize(info, m_transformInfo->imageFileInfo); }, policy);

    if (size == 0)
        STAR_THROW("Skybox buffer size is incomplete");

    std::vector<uint32_t> indices{this->m_graphicsQueueFamilyIndex};
    indices.reserve(transferQueueFamilyIndex.size() + 1);
    for (const auto &index : transferQueueFamilyIndex)
        indices.push_back(index);

    star::StarBuffers::Buffer builtBuffer;
    const auto makeResult =
        star::maker::buffer::MakeBuffer(device)
            .setCreateInfo(
                star::Allocator::AllocationBuilder()
                    .setFlags(VmaAllocationCreateFlagBits::VMA_ALLOCATION_CREATE_DEDICATED_MEMORY_BIT)
                    .setUsage(VmaMemoryUsage::VMA_MEMORY_USAGE_AUTO)
                    .build(),
                vk::BufferCreateInfo()
                    .setSharingMode(vk::SharingMode::eConcurrent)
                    .setSize(size)
                    .setUsage(vk::BufferUsageFlagBits::eTransferDst | vk::BufferUsageFlagBits::eStorageBuffer)
                    .setPQueueFamilyIndices(indices.data())
                    .setQueueFamilyIndexCount(static_cast<uint32_t>(indices.size())),
                GetSkyboxName(info) + "_SkyboxBuffer")
            .setInstanceCount(1)
            .setInstanceSize(size)
            .make(&builtBuffer, 1);
    if (!makeResult)
        STAR_THROW("Failed to create skybox buffer: " + makeResult.error().detail);

    return std::make_unique<star::StarBuffers::Buffer>(std::move(builtBuffer));
}

void SkyboxBuffer::writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const
{
    assert(m_transformInfo.has_value() && "prep() must resolve the transform info before writeDataToStageBuffer()");

    const PolicyVariant policy = MakePolicy(m_transformInfo->skyboxInfo.layout);
    auto skyboxData = std::visit([&](const auto &p) { return p.readSkybox(m_texturePath, *m_transformInfo); }, policy);
    if (!skyboxData)
        STAR_THROW(skyboxData.error().what());

    const auto pixels = skyboxData->data();

    void *mapped = nullptr;
    stagingBuffer.map(&mapped);
    stagingBuffer.writeToBuffer(pixels.data(), mapped, static_cast<vk::DeviceSize>(pixels.size_bytes()));
    stagingBuffer.unmap();
}

SkyboxBuffer::PolicyVariant SkyboxBuffer::MakePolicy(SkyboxLayout layout)
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
