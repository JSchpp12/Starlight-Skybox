#pragma once

#include "star_skybox/detail/SkyboxTransferCore.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"
#include "star_skybox/textures/detail/policy/CPUSampling.hpp"

#include <starlight/core/Exceptions.hpp>
#include <starlight/virtual/TransferRequest_Texture.hpp>

#include <cstdint>
#include <filesystem>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;

/// @brief CPU-path transfer request: transfers the source image into the cube texture via the transfer queue.
/// @details This is the only TransferRequest submitted on the CPU path. It owns the resolved SkyboxInfo. GPU
/// conversion is not part of this request; it lives in the render-thread prep returned by the factory.
class SkyboxTexture : public star::TransferRequest::Texture
{
  public:
    SkyboxTexture(SkyboxInfo skyboxInfo, uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath)
        : m_core(std::move(skyboxInfo), graphicsQueueFamilyIndex, std::move(texturePath), false), m_sampling()
    {
    }

    SkyboxTexture(SkyboxTexture &&) = default;
    SkyboxTexture &operator=(SkyboxTexture &&) = default;
    SkyboxTexture(const SkyboxTexture &) = delete;
    SkyboxTexture &operator=(const SkyboxTexture &) = delete;
    virtual ~SkyboxTexture() = default;

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
        return m_core.createStagingBuffer(device);
    }

    virtual std::unique_ptr<star::StarTextures::Texture> createFinal(
        star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const override
    {
        return m_sampling.createFinal(device, m_core, transferQueueFamilyIndex, nullptr);
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

  private:
    detail::SkyboxTransferCore m_core;
    CPUSampling m_sampling;
};
} // namespace star::skybox::TransferRequest
