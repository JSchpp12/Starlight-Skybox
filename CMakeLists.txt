cmake_minimum_required(VERSION 3.26)

project(starlight_skybox
    DESCRIPTION "Skybox library for Starlight"
    LANGUAGES CXX
    VERSION 1.0.0
)

find_package(starlight_common CONFIG REQUIRED)
find_package(nlohmann_json CONFIG REQUIRED)
find_package(TBB CONFIG REQUIRED)

if(NOT TARGET Starlight::starlight)
    # Fallback: when building skybox standalone, use the installed/config package
    find_package(starlight CONFIG REQUIRED)
endif()

set(${PROJECT_NAME}_HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/struct/SkyboxInfo.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/struct/SkyboxTextureOptions.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/struct/SkyboxTextureSpec.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/struct/SkyboxComputeResources.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/struct/SkyboxConversionPipeline.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/struct/EquirectPushConstants.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/context/SkyboxContext.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/detail/SkyboxTransferCore.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/json/SkyboxEnums_JSON.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/json/SkyboxInfo_JSON.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/io/SkyboxInfoReader.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/io/SkyboxReader.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/SelectSkyboxTextureFormat.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/SkyboxTextureCreation.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/CPUSampling.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/GPUSampling.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/EquirectTexturePolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/EquirectCpuTexturePolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/EquirectTextureTransformer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/detail/policy/LinearTexturePolicy.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/textures/TransferRequest_SkyboxTexture.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/buffers/TransferRequest_SkyboxBuffer.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/maker/MakeSkybox.hpp
    ${CMAKE_CURRENT_SOURCE_DIR}/include/star_skybox/factory/SkyboxFactory.hpp
)

set(${PROJECT_NAME}_SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/json/SkyboxEnums_JSON.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/json/SkyboxInfo_JSON.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/io/SkyboxInfoReader.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/SkyboxTextureCreation.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/CPUSampling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/GPUSampling.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/buffers/TransferRequest_SkyboxBuffer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/EquirectTexturePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/EquirectCpuTexturePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/EquirectTextureTransformer.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/textures/detail/policy/LinearTexturePolicy.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/detail/SkyboxTransferCore.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/maker/MakeSkybox.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/star_skybox/context/SkyboxContext.cpp
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
        # Used by the CPU equirect -> cube transformer.
        TBB::tbb
)

add_library(Starlight::skybox ALIAS ${PROJECT_NAME})