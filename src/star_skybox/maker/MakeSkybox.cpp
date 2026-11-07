#include "star_skybox/maker/MakeSkybox.hpp"

#include "star_skybox/factory/SkyboxFactory.hpp"
#include "star_skybox/io/SkyboxInfoReader.hpp"
#include "star_skybox/textures/detail/policy/CPUSampling.hpp"
#include "star_skybox/textures/detail/policy/GPUSampling.hpp"

#include <starlight/command/FileIO/ReadFromFile.hpp>
#include <starlight/core/Exceptions.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/helper/queue/QueueHelpers.hpp>
#include <starlight/enums/Enums.hpp>
#include <starlight/job/tasks/IOTask.hpp>

#include <filesystem>
#include <future>
#include <memory>
#include <utility>

namespace star::skybox::maker
{
using star::skybox::SkyboxInfo;

static std::filesystem::path GetInfoPath(const std::string &texture)
{
    std::filesystem::path infoPath{texture};
    infoPath.replace_extension();
    infoPath += ".skyboxinfo.json";

    return infoPath;
}

MakeSkybox::MakeSkybox(star::core::device::DeviceContext &deviceContext, star::skybox::SkyboxContext &skyboxContext)
    : m_deviceContext(deviceContext), m_skyboxContext(skyboxContext)
{
}

MakeSkybox &MakeSkybox::setTexture(std::string texture)
{
    m_texture = std::move(texture);
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

    const std::filesystem::path infoPath = GetInfoPath(m_texture);
    const std::filesystem::path texturePath{m_texture};

    star::skybox::io::SkyboxInfoReader reader;
    auto skyboxInfo = std::make_shared<std::future<SkyboxInfo>>(reader.getFuture());

    auto readPayload = star::job::tasks::io::ReadPayload<star::skybox::io::SkyboxInfoReader>{
        .filePath = infoPath, .readFunction = std::move(reader)};
    auto readTask = star::job::tasks::io::CreateReadTask(std::move(readPayload));
    star::command::file_io::ReadFromFile readCmd{std::move(readTask)};
    m_deviceContext.getCmdBus().submit(readCmd);

    if (m_useGpuSampling)
        return factory::MakeSkybox<star::skybox::TransferRequest::GPUSampling>(
            m_deviceContext, m_skyboxContext, std::move(skyboxInfo), graphicsQueue->getParentQueueFamilyIndex(),
            texturePath);

    return factory::MakeSkybox<star::skybox::TransferRequest::CPUSampling>(
        m_deviceContext, m_skyboxContext, std::move(skyboxInfo), graphicsQueue->getParentQueueFamilyIndex(),
        texturePath);
}
} // namespace star::skybox::maker
