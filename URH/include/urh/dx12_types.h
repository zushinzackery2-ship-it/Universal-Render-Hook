#pragma once

#include <d3d12.h>
#include <dxgi1_4.h>

struct UrhDx12HookRuntime;

using UrhDx12HookSetupCallback = void (*)(
    const UrhDx12HookRuntime* runtime,
    void* userData);

using UrhDx12HookRenderCallback = void (*)(
    const UrhDx12HookRuntime* runtime,
    void* userData);

using UrhDx12HookVisibleCallback = bool (*)(
    void* userData);

using UrhDx12HookShutdownCallback = void (*)(
    void* userData);

struct UrhDx12HookRuntime
{
    void* hwnd;
    IDXGISwapChain* swapChain;
    ID3D12Device* device;
    ID3D12CommandQueue* commandQueue;
    UINT bufferCount;
    UINT backBufferIndex;
    DXGI_FORMAT backBufferFormat;
    float width;
    float height;
    UINT frameCount;
};

struct UrhDx12HookDesc
{
    UrhDx12HookSetupCallback onSetup;
    UrhDx12HookRenderCallback onRender;
    UrhDx12HookVisibleCallback isVisible;
    UrhDx12HookShutdownCallback onShutdown;
    void* userData;

    bool autoCreateContext;
    bool hookWndProc;
    bool blockInputWhenVisible;
    bool enableDefaultDebugWindow;

    UINT warmupFrames;
    UINT fenceWaitTimeoutMs;
    UINT shutdownWaitTimeoutMs;
    UINT toggleVirtualKey;
    bool startVisible;
};
