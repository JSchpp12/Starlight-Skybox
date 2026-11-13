#include "star_skybox/maker/MakeSkybox.hpp"

#include "star_skybox/factory/SkyboxFactory.hpp"

#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>
#include <starlight/enums/Enums.hpp>

#include <filesystem>
#include <memory>
#include <utility>

namespace star::skybox::maker
{
MakeSkybox::MakeSkybox(star::core::device::DeviceContext &deviceContext, star::skybox::SkyboxContext &skyboxContext)
    : m_deviceContext(deviceContext), m_skyboxContext(skyboxContext)
{
}

MakeSkybox &MakeSkybox::setTexture(std::string texture)
{
    m_texture = std::move(texture);
    return *this;
}

MakeSkybox &MakeSkybox::setInfo(SkyboxInfo info)
{
    m_resolvedInfo = std::move(info);
    return *this;
}

MakeSkybox &MakeSkybox::useCPUSampling()
{
    m_useGpuSampling = false;
    return *this;
}

MakeSkybox &MakeSkybox::useGPUSampling()
{
    m_useGpuSampling = true;
    return *this;
}

SkyboxMakeResult MakeSkybox::make()
{
    if (m_texture.empty())
        STAR_THROW("Skybox texture must be set before make() is called");

    const auto *graphicsQueue = star::core::helper::GetEngineDefaultQueue(
        m_deviceContext.getEventBus(), m_deviceContext.getGraphicsManagers().queueManager, star::Queue_Type::Tgraphics);
    if (graphicsQueue == nullptr)
        STAR_THROW("Unable to find the engine graphics queue for the skybox transfer");

    if (!m_resolvedInfo.has_value())
        STAR_THROW("Skybox info must be resolved before make() is called");

    const std::filesystem::path texturePath{m_texture};
    const uint32_t graphicsQueueFamilyIndex = graphicsQueue->getParentQueueFamilyIndex();

    SkyboxInfo info = std::move(*m_resolvedInfo);
    m_resolvedInfo.reset();

    if (m_useGpuSampling)
        return factory::MakeSkyboxGPU(m_deviceContext, m_skyboxContext, std::move(info), graphicsQueueFamilyIndex,
                                      texturePath);

    return factory::MakeSkyboxCPU(m_deviceContext, std::move(info), graphicsQueueFamilyIndex, texturePath);
}
} // namespace star::skybox::maker
