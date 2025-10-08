cmake_minimum_required(VERSION 3.16)
project(leaf-can-dashboard VERSION 1.0.0 LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Detect platform
if(WIN32)
    set(PLATFORM "windows")
    add_definitions(-DPLATFORM_WINDOWS)
elseif(UNIX)
    set(PLATFORM "linux")
    add_definitions(-DPLATFORM_LINUX)
endif()

message(STATUS "Building for platform: ${PLATFORM}")

# LVGL configuration
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

# Fetch LVGL
include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v8.3.11
)
FetchContent_MakeAvailable(lvgl)

# Source files
set(SOURCES
    src/main.cpp
    src/ui/dashboard_ui.cpp
    src/shared/can_receiver.cpp
)

# Platform-specific sources
if(PLATFORM STREQUAL "windows")
    list(APPEND SOURCES
        src/platform/windows/sdl_display.cpp
        src/platform/windows/mock_can.cpp
    )
elseif(PLATFORM STREQUAL "linux")
    list(APPEND SOURCES
        src/platform/linux/fbdev_display.cpp
        src/platform/linux/socketcan.cpp
    )
endif()

# Executable
add_executable(${PROJECT_NAME} ${SOURCES})

# Include directories
target_include_directories(${PROJECT_NAME} PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}/src
    ${CMAKE_CURRENT_SOURCE_DIR}/src/ui
    ${CMAKE_CURRENT_SOURCE_DIR}/src/platform
    ${CMAKE_CURRENT_SOURCE_DIR}/src/shared
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/LeafCANBus/src
)

# Link LVGL
target_link_libraries(${PROJECT_NAME} PRIVATE lvgl::lvgl)

# Platform-specific dependencies
if(PLATFORM STREQUAL "windows")
    # SDL2 for Windows
    find_package(SDL2 REQUIRED)
    target_include_directories(${PROJECT_NAME} PRIVATE ${SDL2_INCLUDE_DIRS})
    target_link_libraries(${PROJECT_NAME} PRIVATE ${SDL2_LIBRARIES})

    # Copy SDL2.dll to output directory on Windows
    if(SDL2_LIBRARIES)
        get_filename_component(SDL2_DLL_DIR ${SDL2_LIBRARIES} DIRECTORY)
        file(GLOB SDL2_DLLS "${SDL2_DLL_DIR}/*.dll")
        foreach(DLL ${SDL2_DLLS})
            add_custom_command(TARGET ${PROJECT_NAME} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different
                ${DLL} $<TARGET_FILE_DIR:${PROJECT_NAME}>
            )
        endforeach()
    endif()

elseif(PLATFORM STREQUAL "linux")
    # pthreads for Linux
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# Installation
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# Print build info
message(STATUS "Sources: ${SOURCES}")
message(STATUS "C++ Standard: ${CMAKE_CXX_STANDARD}")
