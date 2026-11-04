cmake_minimum_required(VERSION 3.26)

project(starlight_skybox
    DESCRIPTION "Skybox library for Starlight"
    LANGUAGES CXX
    VERSION 1.0.0
)

find_package(starlight_common CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)

if(NOT TARGET Starlight::starlight)
    # Fallback: when building skybox standalone, use the installed/config package
    find_package(starlight CONFIG REQUIRED)
endif()

set(${PROJECT_NAME}_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/structs/SkyboxInfo.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/structs/SkyboxTextureOptions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/structs/SkyboxTextureSpec.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/json/SkyboxEnums_JSON.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/json/SkyboxInfo_JSON.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/io/SkyboxInfoReader.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/io/SkyboxReader.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/SelectSkyboxTextureFormat.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/LinearTexturePolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/TransferRequest_Skybox.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/maker/MakeSkybox.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/maker/SkyboxFactory.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/primitive/SkyboxFactory.hpp
)

set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/json/SkyboxEnums_JSON.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/json/SkyboxInfo_JSON.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/io/SkyboxInfoReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/TransferRequest_Skybox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/EquirectTexturePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/LinearTexturePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/maker/MakeSkybox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/maker/SkyboxFactory.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/primitive/SkyboxFactory.cpp
)

add_library(${PROJECT_NAME} STATIC
    ${${PROJECT_NAME}_HEADERS}
    ${${PROJECT_NAME}_SOURCES}
)

target_compile_features(${PROJECT_NAME} PUBLIC cxx_std_23)

if (MSVC)
    target_compile_options(${PROJECT_NAME} PRIVATE
        /bigobj
        /MP
        /EHsc
        /Zc:__cplusplus
    )
endif()

target_include_directories(${PROJECT_NAME}
    PUBLIC
        $<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}/include>
        $<INSTALL_INTERFACE:include>
)

target_link_libraries(${PROJECT_NAME}
    PUBLIC
        Starlight::starlight
        starlight_common
        # JSON serialization is used in public headers (SkyboxInfo_JSON.hpp), so it must be PUBLIC.
        nlohmann_json::nlohmann_json
)

add_library(Starlight::skybox ALIAS ${PROJECT_NAME})