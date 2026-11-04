#pragma once

#include "star_skybox/structs/SkyboxInfo.hpp"

#include <filesystem>
#include <future>

namespace starlight::skybox::io
{
using starlight::skybox::SkyboxInfo;
/// @brief Reads a SkyboxInfo description from a JSON file.
class SkyboxInfoReader
{
  public:
    /// @brief Loads the file and stores the result for retrieval via getFuture().
    /// @returns 0, so the reader can be used as an IOTask read function.
    int operator()(const std::filesystem::path &filePath);

    /// @brief Future resolving to the loaded info once operator() has run.
    std::future<SkyboxInfo> getFuture()
    {
        return m_skyboxInfo.get_future();
    }

  private:
    SkyboxInfo load(const std::filesystem::path &filePath) const;

    std::promise<SkyboxInfo> m_skyboxInfo;
};
} // namespace starlight::skybox::io