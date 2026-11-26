set(DUO_SOURCE "${CMAKE_SOURCE_DIR}/ext/duocontroller/DuoController")
set(WDK_ROOT "C:/Program Files (x86)/Windows Kits/10")
set(WDK_VERSION "10.0.26100.0")
set(UMDF_MINOR 25)

add_library(duocontroller SHARED
    ${DUO_SOURCE}/Device.c
    ${DUO_SOURCE}/Driver.c
    ${DUO_SOURCE}/DuoController.c
    ${DUO_SOURCE}/Ioctl.c
    ${DUO_SOURCE}/MessageProcessor.c
    ${DUO_SOURCE}/Queue.c
    ${DUO_SOURCE}/SharedMemoryServer.c
    ${DUO_SOURCE}/DuoController.def
    ${DUO_SOURCE}/DuoController.rc
)
set_target_properties(duocontroller PROPERTIES
    OUTPUT_NAME "DuoController"
    LINKER_LANGUAGE C
    RUNTIME_OUTPUT_DIRECTORY "$<1:${CMAKE_BINARY_DIR}/driver/package>"
    MSVC_RUNTIME_LIBRARY "MultiThreaded"
)
target_include_directories(duocontroller PRIVATE
    "${CMAKE_CURRENT_SOURCE_DIR}/trace"
    "${DUO_SOURCE}"
    "${WDK_ROOT}/Include/wdf/umdf/2.${UMDF_MINOR}"
)
target_include_directories(duocontroller SYSTEM PRIVATE "${WDK_ROOT}/Include/${WDK_VERSION}/km")
target_compile_definitions(duocontroller PRIVATE
    UNICODE _UNICODE _WINDOWS _USRDLL DuoController_EXPORTS NDEBUG
    UMDF_VERSION_MAJOR=2 UMDF_VERSION_MINOR=${UMDF_MINOR}
    _NT_TARGET_VERSION=0xA000005 NTDDI_VERSION=0x0A000005 _WIN32_WINNT=0x0A00 WINVER=0x0A00
)
target_compile_options(duocontroller PRIVATE /W3 /wd4201 /wd4214)
target_link_directories(duocontroller PRIVATE "${WDK_ROOT}/Lib/wdf/umdf/x64/2.${UMDF_MINOR}")
target_link_libraries(duocontroller PRIVATE WdfDriverStubUm setupapi newdev cfgmgr32 runtimeobject advapi32 kernel32 user32 ole32 ntdll)

set(DRIVER_PACKAGE_DIR "${CMAKE_BINARY_DIR}/driver/package" PARENT_SCOPE)
add_custom_command(TARGET duocontroller POST_BUILD
    COMMAND powershell -NoProfile -ExecutionPolicy Bypass -File "${CMAKE_CURRENT_SOURCE_DIR}/package.ps1"
        -Dll "$<TARGET_FILE:duocontroller>"
        -Inf "${DUO_SOURCE}/DuoController.inf"
        -OutDir "${CMAKE_BINARY_DIR}/driver/package"
    VERBATIM
)
