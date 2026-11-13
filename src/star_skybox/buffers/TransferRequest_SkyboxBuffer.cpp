#include "star_skybox/buffers/TransferRequest_SkyboxBuffer.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/maker/buffer/MakeBuffer.hpp"
#include "starlight/wrappers/Allocator.hpp"

#include <utility>
#include <vector>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;

SkyboxBuffer::SkyboxBuffer(SkyboxInfo skyboxInfo, uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath,
                           std::promise<ConversionResult> inputsPromise)
    // computeEquirect=true stages the raw equirect pixels for the GPU conversion dispatch.
    : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath), true),
      m_inputsPromise(std::move(inputsPromise))
{
}

void SkyboxBuffer::prep()
{
    try
    {
        m_core.prep();
    }
    catch (const std::exception &e)
    {
        publishFailure(e.what());
        throw;
    }
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

void SkyboxBuffer::onTransferComplete()
{
    if (!m_core.hasConversionData())
    {
        // Defensive: never leave the render thread's future unresolved.
        publishFailure("Skybox buffer transfer completed without conversion inputs");
        return;
    }

    publishInputs(m_core.conversionInputs());
}

void SkyboxBuffer::onTransferFailed(const std::string &message)
{
    publishFailure(message);
}

void SkyboxBuffer::publishInputs(ConversionInputs inputs)
{
    if (!m_inputsPromise.has_value())
        return;

    m_inputsPromise->set_value(std::move(inputs));
    m_inputsPromise.reset();
}

void SkyboxBuffer::publishFailure(const std::string &reason)
{
    if (!m_inputsPromise.has_value())
        return;

    m_inputsPromise->set_value(std::unexpected(reason));
    m_inputsPromise.reset();
}
} // namespace star::skybox::TransferRequest
