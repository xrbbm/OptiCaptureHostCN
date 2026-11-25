cmake_minimum_required(VERSION 3.20)
project(OptiCaptureHost LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_executable(OptiCaptureHost WIN32 src/main.cpp src/host.rc)

target_compile_definitions(OptiCaptureHost PRIVATE UNICODE _UNICODE WIN32_LEAN_AND_MEAN NOMINMAX)
target_compile_options(OptiCaptureHost PRIVATE /W4 /permissive- /EHsc /utf-8)
target_include_directories(OptiCaptureHost PRIVATE third_party/ffx/include)
target_link_libraries(OptiCaptureHost PRIVATE d3d11 d3d12 d3dcompiler dxgi dwmapi shcore shell32 user32 windowsapp)

# NVIDIA DLSS (NGX SDK: headers + nvsdk_ngx_s.lib from https://github.com/NVIDIA/DLSS, fetched by the build workflow into third_party/ngx).
# Without those files the host still builds, just without the DLSS option.
set(NGX_DIR ${CMAKE_SOURCE_DIR}/third_party/ngx)
if(EXISTS ${NGX_DIR}/include/nvsdk_ngx.h AND EXISTS ${NGX_DIR}/lib/nvsdk_ngx_s.lib)
    message(STATUS "NVIDIA NGX SDK found: DLSS enabled")
    target_compile_definitions(OptiCaptureHost PRIVATE OC_HAVE_NGX)
    target_include_directories(OptiCaptureHost PRIVATE ${NGX_DIR}/include)
    target_link_libraries(OptiCaptureHost PRIVATE ${NGX_DIR}/lib/nvsdk_ngx_s.lib)
else()
    message(STATUS "NVIDIA NGX SDK not found: building without DLSS")
endif()

# The launcher: plain GDI+ window (no Direct3D, so OptiScaler's dxgi.dll is never loaded into it).
add_executable(OptiCaptureLauncher WIN32 src/launcher.cpp src/launcher.rc)

target_compile_definitions(OptiCaptureLauncher PRIVATE UNICODE _UNICODE NOMINMAX)
target_compile_options(OptiCaptureLauncher PRIVATE /W4 /permissive- /EHsc /utf-8)
target_link_libraries(OptiCaptureLauncher PRIVATE gdiplus user32 gdi32 shell32 dwmapi)

set(OC_ZH_CN_DIR "" CACHE PATH "Directory containing the generated ui_zh_CN.inc (empty builds English)")
if(OC_ZH_CN_DIR)
    if(NOT EXISTS "${OC_ZH_CN_DIR}/ui_zh_CN.inc")
        message(FATAL_ERROR "Missing generated UI catalog: ${OC_ZH_CN_DIR}/ui_zh_CN.inc")
    endif()
    foreach(app OptiCaptureHost OptiCaptureLauncher)
        target_compile_definitions(${app} PRIVATE OC_UI_ZH_CN)
        target_include_directories(${app} PRIVATE "${OC_ZH_CN_DIR}")
    endforeach()
endif()

# Optional: the ReShade depth add-on (OptiCaptureDepth.addon64). It is NOT part of the normal build (EXCLUDE_FROM_ALL): the workflow builds it
# on its own, so a problem with it can never stop the host or the launcher from building. Needs ReShade's public headers
# (third_party/reshade/include, fetched by the workflow; BSD-3-Clause).
if(EXISTS ${CMAKE_SOURCE_DIR}/third_party/reshade/include/reshade.hpp)
    add_library(OptiCaptureDepthAddon SHARED EXCLUDE_FROM_ALL src/reshade_addon/oc_depth_addon.cpp)
    set_target_properties(OptiCaptureDepthAddon PROPERTIES OUTPUT_NAME "OptiCaptureDepth" SUFFIX ".addon64")
    target_compile_definitions(OptiCaptureDepthAddon PRIVATE UNICODE _UNICODE NOMINMAX)
    target_compile_options(OptiCaptureDepthAddon PRIVATE /W3 /permissive- /EHsc /utf-8)
    target_include_directories(OptiCaptureDepthAddon PRIVATE third_party/reshade/include)
endif()
