#pragma once

#include <Windows.h>
#include <d3d11.h>
#include <d3d11_4.h>
#include <dxgi1_4.h>

#include "urh_dx11_types.h"
#include "urh_dx11_hook.h"

#if __has_include("urh_console_logger.h")
#include "urh_console_logger.h"
#define URH_DX11HOOK_LOG(...) ConsoleLogger::Log(__VA_ARGS__)
#else
#define URH_DX11HOOK_LOG(...) do { } while (0)
#endif

namespace UrhDx11HookInternal
{
    static constexpr UINT MaxBackBuffers = 8;

    typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags);

    struct Dx11HookProbeData
    {
        void** swapChainVtable;
    };

    struct ModuleState
    {
        UrhDx11HookDesc desc;
        UrhDx11HookRuntime runtime;
        Dx11HookProbeData probe;

        PresentFn originalPresent;

        ID3D11Device* device;
        ID3D11DeviceContext* deviceContext;
        ID3D11RenderTargetView* renderTargetViews[MaxBackBuffers];
        ID3D11Multithread* multithread;
        UINT bufferCount;
        DXGI_FORMAT backBufferFormat;
        UINT trackedWidth;
        UINT trackedHeight;

        HWND gameWindow;
        WNDPROC originalWndProc;
        IDXGISwapChain* trackedSwapChain;

        bool setupCalled;
        bool installed;
        bool backendReady;
        bool deviceLost;
        volatile bool unloading;
        volatile bool suspendRendering;
        volatile LONG presentInFlight;

        CRITICAL_SECTION renderCs;
        bool renderCsReady;

        UINT frameCount;
    };

    extern ModuleState g_state;

    void ResetRuntime();
    void FillDefaultDesc(UrhDx11HookDesc& desc);
    bool NeedsUrhBackend();

    bool ProbeVtables(Dx11HookProbeData& probeData);

    bool PatchVtable(
        void** vtable,
        int index,
        void* hookFn,
        void** originalFn);

    bool RestoreVtable(
        void** vtable,
        int index,
        void* originalFn);

    bool InstallHooks();
    void UninstallHooks();

    void UpdateRuntimeSnapshot(IDXGISwapChain* swapChain);
    bool CreateRenderResources(IDXGISwapChain* swapChain);
    bool EnsureRenderTargetView(IDXGISwapChain* swapChain, UINT bufferIndex);
    void CleanupRenderResources();
    bool InitializeBackends(IDXGISwapChain* swapChain);
    void ShutdownBackends(bool finalShutdown);

    bool IsInteractiveVisible();
    void UpdateDefaultDebugState();
    void RenderDebugUi();

    PresentFn ResolvePresentFn(IDXGISwapChain* swapChain);

    HRESULT CallOriginalPresentSafe(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags,
        DWORD* exceptionCode = nullptr);

    void RenderFrame(IDXGISwapChain* swapChain);
    void ProcessPresentFrame(IDXGISwapChain* swapChain);

    LRESULT CALLBACK HookedWndProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

    HRESULT STDMETHODCALLTYPE HookPresent(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags);
}
