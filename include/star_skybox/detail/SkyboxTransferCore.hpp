#pragma once

#include "star_skybox/io/SkyboxReader.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"
#include "star_skybox/struct/SkyboxTextureSpec.hpp"
#include "star_skybox/textures/detail/policy/EquirectCpuTexturePolicy.hpp"
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
#include <vulkan/vulkan.hpp>

namespace star
{
namespace core::device
{
class StarDevice;
}
namespace StarBuffers
{
class Buffer;
}
namespace StarTextures
{
class Texture;
}
} // namespace star

namespace star::skybox::detail
{
/// @brief Shared implementation of the two skybox transfer requests.
/// @details Both the cube-texture and the storage-buffer request resolve the same sidecar, read the same source image
/// and select the same layout policy; only their destination resource differs. This class owns that shared half so
/// the requests do not duplicate prep/staging/read logic.
class SkyboxTransferCore
{
  public:
    /// @param skyboxInfo Future carrying the loaded skybox description.
    /// @param graphicsQueueFamilyIndex Family index of the engine's graphics queue.
    /// @param texturePath Path of the source image file the code selected.
    /// @param computeEquirect True when equirectangular images are staged raw and reshaped by a compute dispatch; false
    /// when the CPU transformer reshapes them. Linear images ignore it.
    SkyboxTransferCore(std::shared_ptr<std::future<SkyboxInfo>> skyboxInfo, uint32_t graphicsQueueFamilyIndex,
                       std::filesystem::path texturePath, bool computeEquirect);

    SkyboxTransferCore(SkyboxTransferCore &&) = default;
    SkyboxTransferCore &operator=(SkyboxTransferCore &&) = default;
    SkyboxTransferCore(const SkyboxTransferCore &) = delete;
    SkyboxTransferCore &operator=(const SkyboxTransferCore &) = delete;
    ~SkyboxTransferCore() = default;

    /// @brief Resolves the skybox description and the source image's file info.
    /// @throws When the source file's header cannot be parsed.
    void prep();

    const SkyboxInfo &info() const;
    const io::SkyboxTransformInfo &transformInfo() const;

    /// @brief True when the sidecar selects the equirectangular layout.
    bool isEquirect() const;

    /// @brief True when this request stages raw pixels for the graphics compute conversion instead of a CPU transform.
    /// @details Only equirectangular images can use the compute conversion; linear and CPU-prepared equirect images
    /// are copied by the transfer queue.
    bool usesComputeConversion() const;

    /// @brief Family index of the engine's graphics queue, as supplied at construction.
    uint32_t graphicsQueueFamilyIndex() const
    {
        return m_graphicsQueueFamilyIndex;
    }

    /// @brief Byte size of the staging buffer: the raw source pixels (equirect) or the pass-through pixels (linear).
    vk::DeviceSize stagingSize() const;

    /// @brief Usage flags the staging buffer needs for this layout.
    /// @details An equirect staging buffer is read as an SSBO by the conversion dispatch; a linear one is a plain
    /// transfer source.
    vk::BufferUsageFlags stagingUsage() const;

    /// @brief Describes the GPU texture this layout produces.
    SkyboxTextureSpec textureSpec() const;

    /// @brief Creates the host-visible staging buffer.
    std::unique_ptr<star::StarBuffers::Buffer> createStagingBuffer(star::core::device::StarDevice &device) const;

    /// @brief Decodes the source image and writes it into the staging buffer.
    void writeDataToStageBuffer(star::StarBuffers::Buffer &stagingBuffer) const;

    /// @brief Performs the policy's transfer-queue copy into the texture.
    /// @details Equirect performs no copy: its pixels are uploaded by a graphics-side compute dispatch instead.
    void copyToTexture(star::StarBuffers::Buffer &srcBuffer, star::StarTextures::Texture &dst,
                       vk::CommandBuffer &commandBuffer) const;

    /// @brief Debug identity of the source.
    std::string debugName() const;

  private:
    using PolicyVariant = std::variant<TransferRequest::LinearTexturePolicy, TransferRequest::EquirectTexturePolicy,
                                       TransferRequest::EquirectCpuTexturePolicy>;

    static PolicyVariant MakePolicy(SkyboxLayout layout, bool computeEquirect);
    PolicyVariant makePolicy() const;

    std::shared_ptr<std::future<SkyboxInfo>> m_skyboxInfo;
    uint32_t m_graphicsQueueFamilyIndex = 0;
    /// True when equirectangular images are prepared by the compute dispatch instead of the CPU transformer.
    bool m_computeEquirect = false;
    /// Source image file the code selected; the sole authority for which image is transferred.
    std::filesystem::path m_texturePath;
    std::optional<SkyboxInfo> m_info = std::nullopt;
    /// Cached source-file and skybox info resolved by prep(); reused by every phase of the transfer.
    std::optional<io::SkyboxTransformInfo> m_transformInfo = std::nullopt;
};
} // namespace star::skybox::detail
