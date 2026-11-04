#pragma once

#include "star_skybox/structs/SkyboxInfo.hpp"

#include <starlight/structs/ExrData.hpp>
#include <starlight/structs/ImageFileInfo.hpp>
#include <starlight/util/io/ImageLoadException.hpp>
#include <starlight/util/io/ImageParseException.hpp>
#include <starlight/util/io/ReadExr.hpp>

#include <concepts>
#include <expected>
#include <filesystem>
#include <optional>
#include <type_traits>
#include <utility>

namespace starlight::skybox::io
{
using starlight::skybox::SkyboxInfo;
/// @brief Description of a skybox image and the metadata a layout transform needs to reinterpret it.
struct SkyboxTransformInfo
{
    /// Header-level description of the skybox image file.
    star::ImageFileInfo imageFileInfo;
    /// Skybox description handed to the transform when pixel data is read.
    SkyboxInfo skyboxInfo;
};

/// @brief Describes a callable that reshapes raw EXR pixel data for the skybox transfer.
/// @details A transform must be movable so the reader can take ownership of it at construction, and its `operator()`
/// must accept the decoded EXR data (by value or moved), the skybox description and the image file info, and return
/// the reshaped pixel data.
template <typename T>
concept SkyboxTransform = std::movable<T> && requires(T transform, star::ExrData data, const SkyboxInfo &skyboxInfo,
                                                      const star::ImageFileInfo &imageFileInfo) {
    { transform(std::move(data), skyboxInfo, imageFileInfo) } -> std::convertible_to<star::ExrData>;
};

/// @brief Reads skybox pixel data from an EXR file, optionally applying a layout transform.
/// @details Mirrors the ReadExr interface (getInfo/read), but allows callers to cache a file's info at construction
/// time and to install a transform that reshapes the raw EXR pixels into the layout the transfer expects.
/// @tparam Transform Optional callable type; `void` (the default) reads the raw EXR data without transforming it. A
/// provided type must satisfy SkyboxTransform and is moved into the reader at construction.
template <typename Transform = void>
    requires std::is_void_v<Transform> || SkyboxTransform<Transform>
class SkyboxReader
{
  private:
    /// @brief True when a transform type was supplied; controls which constructors are viable.
    static constexpr bool hasTransform = !std::is_void_v<Transform>;

    /// @brief Placeholder stored in place of a transform so the member is well-formed for `Transform = void`.
    struct NoTransform
    {
    };
    using TransformStorage = std::conditional_t<hasTransform, Transform, NoTransform>;

  public:
    /// @param path Path of the EXR skybox image.
    /// @param transformInfo Optional image/skybox info resolved ahead of time; when provided, getInfo() reuses it
    /// instead of re-parsing the file and the transform receives its metadata.
    explicit SkyboxReader(std::filesystem::path path, std::optional<SkyboxTransformInfo> transformInfo = std::nullopt)
        requires std::is_void_v<Transform>
        : m_path(std::move(path)), m_transformInfo(std::move(transformInfo))
    {
    }

    /// @param path Path of the EXR skybox image.
    /// @param transform Transform moved into the reader and applied on every read().
    /// @param transformInfo Optional image/skybox info resolved ahead of time; when provided, getInfo() reuses it
    /// instead of re-parsing the file and the transform receives its metadata.
    /// @details Declaring the parameter as a deduced `T` rather than `Transform` keeps the declaration well-formed
    /// when `Transform` is `void`, where the constraint simply makes this ctor non-viable.
    template <typename T>
        requires std::same_as<std::remove_cvref_t<T>, Transform>
    explicit SkyboxReader(std::filesystem::path path, T &&transform,
                          std::optional<SkyboxTransformInfo> transformInfo = std::nullopt)
        : m_path(std::move(path)), m_transform(std::move(transform)), m_transformInfo(std::move(transformInfo))
    {
    }

    /// @brief Reads the file header and returns the image's size, resolution and channel format.
    /// @returns The image info (the cached one when the ctor was given it), or the reason the header could not be
    /// parsed.
    std::expected<star::ImageFileInfo, star::io::ImageParseException> getInfo() const noexcept
    {
        if (m_transformInfo)
            return m_transformInfo->imageFileInfo;

        return star::io::ReadExr(m_path).getInfo();
    }

    /// @brief Loads the file's pixel data, applying the transform when one was provided.
    /// @returns The (optionally transformed) pixel data, or the reason the file could not be decoded.
    std::expected<star::ExrData, star::io::ImageLoadException> read() const
    {
        auto data = star::io::ReadExr(m_path).read();
        if (!data)
            return data;

        if constexpr (std::is_void_v<Transform>)
        {
            return data;
        }
        else
        {
            if (m_transformInfo)
                return m_transform(std::move(*data), m_transformInfo->skyboxInfo, m_transformInfo->imageFileInfo);

            auto info = star::io::ReadExr(m_path).getInfo();
            if (!info)
                return std::unexpected(star::io::ImageLoadException(info.error().path(), info.error().reason()));

            SkyboxInfo skyboxInfo;
            skyboxInfo.name = m_path.string();
            return m_transform(std::move(*data), skyboxInfo, *info);
        }
    }

  private:
    std::filesystem::path m_path;
    std::optional<SkyboxTransformInfo> m_transformInfo;
    /// Stored by value; read() is const, so the transform's operator() must be const-callable.
    TransformStorage m_transform;
};
} // namespace starlight::skybox::io