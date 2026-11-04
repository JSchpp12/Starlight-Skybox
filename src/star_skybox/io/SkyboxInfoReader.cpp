#include "star_skybox/io/SkyboxInfoReader.hpp"

#include "star_skybox/json/SkyboxInfo_JSON.hpp"

#include <starlight/core/Exceptions.hpp>

#include <nlohmann/json.hpp>

#include <cassert>
#include <fstream>

namespace starlight::skybox::io
{
int SkyboxInfoReader::operator()(const std::filesystem::path &filePath)
{
    assert(filePath.extension() == ".json" && "Requested file is not an expected json file");

    m_skyboxInfo.set_value(load(filePath));
    return 0;
}

SkyboxInfo SkyboxInfoReader::load(const std::filesystem::path &filePath) const
{
    if (!std::filesystem::exists(filePath))
        STAR_THROW("Skybox info file does not exist: " + filePath.string());

    std::ifstream stream(filePath);
    if (!stream)
        STAR_THROW("Failed to open skybox info file: " + filePath.string());

    return nlohmann::json::parse(stream).get<SkyboxInfo>();
}
} // namespace starlight::skybox::io