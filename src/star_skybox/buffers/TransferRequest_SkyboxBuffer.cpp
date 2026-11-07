#include "star_skybox/buffers/TransferRequest_SkyboxBuffer.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/maker/buffer/MakeBuffer.hpp"
#include "starlight/wrappers/Allocator.hpp"

#include <utility>
#include <vector>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;

SkyboxBuffer::SkyboxBuffer(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                           std::filesystem::path texturePath)
    // A storage buffer carries the CPU-reshaped cube faces, matching the historical buffer path.
    : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath), false)
{
}

void SkyboxBuffer::prep()
{
    m_core.prep();
}

std::unique_ptr<star::StarBuffers::Buffer> SkyboxBuffer::createStagingBuffer(
    star::core::device::StarDevice &device) const
{
    return m_core.createStagingBuffer(device);
}

std::unique_ptr<star::StarBuffers::Buffer> SkyboxBuffer::createFinal(
    star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const
{
    const vk::DeviceSize size = m_core.stagingSize();
    if (size == 0)
        STAR_THROW("Skybox buffer size is incomplete");

    std::vector<uint32_t> indices{m_core.graphicsQueueFamilyIndex()};
    indices.insert(indices.end(), transferQueueFamilyIndex.begin(), transferQueueFamilyIndex.end());

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
                m_core.debugName() + "_SkyboxBuffer")
            .setInstanceCount(1)
            .setInstanceSize(size)
            .make(&builtBuffer, 1);
    if (!makeResult)
        STAR_THROW("Failed to create skybox buffer: " + makeResult.error().detail);

    return std::make_unique<star::StarBuffers::Buffer>(std::move(builtBuffer));
}

void SkyboxBuffer::writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const
{
    m_core.writeDataToStageBuffer(stagingBuffer);
}
} // namespace star::skybox::TransferRequest
