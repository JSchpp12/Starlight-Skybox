#include "star_skybox/json/SkyboxInfo_JSON.hpp"

#include <stdexcept>

namespace starlight::skybox
{
void to_json(nlohmann::json &j, const SkyboxInfo &info)
{
    j = nlohmann::json{
        {"yaw_degrees", info.yawDegrees},
        {"layout", info.layout},
        {"face_size", info.textureOptions.faceSize},
        {"format_scaling", info.textureOptions.formatScaling},
        {"name", info.name},
    };
}

void from_json(const nlohmann::json &j, SkyboxInfo &info)
{
    if (j.contains("yaw_degrees"))
        info.yawDegrees = j.at("yaw_degrees").get<float>();

    if (j.contains("layout"))
        info.layout = j.at("layout").get<SkyboxLayout>();

    if (j.contains("face_size"))
        info.textureOptions.faceSize = j.at("face_size").get<std::uint32_t>();

    if (j.contains("format_scaling"))
        info.textureOptions.formatScaling = j.at("format_scaling").get<SkyboxFormatScaling>();

    if (j.contains("name"))
        info.name = j.at("name").get<std::string>();
}
} // namespace starlight::skybox