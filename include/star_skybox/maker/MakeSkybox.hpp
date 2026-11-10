#pragma once

#include "star_skybox/maker/SkyboxInitializer.hpp"

#include <starlight/setup/RenderResourceSetup.hpp>
#include <starlight/wrappers/graphics/StarSemaphore.hpp>

#include <star_common/Handle.hpp>

#include <optional>
#include <string>

namespace star::core::device
{
class DeviceContext;
}

namespace star::skybox
{
class SkyboxContext;
}

namespace star::skybox::maker
{
/// @brief Result of building a skybox: the GPU resource and the one-shot setup the render phase must record.
struct SkyboxMakeResult
{
    star::Handle resource{};
    /// Image transfer completion semaphore.
    star::StarSemaphore transfer{};
    /// Buffer-manager handle of the equirect source buffer, when a compute conversion is used.
    star::Handle buffer{};
    /// Equirect source-buffer transfer completion semaphore, when a compute conversion is used.
    std::optional<star::StarSemaphore> bufferTransfer{};
    /// One-shot work to submit once the owning render phase exists; empty for CPU sampling.
    star::skybox::SkyboxInitializer initializer{};
};

/// @brief Loads a skybox description from a JSON file and submits the texture transfer request that backs it.
class MakeSkybox
{
  public:
    /// @param deviceContext Device the skybox belongs to.
    /// @param skyboxContext App-held conversion cache; must outlive this maker and the transfer request it submits.
    MakeSkybox(star::core::device::DeviceContext &deviceContext, star::skybox::SkyboxContext &skyboxContext);

    /// @brief Path of the skybox texture file. Must be set before calling make().
    MakeSkybox &setTexture(std::string texture);

    /// @brief Samples the cube texture from a CPU-prepared transfer copy. This is the default.
    MakeSkybox &useCPUSampling();

    /// @brief Samples the cube texture from the GPU compute conversion dispatch.
    MakeSkybox &useGPUSampling();

    /// @brief Queues the info read, submits the transfer request, and returns the resulting resource.
    /// @details The transfer policy is chosen from the sidecar's layout, so the same maker handles every skybox
    /// layout without the caller having to know which one the asset uses. A cube-texture result carries a setup the
    /// render phase must record before sampling the texture.
    [[nodiscard]] SkyboxMakeResult make();

  private:
    star::core::device::DeviceContext &m_deviceContext;
    star::skybox::SkyboxContext &m_skyboxContext;
    std::string m_texture;
    /// False selects the CPU sampling policy; true the GPU compute one.
    bool m_useGpuSampling{false};
};
} // namespace star::skybox::maker
