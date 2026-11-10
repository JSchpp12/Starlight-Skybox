#pragma once

#include "star_skybox/detail/SkyboxTransferCore.hpp"
#include "star_skybox/struct/SkyboxComputeResources.hpp"
#include "star_skybox/textures/detail/policy/CPUSampling.hpp"
#include "star_skybox/textures/detail/policy/GPUSampling.hpp"

#include <starlight/core/Exceptions.hpp>
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
template <typename SamplingPolicy> class SkyboxTexture : public star::TransferRequest::Texture
{
  public:
    /// @brief Creates a request whose sampling policy needs no render-side resources (CPU sampling).
    SkyboxTexture(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                  std::filesystem::path texturePath, star::StarBuffers::Buffer *equirectBuffer = nullptr)
        requires std::default_initializable<SamplingPolicy>
        : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath),
                 SamplingPolicy::usesComputeConversion),
          m_sampling(), m_equirectBuffer(equirectBuffer)
    {
    }

    /// @brief Creates a request whose sampling policy is constructed from the maker's conversion resources.
    SkyboxTexture(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                  std::filesystem::path texturePath, SkyboxComputeResources computeResources,
                  star::StarBuffers::Buffer *equirectBuffer = nullptr)
        requires std::constructible_from<SamplingPolicy, SkyboxComputeResources>
        : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath),
                 SamplingPolicy::usesComputeConversion),
          m_sampling(std::move(computeResources)), m_equirectBuffer(equirectBuffer)
    {
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
        // The compute conversion reads the separate equirect buffer; there is nothing to stage for the image.
        if (m_core.usesComputeConversion())
            return nullptr;

        return m_core.createStagingBuffer(device);
    }

    virtual std::unique_ptr<star::StarTextures::Texture> createFinal(
        star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const override
    {
        star::StarBuffers::Buffer *equirectBuffer = nullptr;
        if (m_core.usesComputeConversion())
        {
            if (m_equirectBuffer == nullptr)
                STAR_THROW("Skybox compute conversion requires an equirect buffer request");

            equirectBuffer = m_equirectBuffer;
        }

        return m_sampling.createFinal(device, m_core, transferQueueFamilyIndex, equirectBuffer);
    }

    virtual void copyFromTransferSRCToDST(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                          vk::CommandBuffer &commandBuffer) const override
    {
        // The compute path has no staging buffer and no image copy; its dispatch is recorded by the setup.
        if (!m_core.usesComputeConversion())
            m_core.copyToTexture(srcBuffer, dst, commandBuffer);
    }

    virtual void writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const override
    {
        m_core.writeDataToStageBuffer(stagingBuffer);
    }

    virtual void onTransferComplete() override
    {
        // Publish this worker's steps, if the sampling policy produced any.
        m_sampling.publishContribution();
    }

    virtual void onTransferFailed(const std::string &message) override
    {
        m_sampling.publishFailure(message);
    }

  private:
    detail::SkyboxTransferCore m_core;
    SamplingPolicy m_sampling;
    /// Buffer-manager-owned equirect buffer the conversion dispatch reads. Input only; the request never owns it.
    star::StarBuffers::Buffer *m_equirectBuffer = nullptr;
};
} // namespace star::skybox::TransferRequest
