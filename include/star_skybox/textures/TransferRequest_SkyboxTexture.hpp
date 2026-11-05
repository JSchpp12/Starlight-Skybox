#pragma once

#include <starlight/virtual/TransferRequest_Texture.hpp>

#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/structs/SkyboxInfo.hpp"
#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"
#include "star_skybox/textures/detail/policy/LinearTexturePolicy.hpp"

#include <cstdint>
#include <filesystem>
#include <future>
#include <memory>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace starlight::skybox::TransferRequest
{
using starlight::skybox::SkyboxInfo;
using starlight::skybox::SkyboxLayout;
/// @brief Transfer request creating a skybox texture from a single image file.
/// @details The loaded skybox description's layout (SkyboxInfo::layout) selects the transfer policy, which reshapes
/// the image for cube sampling. The source image is always the file the code selected through
/// MakeSkybox::setTexture; the sidecar's "name" is only a debug label.
class SkyboxTexture : public star::TransferRequest::Texture
{
  public:
    /// @param skyboxInfo Future carrying the loaded skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    /// @param texturePath Path of the source image file the code selected; read for header info and pixel data.
    SkyboxTexture(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                  std::filesystem::path texturePath);

    SkyboxTexture(SkyboxTexture &&) = default;
    SkyboxTexture &operator=(SkyboxTexture &&) = default;
    SkyboxTexture(const SkyboxTexture &) = default;
    SkyboxTexture &operator=(const SkyboxTexture &) = default;
    virtual ~SkyboxTexture() = default;

    /// @brief Resolves the skybox description and the source image's file info.
    /// @throws When the source file's header cannot be parsed.
    virtual void prep() override;

    virtual std::string getDebugDescription() const override;

    virtual std::unique_ptr<star::StarBuffers::Buffer> createStagingBuffer(
        star::core::device::StarDevice &device) const override;

    virtual std::unique_ptr<star::StarTextures::Texture> createFinal(
        star::core::device::StarDevice &device, const std::vector<uint32_t> &transferQueueFamilyIndex) const override;

    virtual void copyFromTransferSRCToDST(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                                          vk::CommandBuffer &commandBuffer) const override;

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
