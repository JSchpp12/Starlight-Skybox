#pragma once

#include <starlight/virtual/TransferRequest_Texture.hpp>

#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/structs/SkyboxInfo.hpp"
#include "star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp"
#include "star_skybox/textures/detail/policy/LinearTexturePolicy.hpp"

#include <cstdint>
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
/// @details The loaded skybox description's layout (SkyboxInfo::layout) selects the transfer policy: Linear copies
/// the image as-is into a single-layer 2D texture, Equirect reshapes it into the cube texture's six faces.
class Skybox : public star::TransferRequest::Texture
{
  public:
    /// @param skyboxInfo Future carrying the loaded skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    Skybox(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex);

    Skybox(Skybox &&) = default;
    Skybox &operator=(Skybox &&) = default;
    Skybox(const Skybox &) = default;
    Skybox &operator=(const Skybox &) = default;
    virtual ~Skybox() = default;

    /// @brief Resolves the skybox description and the source image's file info.
    /// @throws When the skybox info is missing a name or the source file's header cannot be parsed.
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
    std::optional<SkyboxInfo> m_info = std::nullopt;
    /// Cached source-file and skybox info resolved by prep(); reused by every phase of the transfer.
    std::optional<io::SkyboxTransformInfo> m_transformInfo = std::nullopt;
};
} // namespace starlight::skybox::TransferRequest
