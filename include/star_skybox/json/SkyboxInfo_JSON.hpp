#pragma once

#include "star_skybox/json/SkyboxEnums_JSON.hpp"
#include "star_skybox/structs/SkyboxInfo.hpp"

#include <nlohmann/json.hpp>

namespace star_skybox
{
void to_json(nlohmann::json &j, const SkyboxInfo &info);
void from_json(const nlohmann::json &j, SkyboxInfo &info);
} // namespace star_skybox