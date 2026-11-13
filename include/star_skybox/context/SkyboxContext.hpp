#pragma once

#include "star_skybox/struct/SkyboxConversionPipeline.hpp"

#include <vulkan/vulkan.hpp>

#include <cstdint>
#include <memory>
#include <optional>

namespace star::core::device
{
class DeviceContext;
}

namespace star
{
class StarDescriptorSetLayout;
} // namespace star

namespace star::skybox
{
/// @brief App-held, per-device cache for the equirectangular -> cube conversion resources.
/// @details Replaces the per-make minting of the descriptor set layout, pipeline layout, shader, pipeline and
/// descriptor pool. The context owns the two raw layouts (there is no handle-based layout type in the engine) and the
/// manager owns the shared pool (a DescriptorPool record, freed by the manager at device teardown).
///
/// @warning Lifetime/ordering: construct with a live DeviceContext and destroy BEFORE that DeviceContext. Nothing
/// enforces this; the app layer resets the context during shutdown, after the device is idle. It is deliberately NOT a
/// DeviceContext member, a static, or a starlight service.
///
/// @warning Threading: ensureConversionResources() and make() must be called on the thread that owns the device's
/// command bus (the main/render thread). Per-make requests only receive copies of the handles, so the transfer worker
/// never touches this object. All descriptor-pool operations (allocation in the maker, freeing in the setup's
/// keepAlive) happen on the render thread; the worker only writes the pre-allocated set.
class SkyboxContext
{
  public:
    /// Maximum concurrently-live conversion sets. Each in-flight make() holds one until its recorded dispatch gate
    /// passes; freed sets recycle. This is a concurrency bound, not a leak ceiling.
    static constexpr uint32_t ConversionMaxSets = 8;

    explicit SkyboxContext(star::core::device::DeviceContext &deviceContext);
    ~SkyboxContext();

    SkyboxContext(const SkyboxContext &) = delete;
    SkyboxContext &operator=(const SkyboxContext &) = delete;
    SkyboxContext(SkyboxContext &&) = delete;
    SkyboxContext &operator=(SkyboxContext &&) = delete;

    /// @brief Creates the shared conversion pipeline and pool if they do not exist yet. Idempotent.
    void ensureConversionResources();

    /// @brief Engine-lifetime conversion pipeline. Valid after ensureConversionResources().
    const SkyboxConversionPipeline &conversionPipeline() const;

    /// @brief Raw handle of the shared conversion pool. Valid after ensureConversionResources().
    vk::DescriptorPool conversionPool() const;

  private:
    star::core::device::DeviceContext &m_deviceContext;
    /// Raw handle of the shared pool; the manager owns the record until device teardown.
    vk::DescriptorPool m_pool{};
    /// Owns the raw descriptor set layout; destroyed (cleanupRender) in the destructor.
    std::unique_ptr<star::StarDescriptorSetLayout> m_setLayout{};
    /// Minted lazily on the first GPU-sampling make().
    std::optional<SkyboxConversionPipeline> m_pipeline{};
};
} // namespace star::skybox
