#pragma once

#include <starlight/virtual/TransferRequest_Buffer.hpp>

#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/structs/SkyboxInfo.hpp"
#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"
#include "star_skybox/textures/detail/policy/LinearTexturePolicy.hpp"

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <variant>
#include <vector>

namespace starlight::skybox::TransferRequest
{
using starlight::skybox::SkyboxInfo;
using starlight::skybox::SkyboxLayout;
/// @brief Transfer request uploading a skybox image into a storage buffer.
/// @details Mirrors SkyboxTexture: the loaded skybox description's layout (SkyboxInfo::layout) selects the transfer
/// policy, which reshapes the image's pixel data. The only difference is the final resource, which is a GPU buffer
/// rather than a sampled texture, so no image layout transition is performed.
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
    SkyboxBuffer(const SkyboxBuffer &) = default;
    SkyboxBuffer &operator=(const SkyboxBuffer &) = default;
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
    using PolicyVariant = std::variant<LinearTexturePolicy, EquirectTexturePolicy>;

    /// @brief Builds the transfer policy matching the skybox's layout.
    static PolicyVariant MakePolicy(SkyboxLayout layout);

    std::shared_ptr<std::future<SkyboxInfo>> m_skyboxInfo;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    /// Source image file the code selected; the sole authority for which image is transferred.
    std::filesystem::path m_texturePath;
    std::optional<SkyboxInfo> m_info = std::nullopt;
    /// Cached source-file and skybox info resolved by prep(); reused by every phase of the transfer.
    std::optional<io::SkyboxTransformInfo> m_transformInfo = std::nullopt;
};
} // namespace starlight::skybox::TransferRequest
