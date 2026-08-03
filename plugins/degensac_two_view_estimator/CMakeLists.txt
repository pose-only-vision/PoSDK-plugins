# ==============================================================================
# DegensacTwoViewEstimator Plugin CMakeLists.txt
# Degensac两视图位姿估计插件构建配置（纯C++版本，无需Python）
# ==============================================================================

cmake_minimum_required(VERSION 3.19)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(degensac_two_view_estimator_plugin LANGUAGES C CXX)
endif()

# 启用C语言支持（degensac是C代码）
enable_language(C)

# 包含插件配置工具
if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/plugin-config.cmake")
    include("${CMAKE_CURRENT_SOURCE_DIR}/../../cmake/plugin-config.cmake")
elseif(DEFINED POSDK_PLUGIN_SDK_ROOT)
    include("${POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")
elseif(DEFINED ENV{POSDK_PLUGIN_SDK_ROOT})
    include("$ENV{POSDK_PLUGIN_SDK_ROOT}/plugins/cmake/plugin-config.cmake")
else()
    message(FATAL_ERROR "Set POSDK_PLUGIN_SDK_ROOT to the PoSDK Plugin SDK")
endif()

# 设置当前插件目录的特定配置
set(CURRENT_PLUGIN_DIR ${CMAKE_CURRENT_SOURCE_DIR})

# Degensac/LiAS/PyDegensac are distributed as independent .pospkg plugins.
# Packaged GUI builds must not inspect or compile their external source trees;
# the installed plugin lifecycle is owned by Plugin Manager instead.
if(POSDK_GUI_CONSUMER_BUILD OR POSDK_GUI_DEVELOPER_PACKAGE_BUILD)
    message(STATUS
        "degensac_two_view_estimator: external .pospkg only; excluded from packaged App")
    return()
endif()

# ------------------------------------------------------------------------------
# Degensac C源代码路径
# ------------------------------------------------------------------------------
set(DEGENSAC_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/pydegensac/src/pydegensac)

# 检查 degensac 源代码是否存在
if(NOT EXISTS ${DEGENSAC_SRC_DIR}/degensac/exp_ranF.c)
    message(WARNING "Degensac source code not found at ${DEGENSAC_SRC_DIR}")
    message(WARNING "The plugin's vendored pydegensac source tree is incomplete")
    message(WARNING "Skipping DegensacTwoViewEstimator plugin build")
    return()
endif()

# ------------------------------------------------------------------------------
# 查找LAPACK和BLAS依赖
# ------------------------------------------------------------------------------
find_package(LAPACK REQUIRED)
find_package(BLAS REQUIRED)

# ------------------------------------------------------------------------------
# Degensac C源文件
# ------------------------------------------------------------------------------
set(DEGENSAC_C_SOURCES
    ${DEGENSAC_SRC_DIR}/degensac/exp_ranF.c
    ${DEGENSAC_SRC_DIR}/degensac/exp_ranH.c
    ${DEGENSAC_SRC_DIR}/degensac/ranF.c
    ${DEGENSAC_SRC_DIR}/degensac/ranH.c
    ${DEGENSAC_SRC_DIR}/degensac/ranH2el.c
    ${DEGENSAC_SRC_DIR}/degensac/Ftools.c
    ${DEGENSAC_SRC_DIR}/degensac/Htools.c
    ${DEGENSAC_SRC_DIR}/degensac/rtools.c
    ${DEGENSAC_SRC_DIR}/degensac/utools.c
    ${DEGENSAC_SRC_DIR}/degensac/DegUtils.c
    ${DEGENSAC_SRC_DIR}/degensac/hash.c
    ${DEGENSAC_SRC_DIR}/degensac/lapwrap.c
)

