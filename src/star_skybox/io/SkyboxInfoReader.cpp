#include "star_skybox/io/SkyboxInfoReader.hpp"

#include "star_skybox/json/SkyboxInfo_JSON.hpp"

#include <starlight/core/Exceptions.hpp>
#include <starlight/core/logging/LoggingFactory.hpp>

#include <nlohmann/json.hpp>

#include <cassert>
#include <exception>
#include <fstream>

namespace star::skybox::io
{
int SkyboxInfoReader::operator()(const std::filesystem::path &filePath)
{
    assert(filePath.extension() == ".json" && "Requested file is not an expected json file");

    try
    {
        m_skyboxInfo.set_value(load(filePath));
    }
    catch (const std::exception &e)
    {
        star::core::logging::warning("Failed to read skybox info '", filePath.string(), "': ", e.what());
        m_skyboxInfo.set_exception(std::current_exception());
    }

    return 0;
}

SkyboxInfo SkyboxInfoReader::load(const std::filesystem::path &filePath) const
{
    if (!std::filesystem::exists(filePath))
        STAR_THROW("Skybox info file does not exist: " + filePath.string());

    std::ifstream stream(filePath);
    if (!stream)
        STAR_THROW("Failed to open skybox info file: " + filePath.string());

    const SkyboxInfo info = nlohmann::json::parse(stream).get<SkyboxInfo>();
    star::core::logging::debug("Parsed skybox info from ", filePath.string(), ": ", nlohmann::json(info).dump());

    return info;
}
} // namespace star::skybox::io