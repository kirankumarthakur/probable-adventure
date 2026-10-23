cmake_minimum_required(VERSION 4.0)
project(probable_adventure VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

# Build dependencies as static libraries.
set(BUILD_SHARED_LIBS OFF CACHE BOOL "" FORCE)

# Only enable the SFML modules this project currently uses.
set(SFML_BUILD_AUDIO OFF CACHE BOOL "" FORCE)
set(SFML_BUILD_NETWORK OFF CACHE BOOL "" FORCE)

include(FetchContent)

find_package(Threads REQUIRED)

# -------------------------------------------------------------------
# Third-party Dependencies
# -------------------------------------------------------------------

FetchContent_Declare(
    entt
    GIT_REPOSITORY https://github.com/skypjack/entt.git
    GIT_TAG v3.13.0
    SYSTEM
)

FetchContent_Declare(
    glm
    GIT_REPOSITORY https://github.com/g-truc/glm.git
    GIT_TAG 1.0.1
    SYSTEM
)

FetchContent_Declare(
    spdlog
    GIT_REPOSITORY https://github.com/gabime/spdlog.git
    GIT_TAG v1.14.1
    SYSTEM
)
set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    GIT_TAG v1.3.2
    SYSTEM
)

FetchContent_Declare(
    sfml
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    SYSTEM
)

FetchContent_Declare(
    eigen
    GIT_REPOSITORY https://gitlab.com/libeigen/eigen.git
    GIT_TAG 3.4.0
    SYSTEM
)

FetchContent_MakeAvailable(
    entt
    glm
    spdlog
    cereal
    sfml
    eigen
)

set(SIMULATION_SRC
    src/Arena.cpp
    src/MLP.cpp
    src/ModalPolicy.cpp
    src/SpatialGrid2D.cpp
    src/Serialization.cpp
    src/Trainer.cpp
    src/SlimeFactory.cpp
    src/AiSystems.cpp
    src/ActionSystems.cpp
    src/CombatSystems.cpp
    src/MovementSystems.cpp
    src/SpawnSystems.cpp
    src/Config.cpp
    src/CheckpointManager.cpp
    src/TextureManager.cpp
    src/RenderSystem.cpp
    src/ui/ClayRenderer.cpp
    src/ui/App.cpp
)

file(GLOB_RECURSE INC_FILES CONFIGURE_DEPENDS
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.hpp"
    "${CMAKE_CURRENT_SOURCE_DIR}/include/*.h"
)

# -------------------------------------------------------------------
# Compiler warnings
# -------------------------------------------------------------------

function(enable_project_warnings target)
    if(MSVC)
        target_compile_options(${target} PRIVATE
            /W4
            /permissive-
        )
    elseif(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
        target_compile_options(${target} PRIVATE
            -Wall
            -Wextra
            -Wpedantic
            -Wshadow
        )
    endif()
endfunction()

add_executable(${PROJECT_NAME}
    src/Main.cpp
    ${SIMULATION_SRC}
    ${INC_FILES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}/include"
)

target_compile_definitions(${PROJECT_NAME}
    PRIVATE
        UNICODE
        _UNICODE
)

enable_project_warnings(${PROJECT_NAME})

if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    target_compile_options(${PROJECT_NAME} PRIVATE
        $<$<CONFIG:Debug>:-O0>
        $<$<CONFIG:Debug>:-g>
        $<$<CONFIG:Release>:-O3>
        $<$<CONFIG:Release>:-march=native>
    )
endif()

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glm::glm
        spdlog::spdlog
        EnTT::EnTT
        cereal::cereal
        Threads::Threads
        Eigen3::Eigen
        SFML::Graphics
        SFML::Window
        SFML::System
)

add_custom_target(copy_assets ALL
    COMMAND ${CMAKE_COMMAND} -E copy_directory
        "${CMAKE_CURRENT_SOURCE_DIR}/assets"
        "$<TARGET_FILE_DIR:${PROJECT_NAME}>/assets"
)
add_dependencies(${PROJECT_NAME} copy_assets)
