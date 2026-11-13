#pragma once

#include "star_skybox/struct/SkyboxInfo.hpp"

#include <starlight/core/transfer/ReadinessCondition.hpp>
#include <starlight/setup/RenderResourceSetup.hpp>

#include <star_common/Handle.hpp>

#include <optional>
#include <string>
#include <vector>

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
/// @brief Result of building a skybox: the GPU resource, the transfer waits it needs, and the one-shot setup the
/// render phase must record.
struct SkyboxMakeResult
{
    /// @brief Manager handle of the cube texture. Always initialized on return. The GPU path creates the texture
    /// synchronously (it is a GPU-only resource), so its creation failures throw from make().
    star::Handle resource{};
    /// @brief One-shot work the owning pass must record before sampling the texture.
    star::setup::RenderResourceSetup::PrepRequest prep{};
    /// @brief Transfer-completion waits the owning pass must observe before sampling the texture or running the
    /// conversion dispatch that reads the equirect source buffer.
    std::optional<star::setup::TransferWait> wait{};
    /// @brief Readiness conditions the material service must satisfy before committing the resource.
    std::optional<star::ReadinessCondition> condition{};
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

    /// @brief Uses an already-resolved sidecar description.
    /// @details Required before make(). The SkyboxFileService owns all sidecar reads and resolution.
    MakeSkybox &setInfo(SkyboxInfo info);

    /// @brief Samples the cube texture from a CPU-prepared transfer copy. This is the default.
    MakeSkybox &useCPUSampling();

    /// @brief Samples the cube texture from the GPU compute conversion dispatch.
    MakeSkybox &useGPUSampling();

    /// @brief Submits the transfer request and returns the resulting resource.
    /// @details Requires a resolved description via setInfo(). The transfer policy is chosen from its layout, so the
    /// same maker handles every skybox layout without the caller having to know which one the asset uses. A
    /// cube-texture result carries a setup the render phase must record before sampling the texture.
    [[nodiscard]] SkyboxMakeResult make();

  private:
    star::core::device::DeviceContext &m_deviceContext;
    star::skybox::SkyboxContext &m_skyboxContext;
    std::string m_texture;
    /// Already-resolved sidecar description; required before make().
    std::optional<SkyboxInfo> m_resolvedInfo;
    /// False selects the CPU sampling policy; true the GPU compute one.
    bool m_useGpuSampling{false};
};
} // namespace star::skybox::maker
