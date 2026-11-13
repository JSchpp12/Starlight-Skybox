#include "star_skybox/io/SkyboxReadResult.hpp"

#include "star_skybox/io/SkyboxInfoReader.hpp"

#include <starlight/command/FileIO/ReadFromFile.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/job/tasks/IOTask.hpp>

#include <chrono>
#include <filesystem>
#include <utility>

namespace star::skybox::io
{
static std::filesystem::path GetInfoPath(const std::string &texture)
{
    std::filesystem::path infoPath{texture};
    infoPath.replace_extension();
    infoPath += ".skyboxinfo.json";

    return infoPath;
}

SkyboxReadResult SkyboxReadResult::request(star::core::device::DeviceContext &deviceContext, const std::string &texture)
{
    const std::filesystem::path infoPath = GetInfoPath(texture);

    SkyboxInfoReader reader;
    std::future<SkyboxInfo> future = reader.getFuture();

    auto readPayload =
        star::job::tasks::io::ReadPayload<SkyboxInfoReader>{.filePath = infoPath, .readFunction = std::move(reader)};
    auto readTask = star::job::tasks::io::CreateReadTask(std::move(readPayload));
    star::command::file_io::ReadFromFile readCmd{std::move(readTask)};
    deviceContext.getCmdBus().submit(readCmd);

    return SkyboxReadResult{std::move(future)};
}

bool SkyboxReadResult::isReady() const
{
    return m_future.valid() && m_future.wait_for(std::chrono::milliseconds(0)) == std::future_status::ready;
}

std::expected<SkyboxInfo, std::string> SkyboxReadResult::get()
{
    if (!m_future.valid())
        return std::unexpected("skybox read result has already been consumed");

    try
    {
        return m_future.get();
    }
    catch (const std::exception &e)
    {
        return std::unexpected(e.what());
    }
    catch (...)
    {
        return std::unexpected("unknown skybox read failure");
    }
}
} // namespace star::skybox::io
