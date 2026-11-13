#pragma once

#include "star_skybox/command/GetAllSkyboxRegistrations.hpp"
#include "star_skybox/command/GetSkyboxInfo.hpp"
#include "star_skybox/command/GetSkyboxInfoByPath.hpp"
#include "star_skybox/io/SkyboxLibraryReader.hpp"
#include "star_skybox/io/SkyboxReadResult.hpp"

#include <absl/container/flat_hash_map.h>
#include <star_common/Handle.hpp>
#include <starlight/policy/command/ListenFor.hpp>

#include <cstdint>
#include <expected>
#include <filesystem>
#include <future>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace star::core
{
class CommandBus;
class WorkerPool;
} // namespace star::core

namespace star::core::device
{
class DeviceContext;
} // namespace star::core::device

namespace star::job
{
class TaskManager;
} // namespace star::job

namespace star::service
{
struct InitParameters;
} // namespace star::service

namespace star::skybox::service
{
template <typename T>
using ListenForGetSkyboxInfo =
    star::policy::command::ListenFor<T, command::GetSkyboxInfo, command::get_skybox_info::GetTypeName,
                                     &T::onGetSkyboxInfo>;

template <typename T>
using ListenForGetAllSkyboxRegistrations =
    star::policy::command::ListenFor<T, command::GetAllSkyboxRegistrations,
                                     command::get_all_skybox_registrations::GetTypeName,
                                     &T::onGetAllSkyboxRegistrations>;

template <typename T>
using ListenForGetSkyboxInfoByPath =
    star::policy::command::ListenFor<T, command::GetSkyboxInfoByPath,
                                     command::get_skybox_info_by_path::GetTypeName, &T::onGetSkyboxInfoByPath>;

/// @brief Owns the skybox library scan and every entry's sidecar read.
/// @details Entries are keyed by stable handles. Each entry starts as an in-flight `SkyboxReadResult`; a command that
/// requests that entry resolves it (blocking) and replaces it with a `SkyboxInfo`. The map is built once on the first
/// command and is main-thread-only.
class SkyboxFileService
{
  public:
    /// @param configuredTexturePath The skybox texture path from the app config. The library directory is derived
    /// from its parent; the path is also surfaced as a synthetic entry when the scan does not contain it.
    explicit SkyboxFileService(std::string configuredTexturePath);
    SkyboxFileService(const SkyboxFileService &) = delete;
    SkyboxFileService &operator=(const SkyboxFileService &) = delete;
    SkyboxFileService(SkyboxFileService &&other);
    SkyboxFileService &operator=(SkyboxFileService &&other);
    ~SkyboxFileService() = default;

    void setInitParameters(star::service::InitParameters &params);

    void negotiateWorkers(star::core::WorkerPool &, star::job::TaskManager &) {}

    void init();

    void shutdown();

    void onGetSkyboxInfo(command::GetSkyboxInfo &cmd);
    void onGetAllSkyboxRegistrations(command::GetAllSkyboxRegistrations &cmd);
    void onGetSkyboxInfoByPath(command::GetSkyboxInfoByPath &cmd);

  private:
    struct SkyboxEntry
    {
        std::string displayName;
        std::filesystem::path texturePath;
        std::optional<std::string> error;
        std::variant<io::SkyboxReadResult, SkyboxInfo> state;
    };

    void ensureLibraryBuilt();
    std::expected<SkyboxInfo, command::SkyServiceException> resolve(SkyboxEntry &entry);
    star::Handle addEntry(std::string displayName, std::filesystem::path texturePath);
    std::optional<star::Handle> findHandleByPath(const std::filesystem::path &texturePath) const;
    void initListeners(star::core::CommandBus &bus);
    void cleanupListeners(star::core::CommandBus &bus);

    std::string m_configuredTexturePath;
    std::future<io::SkyboxLibraryResult> m_libraryFuture;
    absl::flat_hash_map<star::Handle, SkyboxEntry, star::HandleHash> m_entries;
    std::optional<std::string> m_scanError;
    bool m_libraryBuilt{false};
    star::core::CommandBus *m_cmd{nullptr};
    star::core::device::DeviceContext *m_context{nullptr};
    ListenForGetSkyboxInfo<SkyboxFileService> m_onGetSkyboxInfo;
    ListenForGetAllSkyboxRegistrations<SkyboxFileService> m_onGetAll;
    ListenForGetSkyboxInfoByPath<SkyboxFileService> m_onGetByPath;
};
} // namespace star::skybox::service
