#pragma once

#include "star_skybox/json/SkyboxEnums_JSON.hpp"
#include "star_skybox/struct/SkyboxInfo.hpp"

#include <nlohmann/json.hpp>

namespace star::skybox
{
void to_json(nlohmann::json &j, const SkyboxInfo &info);
void from_json(const nlohmann::json &j, SkyboxInfo &info);
} // namespace star::skybox