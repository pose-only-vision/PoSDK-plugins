# ==============================================================================
# LoFTR Plugin CMakeLists.txt
# LoFTR detector-free feature matching plugin build configuration
# LoFTR 无检测器特征匹配插件构建配置
# ==============================================================================

cmake_minimum_required(VERSION 3.15)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(method_loftr_plugin LANGUAGES CXX)
endif()

# Include plugin configuration tools
include(../../cmake/plugin-config.cmake)

# Set current plugin directory
set(CURRENT_PLUGIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# ------------------------------------------------------------------------------
# LoFTR Plugin
# ------------------------------------------------------------------------------
add_posdk_plugin(method_loftr
    PLUGIN_TYPE methods
    PLUGIN_FOLDER ${CURRENT_PLUGIN_DIR}
    SOURCES
        loftr_pipeline.cpp
        loftr_fast_mode.cpp
        LoFTRParams.cpp
        LoFTRMatcher.cpp
    HEADERS
        loftr_pipeline.hpp
        LoFTRParams.hpp
        LoFTRMatcher.hpp
        LoFTRFeatureIndex.hpp
    RUNTIME_FILES
        loftr_matcher.py
        loftr_batch_worker.py
        loftr_aggregation.py
        loftr_rotation.py
        loftr_ssh_runner.py
        loftr_ssh_agent.py
        LICENSE::method_loftr/LICENSE
        THIRD_PARTY_NOTICES.md::method_loftr/THIRD_PARTY_NOTICES.md
        README.md::method_loftr/README.md
        deps/licenses/TORCH-2.2.2-LICENSE.txt::method_loftr/licenses/runtime/TORCH-2.2.2-LICENSE.txt
        deps/licenses/TORCH-2.2.2-NOTICE.txt::method_loftr/licenses/runtime/TORCH-2.2.2-NOTICE.txt
        deps/licenses/TORCHVISION-0.17.2-LICENSE.txt::method_loftr/licenses/runtime/TORCHVISION-0.17.2-LICENSE.txt
        deps/licenses/NUMPY-1.26.4-LICENSE.txt::method_loftr/licenses/runtime/NUMPY-1.26.4-LICENSE.txt
        deps/licenses/KORNIA-0.7.4-LICENSE.txt::method_loftr/licenses/runtime/KORNIA-0.7.4-LICENSE.txt
        deps/licenses/OPENCV-PYTHON-4.10.0.84-LICENSE.txt::method_loftr/licenses/runtime/OPENCV-PYTHON-4.10.0.84-LICENSE.txt
        deps/licenses/OPENCV-PYTHON-4.10.0.84-LICENSE-3RD-PARTY.txt::method_loftr/licenses/runtime/OPENCV-PYTHON-4.10.0.84-LICENSE-3RD-PARTY.txt
    LINK_LIBRARIES
        ${CMAKE_DL_LIBS}
        PoSDK::po_core
        PoSDK::pomvg_converter
        PoSDK::pomvg_common
        ${OpenCV_LIBS}
    INCLUDE_DIRS
        ${OpenCV_INCLUDE_DIRS}
    COMPILE_DEFINITIONS
        PLUGIN_VERSION="0.3.1"
        $<$<PLATFORM_ID:Linux>:_GNU_SOURCE>
        $<$<CONFIG:Debug>:DEBUG_BUILD=1>
    SUPPORTED_SYSTEMS macos
    SUPPORTED_ARCHES arm64
)

# ------------------------------------------------------------------------------
# Plugin information summary
# ------------------------------------------------------------------------------
message(STATUS "LoFTR Plugin Configuration:")
message(STATUS "  Plugin Name: method_loftr")
message(STATUS "  Plugin Type: methods")
message(STATUS "  Plugin File: posdk_plugin_method_loftr.dylib/.so/.dll")
message(STATUS "  Plugin Folder: ${CURRENT_PLUGIN_DIR}")
message(STATUS "  Sources: loftr_pipeline.cpp, loftr_fast_mode.cpp, LoFTRParams.cpp, LoFTRMatcher.cpp")
message(STATUS "  Headers: loftr_pipeline.hpp, LoFTRParams.hpp, LoFTRMatcher.hpp")
message(STATUS "  Config: method_loftr.ini")
