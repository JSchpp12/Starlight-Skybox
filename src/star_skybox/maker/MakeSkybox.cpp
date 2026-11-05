#include "star_skybox/maker/MakeSkybox.hpp"

#include "star_skybox/io/SkyboxInfoReader.hpp"
#include "star_skybox/maker/SkyboxFactory.hpp"

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

namespace starlight::skybox::maker
{
using starlight::skybox::SkyboxInfo;
static std::filesystem::path GetInfoPath(const std::string &texture)
{
    std::filesystem::path infoPath{texture};
    infoPath.replace_extension();
    infoPath += ".skyboxinfo.json";

    return infoPath;
}

MakeSkybox::MakeSkybox(star::core::device::DeviceContext &deviceContext) : m_deviceContext(deviceContext)
{
}

MakeSkybox &MakeSkybox::setTexture(std::string texture)
{
    m_texture = std::move(texture);
    return *this;
}

MakeSkybox &MakeSkybox::setTextureHandling(SkyboxGPUTextureStorageApproach approach)
{
    m_textureHandling = approach;
    return *this;
}

star::Handle MakeSkybox::make()
{
    if (m_texture.empty())
        STAR_THROW("Skybox texture must be set before make() is called");

    const auto *graphicsQueue = star::core::helper::GetEngineDefaultQueue(
        m_deviceContext.getEventBus(), m_deviceContext.getGraphicsManagers().queueManager, star::Queue_Type::Tgraphics);
    if (graphicsQueue == nullptr)
        STAR_THROW("Unable to find the engine graphics queue for the skybox transfer");

    const std::filesystem::path infoPath = GetInfoPath(m_texture);
    const std::filesystem::path texturePath{m_texture};

    starlight::skybox::io::SkyboxInfoReader reader;
    auto skyboxInfo = std::make_shared<std::future<SkyboxInfo>>(reader.getFuture());

    auto readPayload = star::job::tasks::io::ReadPayload<starlight::skybox::io::SkyboxInfoReader>{
        .filePath = infoPath, .readFunction = std::move(reader)};
    auto readTask = star::job::tasks::io::CreateReadTask(std::move(readPayload));
    star::command::file_io::ReadFromFile readCmd{std::move(readTask)};
    m_deviceContext.getCmdBus().submit(readCmd);

    return SkyboxFactory::MakeSkyboxGPUData(m_deviceContext.getDeviceID(), std::move(skyboxInfo),
                                            graphicsQueue->getParentQueueFamilyIndex(), texturePath, m_textureHandling);
}
} // namespace starlight::skybox::maker