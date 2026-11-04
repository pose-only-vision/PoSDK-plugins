# ==============================================================================
# MethodRotationAveraging Plugin CMakeLists.txt
# 旋转平均插件构建配置
# ==============================================================================

cmake_minimum_required(VERSION 3.15)

# 包含插件配置工具
include(../../cmake/plugin-config.cmake)

# 设置当前插件目录的特定配置
set(CURRENT_PLUGIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------------------
# MethodRotationAveraging插件
# ------------------------------------------------------------------------------
if(POSDK_GUI_CONSUMER_BUILD OR POSDK_GUI_DEVELOPER_PACKAGE_BUILD)
    if(NOT POSDK_RELEASE_CONTENT_PROFILE STREQUAL "custom" OR
       NOT "method_rotation_averaging" IN_LIST POSDK_RELEASE_PLUGIN_IDS)
        message(STATUS "MethodRotationAveraging: not selected for this App")
        return()
    endif()
endif()

add_posdk_plugin(method_rotation_averaging
    CONSUMER_READY
    PLUGIN_TYPE methods
    PLUGIN_FOLDER ${CURRENT_PLUGIN_DIR}
    SOURCES
        method_rotation_averaging.cpp
    HEADERS
        method_rotation_averaging.hpp
    RUNTIME_FILES
        README.md::method_rotation_averaging/README.md
        LICENSE.md::method_rotation_averaging/LICENSE.md
        THIRD_PARTY_NOTICES.md::method_rotation_averaging/THIRD_PARTY_NOTICES.md
    LINK_LIBRARIES
        PoSDK::po_core
        PoSDK::pomvg_converter
        PoSDK::pomvg_common
    COMPILE_DEFINITIONS
        PLUGIN_VERSION="0.1.1"
        $<$<CONFIG:Debug>:DEBUG_BUILD=1>
    SUPPORTED_SYSTEMS macos
    SUPPORTED_ARCHES arm64
)

# ------------------------------------------------------------------------------
# 插件特定配置
# ------------------------------------------------------------------------------

# 插件信息摘要
message(STATUS "MethodRotationAveraging Plugin Configuration:")
message(STATUS "  Plugin Name: method_rotation_averaging")
message(STATUS "  Plugin Type: methods")
message(STATUS "  Plugin File: posdk_plugin_method_rotation_averaging.dylib/.so/.dll")
message(STATUS "  Plugin Folder: ${CURRENT_PLUGIN_DIR}")
message(STATUS "  Sources: method_rotation_averaging.cpp")
message(STATUS "  Headers: method_rotation_averaging.hpp")
message(STATUS "  Dependencies: method_rotation_averaging_chatterjee")

# 调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building MethodRotationAveraging plugin in Debug mode")
endif()

# 测试支持（如果启用测试）
if(BUILD_TESTING)
    message(STATUS "MethodRotationAveraging plugin: Testing enabled")
endif()

if(BUILD_TESTS AND TARGET GTest::gtest)
    add_executable(test_method_rotation_averaging_fixed_core
        tests/method_rotation_averaging_fixed_core_test.cpp)
    target_link_libraries(test_method_rotation_averaging_fixed_core
        PRIVATE method_rotation_averaging
                method_rotation_averaging_chatterjee
                PoSDK::po_core GTest::gtest)
    target_include_directories(test_method_rotation_averaging_fixed_core
        PRIVATE ${CURRENT_PLUGIN_DIR} ${OUTPUT_INCLUDE_DIR})
    target_compile_features(test_method_rotation_averaging_fixed_core
        PRIVATE cxx_std_17)
    set_target_properties(test_method_rotation_averaging_fixed_core PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/output/bin/plugin-tests")
    add_test(NAME PoSDK.RotationAveragingFixedCore
        COMMAND $<TARGET_FILE:test_method_rotation_averaging_fixed_core>)
endif()

# 文档生成（如果启用文档）
if(BUILD_DOCUMENTATION)
    message(STATUS "MethodRotationAveraging plugin: Documentation enabled")
endif()
