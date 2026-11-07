#pragma once

#include "star_skybox/detail/SkyboxTransferCore.hpp"
#include "star_skybox/struct/SkyboxComputeResources.hpp"
#include "star_skybox/textures/detail/policy/CPUSampling.hpp"
#include "star_skybox/textures/detail/policy/GPUSampling.hpp"

#include <starlight/virtual/TransferRequest_Texture.hpp>

#include <concepts>
#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;

/// @brief Transfer request creating a skybox cube texture from a single image file.
/// @details The loaded skybox description's layout selects the transfer policy (linear or equirectangular). The
/// sampling policy selects how the cube image is written and sampled: by the transfer queue (CPUSampling) or by a
/// graphics-family compute dispatch (GPUSampling). The source image is always the file the code selected through
/// MakeSkybox::setTexture; the sidecar's "name" is only a debug label.
/// @tparam SamplingPolicy How the cube image is written and sampled.
template <typename SamplingPolicy> class SkyboxTexture : public star::TransferRequest::Texture
{
  public:
    /// @brief Creates a request whose sampling policy needs no render-side resources (CPU sampling).
    SkyboxTexture(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                  std::filesystem::path texturePath)
        requires std::default_initializable<SamplingPolicy>
        : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath),
                 SamplingPolicy::usesComputeConversion),
          m_sampling()
    {
        takesStagingOwnership = SamplingPolicy::usesComputeConversion;
    }

    /// @brief Creates a request whose sampling policy is constructed from the maker's conversion resources.
    SkyboxTexture(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                  std::filesystem::path texturePath, SkyboxComputeResources computeResources)
        requires std::constructible_from<SamplingPolicy, SkyboxComputeResources>
        : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath),
                 SamplingPolicy::usesComputeConversion),
          m_sampling(std::move(computeResources))
    {
        takesStagingOwnership = SamplingPolicy::usesComputeConversion;
    }

    SkyboxTexture(SkyboxTexture &&) = default;
    SkyboxTexture &operator=(SkyboxTexture &&) = default;
    SkyboxTexture(const SkyboxTexture &) = delete;
    SkyboxTexture &operator=(const SkyboxTexture &) = delete;
    virtual ~SkyboxTexture() = default;

    /// @brief Resolves the skybox description and the source image's file info.
    /// @throws When the source file's header cannot be parsed.
    virtual void prep() override
    {
        m_core.prep();
    }

    virtual std::string getDebugDescription() const override
    {
        return m_core.debugName();
    }

    virtual std::unique_ptr<star::StarBuffers::Buffer> createStagingBuffer(
        star::core::device::StarDevice &device) const override
    {
        auto stagingBuffer = m_core.createStagingBuffer(device);
        // Remember the buffer so the compute descriptor set can bind it; the worker keeps ownership until it hands it
        // back through adoptStagingBuffer().
        m_stagingBuffer = stagingBuffer.get();

        return stagingBuffer;
    }

    virtual std::unique_ptr<star::StarTextures::Texture> createFinal(
        star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const override
    {
        return m_sampling.createFinal(device, m_core, transferQueueFamilyIndex, m_stagingBuffer);
    }

    virtual void copyFromTransferSRCToDST(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                          vk::CommandBuffer &commandBuffer) const override
    {
        m_core.copyToTexture(srcBuffer, dst, commandBuffer);
    }

    virtual void writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const override
    {
        m_core.writeDataToStageBuffer(stagingBuffer);
    }

    virtual void adoptStagingBuffer(star::StarBuffers::Buffer &stagingBuffer) override
    {
        // The manager owns the buffer; the setup releases its handle after the recorded work has completed.
        star::TransferRequest::Texture::adoptStagingBuffer(stagingBuffer);

        // Publish this worker's steps, if the sampling policy produced any.
        m_sampling.publishContribution();
    }

    virtual void publishFailure(const std::string &message) override
    {
        star::TransferRequest::Texture::publishFailure(message);
        m_sampling.publishFailure(message);
    }

  private:
    detail::SkyboxTransferCore m_core;
    SamplingPolicy m_sampling;
    /// Non-owning: the worker owns the staging buffer between createStagingBuffer() and adoption.
    mutable star::StarBuffers::Buffer *m_stagingBuffer = nullptr;
};
} // namespace star::skybox::TransferRequest
