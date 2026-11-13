#include "star_skybox/service/SkyboxFileService.hpp"

#include <star_common/HandleTypeRegistry.hpp>
#include <starlight/command/FileIO/ReadFromFile.hpp>
#include <starlight/core/CommandBus.hpp>
#include <starlight/core/device/DeviceContext.hpp>
#include <starlight/core/logging/LoggingFactory.hpp>
#include <starlight/job/tasks/IOTask.hpp>
#include <starlight/service/InitParameters.hpp>

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <utility>

static std::uint16_t SkyboxFileHandleType()
{
    static const std::uint16_t type = star::common::HandleTypeRegistry::instance().registerType("stSkyboxFile");
    return type;
}

static std::string EntryLabel(const std::filesystem::path &path)
{
    const std::string stem = path.stem().string();
    return stem.empty() ? path.filename().string() : stem;
}

namespace star::skybox::service
{
SkyboxFileService::SkyboxFileService(std::string configuredTexturePath)
    : m_configuredTexturePath(std::move(configuredTexturePath)), m_onGetSkyboxInfo(*this), m_onGetAll(*this),
      m_onGetByPath(*this)
{
}

SkyboxFileService::SkyboxFileService(SkyboxFileService &&other)
    : m_configuredTexturePath(std::move(other.m_configuredTexturePath)),
      m_libraryFuture(std::move(other.m_libraryFuture)), m_entries(std::move(other.m_entries)),
      m_scanError(std::move(other.m_scanError)), m_libraryBuilt(other.m_libraryBuilt), m_cmd(other.m_cmd),
      m_context(other.m_context), m_onGetSkyboxInfo(*this), m_onGetAll(*this), m_onGetByPath(*this)
{
    other.m_cmd = nullptr;
    other.m_context = nullptr;

    if (m_cmd != nullptr)
    {
        other.cleanupListeners(*m_cmd);
        initListeners(*m_cmd);
    }
}

SkyboxFileService &SkyboxFileService::operator=(SkyboxFileService &&other)
{
    if (this == &other)
        return *this;

    if (m_cmd != nullptr)
        cleanupListeners(*m_cmd);

    m_configuredTexturePath = std::move(other.m_configuredTexturePath);
    m_libraryFuture = std::move(other.m_libraryFuture);
    m_entries = std::move(other.m_entries);
    m_scanError = std::move(other.m_scanError);
    m_libraryBuilt = other.m_libraryBuilt;
    m_cmd = other.m_cmd;
    m_context = other.m_context;

    other.m_cmd = nullptr;
    other.m_context = nullptr;

    if (m_cmd != nullptr)
    {
        other.cleanupListeners(*m_cmd);
        initListeners(*m_cmd);
    }

    return *this;
}

void SkyboxFileService::setInitParameters(star::service::InitParameters &params)
{
    m_cmd = &params.commandBus;
    m_context = &params.context;
}

void SkyboxFileService::init()
{
    assert(m_cmd != nullptr && "SkyboxFileService requires a command bus");
    assert(m_context != nullptr && "SkyboxFileService requires a DeviceContext");

    initListeners(*m_cmd);

    auto skyboxParent = std::filesystem::path{m_configuredTexturePath}.parent_path();
    if (skyboxParent.empty())
        skyboxParent = "skybox";

    io::SkyboxLibraryReader reader;
    m_libraryFuture = reader.getFuture();

    auto payload = star::job::tasks::io::ReadPayload<io::SkyboxLibraryReader>{
        .filePath = std::move(skyboxParent), .readFunction = std::move(reader), .kind = star::io::PathKind::directory};
    auto task = star::job::tasks::io::CreateReadTask(std::move(payload));
    star::command::file_io::ReadFromFile command{std::move(task)};
    m_cmd->submit(command);
}

void SkyboxFileService::shutdown()
{
    if (m_cmd != nullptr)
        cleanupListeners(*m_cmd);
}

void SkyboxFileService::initListeners(star::core::CommandBus &bus)
{
    m_onGetSkyboxInfo.init(bus);
    m_onGetAll.init(bus);
    m_onGetByPath.init(bus);
}

void SkyboxFileService::cleanupListeners(star::core::CommandBus &bus)
{
    m_onGetSkyboxInfo.cleanup(bus);
    m_onGetAll.cleanup(bus);
    m_onGetByPath.cleanup(bus);
}

void SkyboxFileService::ensureLibraryBuilt()
{
    if (m_libraryBuilt)
        return;

    m_libraryBuilt = true;

    if (m_context == nullptr)
    {
        m_scanError = "SkyboxFileService is not initialized";
        return;
    }

    std::optional<io::SkyboxLibraryResult> library;
    try
    {
        library = m_libraryFuture.get();
    }
    catch (const std::exception &e)
    {
        m_scanError = e.what();
    }
    catch (...)
    {
        m_scanError = "unknown skybox library scan failure";
    }

    std::size_t expectedEntries = 1;
    if (library.has_value() && library->has_value())
        expectedEntries = library->value().size() + 1;

    m_entries.reserve(expectedEntries);

    if (library.has_value())
    {
        if (*library)
        {
            for (const auto &entry : library->value())
                addEntry(entry.displayName, entry.texturePath);
        }
        else
        {
            m_scanError = library->error();
        }
    }
}

star::Handle SkyboxFileService::addEntry(std::string displayName, std::filesystem::path texturePath)
{
    const std::string textureString = texturePath.string();
    const star::Handle handle{.type = SkyboxFileHandleType(), .id = static_cast<std::uint32_t>(m_entries.size())};

    auto read = io::SkyboxReadResult::request(*m_context, textureString);
    m_entries.emplace(handle, SkyboxEntry{.displayName = std::move(displayName),
                                          .texturePath = std::move(texturePath),
                                          .error = std::nullopt,
                                          .state = std::move(read)});

    return handle;
}

std::optional<star::Handle> SkyboxFileService::findHandleByPath(const std::filesystem::path &texturePath) const
{
    for (const auto &[handle, entry] : m_entries)
    {
        if (m_entries.at(handle).texturePath == texturePath)
            return handle;
    }

    const std::string filename = texturePath.filename().string();
    for (const auto &[handle, entry] : m_entries)
    {
        if (m_entries.at(handle).texturePath.filename().string() == filename)
            return handle;
    }

    return std::nullopt;
}

std::expected<SkyboxInfo, command::SkyServiceException> SkyboxFileService::resolve(SkyboxEntry &entry)
{
    if (entry.error.has_value())
        return std::unexpected(command::SkyServiceException{*entry.error});

    if (auto *info = std::get_if<SkyboxInfo>(&entry.state))
        return *info;

    auto &read = std::get<io::SkyboxReadResult>(entry.state);
    auto result = read.get();
    if (!result)
    {
        entry.error = result.error();
        return std::unexpected(command::SkyServiceException{entry.texturePath.string() + ": " + result.error()});
    }

    entry.state = std::move(*result);
    return std::get<SkyboxInfo>(entry.state);
}

void SkyboxFileService::onGetSkyboxInfo(command::GetSkyboxInfo &cmd)
{
    try
    {
        ensureLibraryBuilt();

        const auto it = m_entries.find(cmd.handle);
        if (it == m_entries.end())
        {
            cmd.getReply().set(
                command::GetSkyboxInfoResult{std::unexpected(command::SkyServiceException{"Unknown skybox handle"})});
            return;
        }

        cmd.getReply().set(resolve(it->second));
    }
    catch (const std::exception &e)
    {
        cmd.getReply().set(command::GetSkyboxInfoResult{std::unexpected(command::SkyServiceException{e.what()})});
    }
    catch (...)
    {
        cmd.getReply().set(
            command::GetSkyboxInfoResult{std::unexpected(command::SkyServiceException{"Unknown skybox error"})});
    }
}

void SkyboxFileService::onGetAllSkyboxRegistrations(command::GetAllSkyboxRegistrations &cmd)
{
    try
    {
        ensureLibraryBuilt();

        command::GetAllSkyboxRegistrationsReply reply;
        reply.scanError = m_scanError;

        for (auto &[handle, entry] : m_entries)
        {
            command::SkyboxRegistration registration{
                .handle = handle, .displayName = entry.displayName, .texturePath = entry.texturePath};

            auto resolved = resolve(entry);
            if (resolved)
                registration.info = *resolved;
            else
                registration.error = resolved.error().what();

            reply.registrations.push_back(std::move(registration));
        }

        // Entries live in a hash map, so iteration order is slot order. Restore the scan's deterministic
        // (display name, then filename) order for callers that present or consume the list positionally.
        std::sort(reply.registrations.begin(), reply.registrations.end(),
                  [](const command::SkyboxRegistration &lhs, const command::SkyboxRegistration &rhs) {
                      if (lhs.displayName != rhs.displayName)
                          return lhs.displayName < rhs.displayName;
                      return lhs.texturePath.filename().string() < rhs.texturePath.filename().string();
                  });

        cmd.getReply().set(std::move(reply));
    }
    catch (const std::exception &e)
    {
        cmd.getReply().set(command::GetAllSkyboxRegistrationsReply{.scanError = e.what()});
    }
    catch (...)
    {
        cmd.getReply().set(
            command::GetAllSkyboxRegistrationsReply{.scanError = "Unknown skybox registrations failure"});
    }
}

void SkyboxFileService::onGetSkyboxInfoByPath(command::GetSkyboxInfoByPath &cmd)
{
    try
    {
        ensureLibraryBuilt();

        const auto handle = findHandleByPath(cmd.texturePath);
        if (!handle.has_value())
        {
            cmd.getReply().set(command::GetSkyboxInfoByPathResult{std::unexpected(
                command::SkyServiceException{"Unknown skybox texture path: " + cmd.texturePath.string()})});
            return;
        }

        cmd.getReply().set(resolve(m_entries.at(*handle)));
    }
    catch (const std::exception &e)
    {
        cmd.getReply().set(command::GetSkyboxInfoByPathResult{std::unexpected(command::SkyServiceException{e.what()})});
    }
    catch (...)
    {
        cmd.getReply().set(command::GetSkyboxInfoByPathResult{
            std::unexpected(command::SkyServiceException{"Unknown skybox by-path error"})});
    }
}
} // namespace star::skybox::service
