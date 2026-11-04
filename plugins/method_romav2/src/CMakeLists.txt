cmake_minimum_required(VERSION 3.19)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(method_romav2_plugin LANGUAGES CXX)
    set(CMAKE_CXX_STANDARD 17)
endif()
if(DEFINED POSDK_PLUGIN_SDK_ROOT)
    include("${POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")
elseif(DEFINED ENV{POSDK_PLUGIN_SDK_ROOT})
    include("$ENV{POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/plugin-config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/plugin-config.cmake")
else()
    message(FATAL_ERROR "Set POSDK_PLUGIN_SDK_ROOT to the matching Plugin SDK")
endif()
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR AND DEFINED POSDK_PLUGIN_SDK_ROOT)
    find_package(OpenCV CONFIG REQUIRED
        PATHS "${POSDK_PLUGIN_SDK_ROOT}/dependencies/opencv/lib/cmake/opencv4"
        NO_DEFAULT_PATH)
endif()
if(POSDK_GUI_CONSUMER_BUILD OR POSDK_GUI_DEVELOPER_PACKAGE_BUILD)
    return()
endif()
add_posdk_plugin(method_romav2
    PLUGIN_TYPE methods OPTIONAL
    PLUGIN_FOLDER "${CMAKE_CURRENT_SOURCE_DIR}"
    SOURCES method_romav2.cpp
    HEADERS method_romav2.hpp plugin_runtime_path.hpp
    RUNTIME_FILES
        romav2_worker.py
        deps/runtime-lock.json::romav2-runtime-lock.json
        deps/dense_aggregation.py::romav2_aggregation.py
        licenses/DINOv3-LICENSE.md::method_romav2/licenses/DINOv3-LICENSE.md
        licenses/RoMaV2-MIT.txt::method_romav2/licenses/RoMaV2-MIT.txt
        README.md::method_romav2/README.md
        THIRD_PARTY_NOTICES.md::method_romav2/THIRD_PARTY_NOTICES.md
        LICENSE::method_romav2/LICENSE
    LINK_LIBRARIES PoSDK::po_core PoSDK::pomvg_common ${OpenCV_LIBS} ${CMAKE_DL_LIBS}
    INCLUDE_DIRS ${OpenCV_INCLUDE_DIRS}
    COMPILE_DEFINITIONS PLUGIN_VERSION="0.1.0"
    SUPPORTED_SYSTEMS macos
    SUPPORTED_ARCHES arm64)
if(APPLE)
    target_link_options(method_romav2 PRIVATE "LINKER:-dead_strip_dylibs")
endif()
