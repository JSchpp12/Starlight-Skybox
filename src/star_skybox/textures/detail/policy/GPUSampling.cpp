#include "star_skybox/textures/detail/policy/GPUSampling.hpp"

#include "star_skybox/struct/EquirectPushConstants.hpp"
#include "star_skybox/textures/detail/SkyboxTextureCreation.hpp"

#include "starlight/core/Exceptions.hpp"
#include "starlight/core/device/DeviceContext.hpp"

#include <array>
#include <cassert>
#include <cstring>
#include <utility>
#include <vector>

namespace star::skybox::TransferRequest
{
namespace
{
constexpr std::uint32_t kCubeFaceCount = 6u;
constexpr std::uint32_t kEquirectWorkgroupSize = 8u;
constexpr float kDegreesToRadians = 3.14159265358979323846f / 180.0f;

vk::ImageSubresourceRange CubeColorRange()
{
    return vk::ImageSubresourceRange()
        .setAspectMask(vk::ImageAspectFlagBits::eColor)
        .setBaseMipLevel(0)
        .setLevelCount(1)
        .setBaseArrayLayer(0)
        .setLayerCount(kCubeFaceCount);
}

/// @brief Returns a conversion set to the shared pool. Called from the release keepAlive on the render thread.
void FreeConversionSet(star::core::device::DeviceContext &context, vk::DescriptorPool pool, vk::DescriptorSet set)
{
    std::vector<vk::DescriptorSet> sets{set};
    context.getDevice().getVulkanDevice().freeDescriptorSets(pool, sets);
}
} // namespace

GPUSampling::GPUSampling(SkyboxComputeResources computeResources) : m_compute(std::move(computeResources))
{
}

std::unique_ptr<star::StarTextures::Texture> GPUSampling::createFinal(
    star::core::device::StarDevice &device, const star::skybox::detail::SkyboxTransferCore &core,
    const std::vector<uint32_t> &transferQueueFamilyIndices, star::StarBuffers::Buffer *stagingBuffer) const
{
    if (!core.usesComputeConversion())
    {
        // A linear source has no conversion dispatch; the transfer queue writes it like the CPU policy. The maker
        // allocated the conversion set before the sidecar layout was known, so hand it to the release to free it once
        // the (empty) definition's gate passes.
        const vk::DescriptorPool descriptorPool = m_compute.descriptorPool;
        const vk::DescriptorSet descriptorSet = m_compute.descriptorSet;
        if (descriptorPool && descriptorSet)
        {
            m_pendingKeepAlive = [descriptorPool, descriptorSet](star::core::device::DeviceContext &context) {
                FreeConversionSet(context, descriptorPool, descriptorSet);
            };
        }

        std::vector<uint32_t> queueFamilyIndices{core.graphicsQueueFamilyIndex()};
        queueFamilyIndices.insert(queueFamilyIndices.end(), transferQueueFamilyIndices.begin(),
                                  transferQueueFamilyIndices.end());

        return detail::CreateSampledTexture(device, core.textureSpec(),
                                            vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eTransferDst,
                                            vk::SharingMode::eConcurrent, queueFamilyIndices, core.debugName());
    }

    assert(m_compute.isValid() && "The compute sampling policy requires resources from the maker");
    assert(stagingBuffer != nullptr && "The conversion dispatch needs the staged equirect pixels");

    // Only the graphics-family dispatch touches the image, so it needs storage usage and no sharing.
    auto textureResult = detail::CreateSampledTexture(
        device, core.textureSpec(), vk::ImageUsageFlagBits::eSampled | vk::ImageUsageFlagBits::eStorage,
        vk::SharingMode::eExclusive, {}, core.debugName());
    auto &texture = *textureResult;

    vk::Device vulkanDevice = device.getVulkanDevice();

    const std::uint32_t faceSize = core.info().textureOptions.faceSize;
    if (faceSize == 0)
        STAR_THROW("Skybox texture options are incomplete: face_size is required");

    // The texture holds both the cube sampling view and the 2D-array view the conversion dispatch writes through.
    const vk::ImageView storageView =
        texture.getImageView().get(texture.getBaseFormat(), vk::ImageViewType::e2DArray);

    // The set was allocated by the maker from the shared pool; fill it with the resources that only exist here. Host
    // access to the set is ordered before recording by the contribution promise. The pool itself is not touched here.
    const vk::DescriptorBufferInfo bufferInfo{stagingBuffer->getVulkanBuffer(), 0, stagingBuffer->getBufferSize()};
    const vk::DescriptorImageInfo imageInfo{vk::Sampler{}, storageView, vk::ImageLayout::eGeneral};

    std::array<vk::WriteDescriptorSet, 2> writes{vk::WriteDescriptorSet()
                                                     .setDstSet(m_compute.descriptorSet)
                                                     .setDstBinding(0)
                                                     .setDescriptorCount(1)
                                                     .setDescriptorType(vk::DescriptorType::eStorageBuffer)
                                                     .setBufferInfo(bufferInfo),
                                                 vk::WriteDescriptorSet()
                                                     .setDstSet(m_compute.descriptorSet)
                                                     .setDstBinding(1)
                                                     .setDescriptorCount(1)
                                                     .setDescriptorType(vk::DescriptorType::eStorageImage)
                                                     .setImageInfo(imageInfo)};
    vulkanDevice.updateDescriptorSets(writes, {});

    // Released once the recorded dispatch has completed: the set back to the shared pool. The storage view belongs
    // to the texture and is destroyed with it.
    const vk::DescriptorPool descriptorPool = m_compute.descriptorPool;
    const vk::DescriptorSet descriptorSet = m_compute.descriptorSet;
    m_pendingKeepAlive = [descriptorPool, descriptorSet](star::core::device::DeviceContext &context) {
        FreeConversionSet(context, descriptorPool, descriptorSet);
    };

    const auto &imageFileInfo = core.transformInfo().imageFileInfo;
    const EquirectPushConstants pushConstants{.sourceWidth = static_cast<std::uint32_t>(imageFileInfo.resolution.x),
                                              .sourceHeight = static_cast<std::uint32_t>(imageFileInfo.resolution.y),
                                              .yawRadians = core.info().yawDegrees * kDegreesToRadians};

    std::vector<std::byte> pushConstantBytes(sizeof(EquirectPushConstants));
    std::memcpy(pushConstantBytes.data(), &pushConstants, sizeof(EquirectPushConstants));

    using Setup = star::setup::RenderResourceSetup;

    // Held until the staging buffer is handed over after submission, then published as one contribution.
    m_pendingSteps.emplace_back(Setup::ImageLayoutChange{.image = texture.getVulkanImage(),
                                                         .from = vk::ImageLayout::eUndefined,
                                                         .to = vk::ImageLayout::eGeneral,
                                                         .range = CubeColorRange()});

    Setup::ComputeDispatch dispatch{};
    dispatch.pipeline = m_compute.pipeline;
    dispatch.descriptorSet = m_compute.descriptorSet;
    dispatch.layout = m_compute.pipelineLayout;
    dispatch.pushConstants = std::move(pushConstantBytes);
    dispatch.dispatchDims = {(faceSize + kEquirectWorkgroupSize - 1u) / kEquirectWorkgroupSize,
                             (faceSize + kEquirectWorkgroupSize - 1u) / kEquirectWorkgroupSize, kCubeFaceCount};
    m_pendingSteps.emplace_back(std::move(dispatch));

    m_pendingSteps.emplace_back(Setup::ImageLayoutChange{.image = texture.getVulkanImage(),
                                                         .from = vk::ImageLayout::eGeneral,
                                                         .to = vk::ImageLayout::eShaderReadOnlyOptimal,
                                                         .range = CubeColorRange()});

    return textureResult;
}

void GPUSampling::publishContribution()
{
    if (!m_compute.contributionPromise.has_value())
        return;

    m_compute.contributionPromise->set_value(
        std::expected<star::setup::RenderResourceSetup::ResourcePrepDefinition, std::string>{
            star::setup::RenderResourceSetup::ResourcePrepDefinition{
                .steps = std::move(m_pendingSteps),
                .release = {.gateOnRecordingSubmission = true,
                            .keepAlive = std::move(m_pendingKeepAlive)}}});
    m_compute.contributionPromise.reset();
}

void GPUSampling::publishFailure(const std::string &message)
{
    if (!m_compute.contributionPromise.has_value())
        return;

    m_compute.contributionPromise->set_value(std::unexpected(message));
    m_compute.contributionPromise.reset();
}
} // namespace star::skybox::TransferRequest