# Matutls数学库源文件
file(GLOB MATUTLS_C_SOURCES ${DEGENSAC_SRC_DIR}/matutls/*.c)

# ------------------------------------------------------------------------------
# 创建degensac静态库（包含所有C源文件）
# 注意：必须在add_posdk_plugin之前创建，以便作为依赖链接
# ------------------------------------------------------------------------------
add_library(degensac_core STATIC
    degensac_rng.c
    ${DEGENSAC_C_SOURCES}
    ${MATUTLS_C_SOURCES}
)

# 设置degensac_core的include目录
target_include_directories(degensac_core
    PUBLIC
        ${DEGENSAC_SRC_DIR}
        ${DEGENSAC_SRC_DIR}/degensac
        ${DEGENSAC_SRC_DIR}/matutls
        ${CMAKE_CURRENT_SOURCE_DIR}
)

# 链接LAPACK和BLAS
target_link_libraries(degensac_core
    PUBLIC
        ${LAPACK_LIBRARIES}
        ${BLAS_LIBRARIES}
)

# 使用 -fgnu89-inline 解决上游 C 源码的 inline 链接语义；不要用 -w
# 隐藏安全相关诊断。
target_compile_options(degensac_core PRIVATE -fgnu89-inline)
if(CMAKE_C_COMPILER_ID MATCHES "Clang")
    # CCMATH 2.2.1 predates C99 complex builtins and intentionally uses the
    # same historical names with its own struct complex ABI. Suppress only
    # that known declaration warning; retain all other compiler diagnostics.
    target_compile_options(degensac_core PRIVATE
        -Wno-incompatible-library-redeclaration)
endif()

# 设置为位置无关代码（PIC），以便链接到动态库
# 明确设置链接器语言为C（因为所有源文件都是.c文件）
set_target_properties(degensac_core PROPERTIES
    POSITION_INDEPENDENT_CODE ON
    LINKER_LANGUAGE C
    C_STANDARD 11
    C_STANDARD_REQUIRED ON
    C_EXTENSIONS OFF
)

# ------------------------------------------------------------------------------
# DegensacTwoViewEstimator插件 - 使用标准add_posdk_plugin宏
# 这是关键！add_posdk_plugin会自动定义PLUGIN_NAME宏，用于安全令牌验证
# ------------------------------------------------------------------------------
add_posdk_plugin(degensac_two_view_estimator
    PLUGIN_TYPE methods
    OPTIONAL  # ← 标记为可选，构建失败不影响其他插件
    SOURCES
        degensac_two_view_estimator.cpp
    HEADERS
        degensac_two_view_estimator.hpp
        degensac_rng.h
    RUNTIME_FILES
        THIRD_PARTY_NOTICES.md::degensac_two_view_estimator/THIRD_PARTY_NOTICES.md
        LICENSE::degensac_two_view_estimator/licenses/PoSDK-Adapter-MPL-2.0.txt
        pydegensac/LICENSE::degensac_two_view_estimator/licenses/pydegensac-MIT.txt
        pydegensac/src/pydegensac/matutls/lgpl.license::degensac_two_view_estimator/licenses/CCMATH-LGPL-2.1.txt
    LINK_LIBRARIES
        degensac_core              # ← 链接degensac静态库
        opencv_core
        opencv_calib3d
    INCLUDE_DIRS
        ${OpenCV_INCLUDE_DIRS}
        ${DEGENSAC_SRC_DIR}
        ${DEGENSAC_SRC_DIR}/degensac
        ${DEGENSAC_SRC_DIR}/matutls
    COMPILE_DEFINITIONS
        PLUGIN_VERSION="1.1.1"
        $<$<CONFIG:Debug>:DEBUG_BUILD=1>
    SUPPORTED_SYSTEMS
        macos
    SUPPORTED_ARCHES
        arm64
)

# ------------------------------------------------------------------------------
# 插件信息摘要
# ------------------------------------------------------------------------------
message(STATUS "DegensacTwoViewEstimator Plugin Configuration:")
message(STATUS "  Plugin Name: degensac_two_view_estimator")
message(STATUS "  Plugin Type: methods")
message(STATUS "  Plugin File: posdk_plugin_degensac_two_view_estimator.dylib/.so/.dll")
message(STATUS "  Plugin Folder: ${CURRENT_PLUGIN_DIR}")
message(STATUS "  Degensac Source: ${DEGENSAC_SRC_DIR}")
message(STATUS "  Dependencies: OpenCV, LAPACK, BLAS, degensac_core")
message(STATUS "  Note: Pure C++ implementation, no Python required!")

# 调试信息
if(CMAKE_BUILD_TYPE STREQUAL "Debug")
    message(STATUS "Building DegensacTwoViewEstimator plugin in Debug mode")
endif()

# 测试支持（如果启用测试）
if(BUILD_TESTING)
    message(STATUS "DegensacTwoViewEstimator plugin: Testing enabled")
endif()

# 文档生成（如果启用文档）
if(BUILD_DOCUMENTATION)
    message(STATUS "DegensacTwoViewEstimator plugin: Documentation enabled")
endif()
