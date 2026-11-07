#pragma once

#include <starlight/virtual/TransferRequest_Buffer.hpp>

#include "star_skybox/detail/SkyboxTransferCore.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <vector>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;
/// @brief Transfer request uploading a skybox image into a storage buffer.
/// @details Shares its sidecar/image handling with SkyboxTexture, so both requests agree on layout selection and pixel
/// data. The only difference is the final resource: a GPU buffer rather than a sampled texture, so the transfer queue
/// performs a plain buffer copy and no image layout transition happens.
class SkyboxBuffer : public star::TransferRequest::Buffer
{
  public:
    /// @param skyboxInfo Future carrying the loaded skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    /// @param texturePath Path of the source image file the code selected; read for header info and pixel data.
    SkyboxBuffer(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                 std::filesystem::path texturePath);

    SkyboxBuffer(SkyboxBuffer &&) = default;
    SkyboxBuffer &operator=(SkyboxBuffer &&) = default;
    SkyboxBuffer(const SkyboxBuffer &) = delete;
    SkyboxBuffer &operator=(const SkyboxBuffer &) = delete;
    virtual ~SkyboxBuffer() = default;

    /// @brief Resolves the skybox description and the source image's file info.
    /// @throws When the source file's header cannot be parsed.
    virtual void prep() override;

    virtual std::unique_ptr<star::StarBuffers::Buffer> createStagingBuffer(
        star::core::device::StarDevice &device) const override;

    virtual std::unique_ptr<star::StarBuffers::Buffer> createFinal(
        star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const override;

    virtual void writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const override;

  private:
    detail::SkyboxTransferCore m_core;
};
} // namespace star::skybox::TransferRequest
