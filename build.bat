@echo off
setlocal

call "D:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat" -arch=amd64
if errorlevel 1 (
    echo [ERROR] VsDevCmd failed
    exit /b 1
)

set SRC_DIR=%~dp0URH\src
set INC_DIR=%~dp0URH\include
set OUT_DIR=%~dp0bin\obj
set LIB_DIR=%~dp0bin

if not exist "%OUT_DIR%" mkdir "%OUT_DIR%"
if not exist "%LIB_DIR%" mkdir "%LIB_DIR%"

set COMMON_FLAGS=/nologo /W4 /WX /EHsc /std:c++17 /O2 /MT /c
set INCLUDES=/I"%SRC_DIR%" /I"%INC_DIR%"
set DEFINES=/DUNICODE /D_UNICODE /DNOMINMAX /DWIN32_LEAN_AND_MEAN

echo === Compiling URH ===

set OBJ_LIST=

for %%F in (
    "%SRC_DIR%\urh_autohook.cpp"
    "%SRC_DIR%\urh_autohook_dispatch.cpp"
    "%SRC_DIR%\urh_autohook_helpers.cpp"
    "%SRC_DIR%\urh_dx11_bootstrap.cpp"
    "%SRC_DIR%\urh_dx11_context.cpp"
    "%SRC_DIR%\urh_dx11_debug.cpp"
    "%SRC_DIR%\urh_dx11_hooks_common.cpp"
    "%SRC_DIR%\urh_dx11_hooks_present.cpp"
    "%SRC_DIR%\urh_dx11_probe.cpp"
    "%SRC_DIR%\urh_dx11_resources.cpp"
    "%SRC_DIR%\urh_dx12_bootstrap.cpp"
    "%SRC_DIR%\urh_dx12_context.cpp"
    "%SRC_DIR%\urh_dx12_debug.cpp"
    "%SRC_DIR%\urh_dx12_hooks_aux.cpp"
    "%SRC_DIR%\urh_dx12_hooks_common.cpp"
    "%SRC_DIR%\urh_dx12_hooks_present.cpp"
    "%SRC_DIR%\urh_dx12_probe.cpp"
    "%SRC_DIR%\urh_dx12_resources.cpp"
    "%SRC_DIR%\urh_vulkan_bootstrap.cpp"
    "%SRC_DIR%\urh_vulkan_tracking.cpp"
    "%SRC_DIR%\urh_vulkan_runtime_probe.cpp"
    "%SRC_DIR%\urh_vulkan_layer_state.cpp"
    "%SRC_DIR%\urh_vulkan_layer_instance.cpp"
    "%SRC_DIR%\urh_vulkan_layer_runtime.cpp"
    "%SRC_DIR%\urh_vulkan_layer_exports.cpp"
) do (
    set "BASENAME=%%~nF"
    echo   Compiling %%~nxF...
    cl %COMMON_FLAGS% %INCLUDES% %DEFINES% /Fo"%OUT_DIR%\%%~nF.obj" "%%F"
    if errorlevel 1 (
        echo [ERROR] Failed to compile %%~nxF
        exit /b 1
    )
    set "OBJ_LIST=!OBJ_LIST! "%OUT_DIR%\%%~nF.obj""
)

echo.
echo === Linking static library ===

lib /NOLOGO /OUT:"%LIB_DIR%\URH.lib" "%OUT_DIR%\*.obj"
if errorlevel 1 (
    echo [ERROR] lib failed
    exit /b 1
)

echo.
echo === Success: %LIB_DIR%\URH.lib ===

endlocal
