#pragma once

#include <d3d11.h>
#include <dxgi1_4.h>

struct UrhDx11HookRuntime;

using UrhDx11HookSetupCallback = void (*)(
    const UrhDx11HookRuntime* runtime,
    void* userData);

using UrhDx11HookRenderCallback = void (*)(
    const UrhDx11HookRuntime* runtime,
    void* userData);

using UrhDx11HookVisibleCallback = bool (*)(
    void* userData);

using UrhDx11HookShutdownCallback = void (*)(
    void* userData);

struct UrhDx11HookRuntime
{
    void* hwnd;
    IDXGISwapChain* swapChain;
    ID3D11Device* device;
    ID3D11DeviceContext* deviceContext;
    UINT bufferCount;
    UINT backBufferIndex;
    DXGI_FORMAT backBufferFormat;
    float width;
    float height;
    UINT frameCount;
};

struct UrhDx11HookDesc
{
    UrhDx11HookSetupCallback onSetup;
    UrhDx11HookRenderCallback onRender;
    UrhDx11HookVisibleCallback isVisible;
    UrhDx11HookShutdownCallback onShutdown;
    void* userData;

    bool autoCreateContext;
    bool hookWndProc;
    bool blockInputWhenVisible;
    bool enableDefaultDebugWindow;

    UINT warmupFrames;
    UINT shutdownWaitTimeoutMs;
    UINT toggleVirtualKey;
    bool startVisible;
};
