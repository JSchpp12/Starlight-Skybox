#pragma once

#include "star_skybox/command/SkyboxTypes.hpp"

#include <star_common/IServiceCommandWithReply.hpp>

#include <expected>
#include <string_view>
#include <utility>

namespace star::skybox::command
{
namespace get_skybox_info
{
inline constexpr const char *GetTypeName()
{
    return "skGI";
}
} // namespace get_skybox_info

using GetSkyboxInfoResult = std::expected<SkyboxInfo, SkyServiceException>;

/// @brief Resolve one library entry's sidecar description.
/// @details The handler blocks on the entry's read if it is still pending, swaps the entry to a resolved
/// `SkyboxInfo`, and returns a copy. Unknown handles or read failures return an unexpected.
struct GetSkyboxInfo : public star::common::IServiceCommandWithReply<GetSkyboxInfoResult>
{
    static constexpr std::string_view GetUniqueTypeName()
    {
        return get_skybox_info::GetTypeName();
    }

    GetSkyboxInfo() = default;
    explicit GetSkyboxInfo(star::Handle handle) : handle(std::move(handle)) {}
    GetSkyboxInfo(star::Handle handle, uint16_t type)
        : star::common::IServiceCommandWithReply<GetSkyboxInfoResult>(type), handle(std::move(handle))
    {
    }

    star::Handle handle{};
};
} // namespace star::skybox::command
