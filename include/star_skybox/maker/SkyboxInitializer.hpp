#pragma once

#include <starlight/command/command_order/AddRenderResourceSetup.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/setup/RenderResourceSetup.hpp>

#include <star_common/Handle.hpp>

#include <utility>
#include <vector>

namespace star::skybox
{
/// @brief The one-shot work a skybox make produced, submitted once its owning pass exists.
/// @details The initializer owns the prepare callback and the record-only definition. The application
/// calls submitMe() after the render phase that owns the skybox has been built, so the pass's
/// CompleteRequest exists.
class SkyboxInitializer
{
  public:
    SkyboxInitializer() = default;
    explicit SkyboxInitializer(star::setup::RenderResourceSetup::PrepRequest prep) : m_prep(std::move(prep))
    {
    }

    /// @brief Runs the (blocking) prepare and submits the prepared definitions to the owning pass request.
    void submitMe(star::core::device::DeviceContext &context, star::Handle owningCommandBuffer)
    {
        if (m_prep.prepare)
            m_prep.prepare(context, m_prep.definition);

        if (!m_prep.definition.hasWork())
            return;

        std::vector<star::setup::RenderResourceSetup::ResourcePrepDefinition> definitions;
        definitions.push_back(std::move(m_prep.definition));

        star::command_order::AddRenderResourceSetup cmd{std::move(owningCommandBuffer), std::move(definitions)};
        context.getCmdBus().submit(cmd);
    }

  private:
    star::setup::RenderResourceSetup::PrepRequest m_prep{};
};
} // namespace star::skybox
