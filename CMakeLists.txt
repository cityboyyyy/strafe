cmake_minimum_required(VERSION 3.20)

project(strafe LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_MSVC_RUNTIME_LIBRARY "MultiThreaded$<$<CONFIG:Debug>:Debug>")

add_library(imgui STATIC
    ext/imgui/imgui.cpp
    ext/imgui/imgui_draw.cpp
    ext/imgui/imgui_tables.cpp
    ext/imgui/imgui_widgets.cpp
    ext/imgui/backends/imgui_impl_win32.cpp
    ext/imgui/backends/imgui_impl_dx11.cpp
)
target_include_directories(imgui PUBLIC ext/imgui)

add_library(cgltf STATIC ext/cgltf/cgltf.c)
target_include_directories(cgltf PUBLIC ext/cgltf)
target_compile_definitions(cgltf PRIVATE _CRT_SECURE_NO_WARNINGS)

set(SHADER_DIR "${CMAKE_CURRENT_SOURCE_DIR}/src/app/render/shaders")
set(SHADER_OUT "${CMAKE_CURRENT_BINARY_DIR}/shaders")

set(SHADERS
    ${SHADER_DIR}/model_vs.hlsl
    ${SHADER_DIR}/model_ps.hlsl
    ${SHADER_DIR}/model_id_ps.hlsl
    ${SHADER_DIR}/background_vs.hlsl
    ${SHADER_DIR}/background_smoke_ps.hlsl
    ${SHADER_DIR}/goo_ps.hlsl
)
set_source_files_properties(${SHADER_DIR}/model_vs.hlsl PROPERTIES VS_SHADER_TYPE Vertex)
set_source_files_properties(${SHADER_DIR}/model_ps.hlsl PROPERTIES VS_SHADER_TYPE Pixel)
set_source_files_properties(${SHADER_DIR}/model_id_ps.hlsl PROPERTIES VS_SHADER_TYPE Pixel)
set_source_files_properties(${SHADER_DIR}/background_vs.hlsl PROPERTIES VS_SHADER_TYPE Vertex)
set_source_files_properties(${SHADER_DIR}/background_smoke_ps.hlsl PROPERTIES VS_SHADER_TYPE Pixel)
set_source_files_properties(${SHADER_DIR}/goo_ps.hlsl PROPERTIES VS_SHADER_TYPE Pixel)
foreach(shader ${SHADERS})
    get_filename_component(name ${shader} NAME_WE)
    set_source_files_properties(${shader} PROPERTIES
        VS_SHADER_MODEL 5.0
        VS_SHADER_ENTRYPOINT main
        VS_SHADER_OUTPUT_HEADER_FILE "${SHADER_OUT}/${name}.h"
        VS_SHADER_OBJECT_FILE_NAME "${SHADER_OUT}/${name}.cso"
        VS_SHADER_VARIABLE_NAME "g_${name}"
        VS_SHADER_FLAGS "/O3 /Qstrip_reflect /Qstrip_debug"
    )
endforeach()

file(GLOB_RECURSE ASSET_FILES CONFIGURE_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/assets/*")
list(FILTER ASSET_FILES EXCLUDE REGEX "/fonts/Inter-")
set(ASSET_RC_CONTENT "")
foreach(asset ${ASSET_FILES})
    file(RELATIVE_PATH relative "${CMAKE_CURRENT_SOURCE_DIR}/assets" "${asset}")
    string(TOUPPER "${relative}" resource)
    string(REGEX REPLACE "[^A-Z0-9]" "_" resource "${resource}")
    string(APPEND ASSET_RC_CONTENT "${resource} RCDATA \"${asset}\"\n")
endforeach()
add_subdirectory(src/engine)
add_subdirectory(src/driver)

set(DRIVER_PACKAGE_FILES "")
foreach(package_file DuoController.dll DuoController.inf duocontroller.cat)
    string(TOUPPER "drivers/duocontroller/${package_file}" resource)
    string(REGEX REPLACE "[^A-Z0-9]" "_" resource "${resource}")
    string(APPEND ASSET_RC_CONTENT "${resource} RCDATA \"${DRIVER_PACKAGE_DIR}/${package_file}\"\n")
    list(APPEND DRIVER_PACKAGE_FILES "${DRIVER_PACKAGE_DIR}/${package_file}")
endforeach()
set(ASSET_RC "${CMAKE_CURRENT_BINARY_DIR}/assets.rc")
file(CONFIGURE OUTPUT "${ASSET_RC}" CONTENT "${ASSET_RC_CONTENT}" @ONLY)
set_source_files_properties(${ASSET_RC} PROPERTIES OBJECT_DEPENDS "${DRIVER_PACKAGE_FILES}")

add_library(app_model STATIC
    src/app/model/catalog.cpp
    src/app/model/image_decoder.cpp
    src/app/model/library.cpp
    src/app/model/loader.cpp
    src/app/model/parts.cpp
    src/app/model/asset_store.cpp
    src/app/model/emulation.cpp
    src/app/model/buttons.cpp
    src/app/model/layout.cpp
)
target_include_directories(app_model PUBLIC src/app)
target_compile_definitions(app_model PUBLIC UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(app_model PRIVATE /W4 /permissive- /MP)
target_link_libraries(app_model PUBLIC cgltf windowscodecs ole32)

add_executable(app WIN32
    src/app/main.cpp
    src/app/window/window.cpp
    src/app/render/device.cpp
    src/app/render/background.cpp
    src/app/render/gpu_model.cpp
    src/app/render/scene.cpp
    src/app/render/goo.cpp
    src/app/ui/ui.cpp
    src/app/ui/controller_view.cpp
    src/app/ui/remap_view.cpp
    src/app/ui/binding_editor.cpp
    src/app/ui/input_capture.cpp
    src/app/system/driver_installer.cpp
    src/app/ui/fonts.cpp
    src/app/ui/icons.cpp
    src/app/ui/paint.cpp
    src/app/ui/title_bar.cpp
    src/app/ui/widgets/arrow_button.cpp
    src/app/ui/widgets/button_badge.cpp
    src/app/ui/widgets/callout_chip.cpp
    src/app/ui/widgets/knob.cpp
    src/app/ui/widgets/install_button.cpp
    src/app/ui/widgets/segmented.cpp
    src/app/ui/curve_view.cpp
    src/app/ui/widgets/confirm_button.cpp
    ${SHADERS}
    ${SHADER_DIR}/common.hlsli
    ${ASSET_RC}
    ${SHADER_DIR}/background_common.hlsli
)
target_include_directories(app PRIVATE src/app ${SHADER_OUT})
target_compile_definitions(app PRIVATE UNICODE _UNICODE NOMINMAX WIN32_LEAN_AND_MEAN)
target_compile_options(app PRIVATE /W4 /permissive- /MP)
target_link_libraries(app PRIVATE app_model engine imgui d3d11 dxgi dcomp shell32 cfgmgr32 newdev)

set_target_properties(app PROPERTIES
    OUTPUT_NAME "Strafe"
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_SOURCE_DIR}/bin"
)
add_dependencies(app duocontroller)
target_link_options(app PRIVATE "/MANIFESTUAC:level='requireAdministrator' uiAccess='false'")

if(EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/src/tools/part_dump/main.cpp")
    add_executable(part_dump src/tools/part_dump/main.cpp)
    target_compile_options(part_dump PRIVATE /W4 /permissive-)
    target_link_libraries(part_dump PRIVATE app_model)
    set_target_properties(part_dump PROPERTIES RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/tools")
endif()
