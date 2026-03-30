#pragma once

#include <dxgi1_4.h>

enum UrhAutoHookBackend
{
    UrhAutoHookBackend_Unknown = 0,
    UrhAutoHookBackend_Dx11 = 1,
    UrhAutoHookBackend_Dx12 = 2,
    UrhAutoHookBackend_Vulkan = 3
};

enum UrhAutoHookBackendMask
{
    UrhAutoHookBackendMask_None = 0,
    UrhAutoHookBackendMask_Dx11 = 1 << 0,
    UrhAutoHookBackendMask_Dx12 = 1 << 1,
    UrhAutoHookBackendMask_Vulkan = 1 << 2,
    UrhAutoHookBackendMask_All =
        UrhAutoHookBackendMask_Dx11 |
        UrhAutoHookBackendMask_Dx12 |
        UrhAutoHookBackendMask_Vulkan
};

struct UrhAutoHookRuntime;

using UrhAutoHookSetupCallback = void (*)(
    const UrhAutoHookRuntime* runtime,
    void* userData);

using UrhAutoHookRenderCallback = void (*)(
    const UrhAutoHookRuntime* runtime,
    void* userData);

using UrhAutoHookVisibleCallback = bool (*)(
    void* userData);

using UrhAutoHookShutdownCallback = void (*)(
    void* userData);

struct UrhAutoHookRuntime
{
    UrhAutoHookBackend backend;
    void* hwnd;
    void* nativeRuntime;
    UINT bufferCount;
    UINT backBufferIndex;
    DXGI_FORMAT backBufferFormat;
    float width;
    float height;
    UINT frameCount;
};

struct UrhAutoHookDiagnostics
{
    UrhAutoHookBackend lockedBackend;
    bool dx11Seen;
    UINT dx11StableFrames;
    float dx11Width;
    float dx11Height;
    bool dx12Seen;
    UINT dx12StableFrames;
    float dx12Width;
    float dx12Height;
    bool vulkanSeen;
    UINT vulkanStableFrames;
    float vulkanWidth;
    float vulkanHeight;
};

struct UrhAutoHookDesc
{
    UrhAutoHookSetupCallback onSetup;
    UrhAutoHookRenderCallback onRender;
    UrhAutoHookVisibleCallback isVisible;
    UrhAutoHookShutdownCallback onShutdown;
    void* userData;

    bool autoCreateContext;
    bool hookWndProc;
    bool blockInputWhenVisible;
    bool enableDefaultDebugWindow;
    UINT backendMask;

    UINT warmupFrames;
    UINT fenceWaitTimeoutMs;
    UINT shutdownWaitTimeoutMs;
    UINT toggleVirtualKey;
    bool startVisible;
};
