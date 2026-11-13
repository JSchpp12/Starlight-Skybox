#pragma once

#include "star_skybox/command/SkyboxTypes.hpp"

#include <star_common/IServiceCommandWithReply.hpp>

#include <expected>
#include <filesystem>
#include <string_view>
#include <utility>

namespace star::skybox::command
{
namespace get_skybox_info_by_path
{
inline constexpr const char *GetTypeName()
{
    return "skGBP";
}
} // namespace get_skybox_info_by_path

using GetSkyboxInfoByPathResult = std::expected<SkyboxInfo, SkyServiceException>;

/// @brief Resolve the entry whose texture path matches @p texturePath (filename first, then exact path).
/// @details Used by the initial scene load; only the requested entry is awaited.
struct GetSkyboxInfoByPath : public star::common::IServiceCommandWithReply<GetSkyboxInfoByPathResult>
{
    static constexpr std::string_view GetUniqueTypeName()
    {
        return get_skybox_info_by_path::GetTypeName();
    }

    GetSkyboxInfoByPath() = default;
    explicit GetSkyboxInfoByPath(std::filesystem::path texturePath) : texturePath(std::move(texturePath)) {}
    GetSkyboxInfoByPath(std::filesystem::path texturePath, uint16_t type)
        : star::common::IServiceCommandWithReply<GetSkyboxInfoByPathResult>(type), texturePath(std::move(texturePath))
    {
    }

    std::filesystem::path texturePath;
};
} // namespace star::skybox::command
