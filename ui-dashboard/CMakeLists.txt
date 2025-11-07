cmake_minimum_required(VERSION 3.16)

# -------- Project --------
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

# -------- LVGL config --------
set(LV_CONF_PATH "${CMAKE_CURRENT_SOURCE_DIR}/lv_conf.h" CACHE STRING "" FORCE)

include(FetchContent)
FetchContent_Declare(
    lvgl
    GIT_REPOSITORY https://github.com/lvgl/lvgl.git
    GIT_TAG v8.3.11
)
FetchContent_MakeAvailable(lvgl)

# -------- Sources --------
set(SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/src)
set(UI_DIR  ${SRC_DIR}/ui)
set(UI_SCREENS_DIR    ${UI_DIR}/screens)
set(UI_COMPONENTS_DIR ${UI_DIR}/components)
set(UI_ASSETS_DIR     ${UI_DIR}/assets)   # legacy/alt location
set(UI_IMAGES_DIR     ${UI_DIR}/images)   # your actual location
set(PLAT_DIR ${SRC_DIR}/platform)
set(SHARED_DIR ${SRC_DIR}/shared)

# SquareLine generated C files (auto-pick what exists, including images/assets)
file(GLOB UI_SCREENS     CONFIGURE_DEPENDS "${UI_SCREENS_DIR}/*.c")
file(GLOB UI_COMPONENTS  CONFIGURE_DEPENDS "${UI_COMPONENTS_DIR}/*.c")
file(GLOB UI_ASSETS_C    CONFIGURE_DEPENDS "${UI_ASSETS_DIR}/*.c")
file(GLOB UI_IMAGES_C    CONFIGURE_DEPENDS "${UI_IMAGES_DIR}/*.c")

set(SOURCES
    ${SRC_DIR}/main.cpp
    ${SRC_DIR}/ui/dashboard_ui.cpp
    ${SHARED_DIR}/can_receiver.cpp

    # SquareLine core
    ${UI_DIR}/ui.c
    ${UI_DIR}/ui_helpers.c

    # Generated submodules
    ${UI_COMPONENTS}
    ${UI_SCREENS}
    ${UI_ASSETS_C}
    ${UI_IMAGES_C}
)

if(PLATFORM STREQUAL "windows")
    list(APPEND SOURCES
        ${PLAT_DIR}/windows/sdl_display.cpp
        ${PLAT_DIR}/windows/mock_can.cpp
    )
elseif(PLATFORM STREQUAL "linux")
    list(APPEND SOURCES
        ${PLAT_DIR}/linux/fbdev_display.cpp
        ${PLAT_DIR}/linux/socketcan.cpp
    )
endif()

add_executable(${PROJECT_NAME} ${SOURCES})

target_include_directories(${PROJECT_NAME} PRIVATE
    ${SRC_DIR}
    ${UI_DIR}
    ${UI_COMPONENTS_DIR}
    ${UI_SCREENS_DIR}
    ${UI_ASSETS_DIR}
    ${UI_IMAGES_DIR}
    ${PLAT_DIR}
    ${PLAT_DIR}/windows
    ${PLAT_DIR}/linux
    ${SHARED_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../lib/LeafCANBus/src
)

target_link_libraries(${PROJECT_NAME} PRIVATE lvgl::lvgl)

# -------- Platform deps --------
if(PLATFORM STREQUAL "windows")
    find_package(SDL2 CONFIG REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE SDL2::SDL2 SDL2::SDL2main)
elseif(PLATFORM STREQUAL "linux")
    find_package(Threads REQUIRED)
    target_link_libraries(${PROJECT_NAME} PRIVATE Threads::Threads)
endif()

# -------- Install --------
install(TARGETS ${PROJECT_NAME} DESTINATION bin)

# -------- Info --------
message(STATUS "SquareLine UI sources found:")
foreach(f IN LISTS UI_SCREENS)
    message(STATUS "  - ${f}")
endforeach()
if(UI_ASSETS_C)
    message(STATUS "SquareLine assets (assets/) found:")
    foreach(f IN LISTS UI_ASSETS_C)
        message(STATUS "  - ${f}")
    endforeach()
endif()
if(UI_IMAGES_C)
    message(STATUS "SquareLine images (images/) found:")
    foreach(f IN LISTS UI_IMAGES_C)
        message(STATUS "  - ${f}")
    endforeach()
endif()

if(NOT UI_ASSETS_C AND NOT UI_IMAGES_C)
    message(WARNING "No image C files found in ${UI_ASSETS_DIR} or ${UI_IMAGES_DIR}. If you use images, export them as C arrays (ui_img_*.c).")
endif()
