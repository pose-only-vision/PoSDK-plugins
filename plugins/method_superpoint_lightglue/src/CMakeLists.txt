cmake_minimum_required(VERSION 3.19)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(method_superpoint_lightglue_plugin LANGUAGES CXX)
endif()

if(DEFINED POSDK_PLUGIN_SDK_ROOT)
    include("${POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")
elseif(DEFINED ENV{POSDK_PLUGIN_SDK_ROOT})
    include("$ENV{POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")
elseif(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/plugin-config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/plugin-config.cmake")
else()
    message(FATAL_ERROR "Set POSDK_PLUGIN_SDK_ROOT to the PoSDK Plugin SDK")
endif()

set(CURRENT_PLUGIN_DIR "${CMAKE_CURRENT_SOURCE_DIR}")

if(POSDK_GUI_CONSUMER_BUILD OR POSDK_GUI_DEVELOPER_PACKAGE_BUILD)
    message(STATUS
        "method_superpoint_lightglue: external .pospkg only; excluded from packaged Apps")
    return()
endif()

add_posdk_plugin(method_superpoint_lightglue
    PLUGIN_TYPE methods
    OPTIONAL
    PLUGIN_FOLDER "${CURRENT_PLUGIN_DIR}"
    SOURCES
        method_superpoint_lightglue.cpp
    HEADERS
        method_superpoint_lightglue.hpp
    RUNTIME_FILES
        superpoint_lightglue_worker.py
        superpoint_lightglue_ssh_runner.py
        superpoint_lightglue_ssh_agent.py
        LICENSE::method_superpoint_lightglue/LICENSE
        README.md::method_superpoint_lightglue/README.md
        THIRD_PARTY_NOTICES.md::method_superpoint_lightglue/THIRD_PARTY_NOTICES.md
        deps/LIGHTGLUE-LICENSE.txt::method_superpoint_lightglue/licenses/LightGlue-Apache-2.0.txt
        deps/SUPERPOINT-LICENSE.txt::method_superpoint_lightglue/licenses/SuperPoint-LICENSE.txt
        deps/licenses/TORCH-2.2.2-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/TORCH-2.2.2-LICENSE.txt
        deps/licenses/TORCH-2.2.2-NOTICE.txt::method_superpoint_lightglue/licenses/runtime/TORCH-2.2.2-NOTICE.txt
        deps/licenses/TORCHVISION-0.17.2-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/TORCHVISION-0.17.2-LICENSE.txt
        deps/licenses/NUMPY-1.26.4-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/NUMPY-1.26.4-LICENSE.txt
        deps/licenses/NUMPY-RANDOM-C-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/NUMPY-RANDOM-C-LICENSE.txt
        deps/licenses/NUMPY-MA-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/NUMPY-MA-LICENSE.txt
        deps/licenses/NUMPY-RANDOM-LICENSE.md::method_superpoint_lightglue/licenses/runtime/NUMPY-RANDOM-LICENSE.md
        deps/licenses/KORNIA-0.6.12-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/KORNIA-0.6.12-LICENSE.txt
        deps/licenses/PILLOW-12.3.0-LICENSE.txt::method_superpoint_lightglue/licenses/runtime/PILLOW-12.3.0-LICENSE.txt
    LINK_LIBRARIES
        PoSDK::po_core
        PoSDK::pomvg_common
        ${CMAKE_DL_LIBS}
    COMPILE_DEFINITIONS
        PLUGIN_VERSION="0.1.1"
        $<$<CONFIG:Debug>:DEBUG_BUILD=1>
    SUPPORTED_SYSTEMS
        macos
    SUPPORTED_ARCHES
        arm64
)

if(POSDK_BUILD_PLUGIN_TESTS AND COMMAND posdk_add_plugin_gtest)
    posdk_add_plugin_gtest(
        TARGET test_method_superpoint_lightglue
        PLUGIN_ID method_superpoint_lightglue
        PLUGIN_KIND method
        PLUGIN_TARGET method_superpoint_lightglue
        SOURCES tests/method_superpoint_lightglue_test.cpp
        LINK_LIBRARIES method_superpoint_lightglue PoSDK::po_core
        INCLUDE_DIRECTORIES "${CMAKE_CURRENT_SOURCE_DIR}")
elseif((BUILD_TESTS OR POSDK_BUILD_FIRST_PARTY_PLUGIN_TESTS) AND
       NOT POSDK_GUI_CONSUMER_BUILD)
    if(TARGET GTest::gtest)
        set(_superpoint_lightglue_gtest GTest::gtest)
    elseif(TARGET GTest::GTest)
        set(_superpoint_lightglue_gtest GTest::GTest)
    else()
        set(_superpoint_lightglue_gtest "")
    endif()

    if(_superpoint_lightglue_gtest)
        add_executable(test_method_superpoint_lightglue
            tests/method_superpoint_lightglue_test.cpp
            tests/method_superpoint_lightglue_gtest_main.cpp)
        target_compile_features(test_method_superpoint_lightglue PRIVATE cxx_std_17)
        target_include_directories(test_method_superpoint_lightglue PRIVATE
            "${CMAKE_SOURCE_DIR}"
            "${CMAKE_CURRENT_SOURCE_DIR}"
            "${OUTPUT_INCLUDE_DIR}")
        target_link_libraries(test_method_superpoint_lightglue PRIVATE
            PoSDK::plugin_test_harness
            method_superpoint_lightglue
            PoSDK::po_core
            ${_superpoint_lightglue_gtest})
        set_target_properties(test_method_superpoint_lightglue PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY
                "${CMAKE_BINARY_DIR}/output/bin/plugin-tests")
    else()
        message(WARNING
            "SuperPoint+LightGlue tests requested, but GoogleTest is unavailable")
    endif()
endif()
