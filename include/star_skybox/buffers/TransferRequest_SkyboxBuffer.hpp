#pragma once

#include <starlight/virtual/TransferRequest_Buffer.hpp>

#include "star_skybox/detail/SkyboxTransferCore.hpp"
#include "star_skybox/struct/SkyboxConversionInputs.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"

#include <cstdint>
#include <expected>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <vector>

namespace star::skybox::TransferRequest
{
using star::skybox::SkyboxInfo;

/// @brief GPU-path transfer request: uploads the equirect source image into a storage buffer.
class SkyboxBuffer : public star::TransferRequest::Buffer
{
  public:
    using ConversionInputs = star::skybox::SkyboxConversionInputs;
    using ConversionResult = std::expected<ConversionInputs, std::string>;

    /// @param skyboxInfo Resolved skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    /// @param texturePath Path of the source image file the code selected.
    /// @param inputsPromise Resolved once the device buffer exists, or on failure.
    SkyboxBuffer(SkyboxInfo skyboxInfo, uint32_t graphicsQueueFamilyIndex, std::filesystem::path texturePath,
                 std::promise<ConversionResult> inputsPromise);
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

    /// @brief Publishes the conversion inputs once the worker has submitted the transfer and published the buffer.
    virtual void onTransferComplete() override;

    /// @brief Rejects the conversion inputs promise when the worker fails the transfer.
    virtual void onTransferFailed(const std::string &message) override;

  private:
    void publishInputs(ConversionInputs inputs);
    void publishFailure(const std::string &reason);

    detail::SkyboxTransferCore m_core;
    /// Filled exactly once from onTransferComplete() (success) or from any failure path.
    std::optional<std::promise<ConversionResult>> m_inputsPromise;
};
} // namespace star::skybox::TransferRequest
