cmake_minimum_required(VERSION 4.0)
project(probable_adventure VERSION 0.1 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 23)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_EXPORT_COMPILE_COMMANDS ON)

set(BUILD_SHARED_LIBS ON CACHE BOOL "" FORCE)

include(FetchContent)

find_package(Threads REQUIRED)

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

FetchContent_Declare(
    cereal
    GIT_REPOSITORY https://github.com/USCiLab/cereal.git
    GIT_TAG v1.3.2
    SYSTEM
)

set(JUST_INSTALL_CEREAL ON CACHE BOOL "" FORCE)

FetchContent_Declare(
    imgui
    GIT_REPOSITORY https://github.com/ocornut/imgui.git
    GIT_TAG v1.91.1
    SYSTEM
)

FetchContent_Declare(
    sfml
    GIT_REPOSITORY https://github.com/SFML/SFML.git
    GIT_TAG 3.1.0
    SYSTEM
)

FetchContent_Declare(
    imgui-sfml
    GIT_REPOSITORY https://github.com/SFML/imgui-sfml.git
    GIT_TAG v3.0
    SYSTEM
)

FetchContent_MakeAvailable(
    entt
    glm
    spdlog
    cereal
    imgui
    sfml
)

set(IMGUI_DIR "${imgui_SOURCE_DIR}" CACHE PATH "" FORCE)

FetchContent_MakeAvailable(imgui-sfml)

set(SIMULATION_SRC
    src/Simulation.cpp
    src/Serialization.cpp
    src/AiManager.cpp
    src/MonsterFactory.cpp
    src/AiSystems.cpp
    src/ActionSystems.cpp
    src/CombatSystems.cpp
    src/MovementSystems.cpp
    src/SpawnSystems.cpp
)

file(GLOB_RECURSE INC_FILES CONFIGURE_DEPENDS
    include/*.hpp
    include/*.h
)

add_executable(${PROJECT_NAME}
    src/Main.cpp
    ${SIMULATION_SRC}
    ${INC_FILES}
)

target_include_directories(${PROJECT_NAME}
    PRIVATE include
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
)

target_compile_options(${PROJECT_NAME}
    PRIVATE
        $<$<CONFIG:Debug>:-O0 -g>
        $<$<CONFIG:Release>:-O3 -march=native>
)

target_link_libraries(${PROJECT_NAME}
    PRIVATE
        glm::glm
        spdlog::spdlog
        EnTT::EnTT
        cereal::cereal
        Threads::Threads
)

add_executable(${PROJECT_NAME}_viewer
    src/Viewer.cpp
    src/Serialization.cpp
    ${INC_FILES}
)

target_include_directories(${PROJECT_NAME}_viewer
    PRIVATE include
)

target_compile_options(${PROJECT_NAME}_viewer
    PRIVATE
        -Wall
        -Wextra
        -Wpedantic
        -Wshadow
        -Wconversion
)

target_link_libraries(${PROJECT_NAME}_viewer
    PRIVATE
        glm::glm
        EnTT::EnTT
        cereal::cereal
        SFML::Graphics
        SFML::Window
        SFML::System
        ImGui-SFML::ImGui-SFML
)