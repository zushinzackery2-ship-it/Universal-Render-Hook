#pragma once

#include <Windows.h>
#include <d3d12.h>
#include <dxgi1_4.h>

#include "urh_dx12_types.h"
#include "urh_dx12_hook.h"

#if __has_include("urh_console_logger.h")
#include "urh_console_logger.h"
#define URH_DX12HOOK_LOG(...) ConsoleLogger::Log(__VA_ARGS__)
#else
#define URH_DX12HOOK_LOG(...) do { } while (0)
#endif

namespace UrhDx12HookInternal
{
    static constexpr UINT MaxBackBuffers = 8;

    typedef HRESULT(STDMETHODCALLTYPE* PresentFn)(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags);

    typedef HRESULT(STDMETHODCALLTYPE* Present1Fn)(
        IDXGISwapChain1* swapChain,
        UINT syncInterval,
        UINT flags,
        const DXGI_PRESENT_PARAMETERS* presentParameters);

    typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffersFn)(
        IDXGISwapChain* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags);

    typedef HRESULT(STDMETHODCALLTYPE* ResizeBuffers1Fn)(
        IDXGISwapChain3* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags,
        const UINT* creationNodeMask,
        IUnknown* const* presentQueue);

    typedef void(STDMETHODCALLTYPE* ExecuteCommandListsFn)(
        ID3D12CommandQueue* queue,
        UINT numCommandLists,
        ID3D12CommandList* const* commandLists);

    struct Dx12HookProbeData
    {
        void** commandQueueVtable;
        void** swapChainVtable;
    };

    struct ModuleState
    {
        UrhDx12HookDesc desc;
        UrhDx12HookRuntime runtime;
        Dx12HookProbeData probe;

        PresentFn originalPresent;
        Present1Fn originalPresent1;
        ResizeBuffersFn originalResizeBuffers;
        ResizeBuffers1Fn originalResizeBuffers1;
        ExecuteCommandListsFn originalExecuteCommandLists;

        ID3D12Device* device;
        ID3D12CommandQueue* commandQueue;
        ID3D12CommandQueue* pendingQueue;
        ID3D12DescriptorHeap* rtvHeap;
        ID3D12DescriptorHeap* srvHeap;
        ID3D12CommandAllocator* commandAllocators[MaxBackBuffers];
        ID3D12GraphicsCommandList* commandList;
        ID3D12Resource* backBuffers[MaxBackBuffers];
        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandles[MaxBackBuffers];
        UINT bufferCount;
        UINT rtvDescriptorSize;

        ID3D12Fence* fence;
        HANDLE fenceEvent;
        UINT64 fenceValues[MaxBackBuffers];
        UINT64 fenceCounter;

        HWND gameWindow;
        WNDPROC originalWndProc;
        IDXGISwapChain* trackedSwapChain;
        void** trackedSwapChainVtable;
        DXGI_FORMAT backBufferFormat;

        bool setupCalled;
        bool installed;
        bool backendReady;
        bool deviceLost;
        volatile LONG bootstrapRequested;
        volatile bool unloading;
        volatile bool suspendRendering;
        volatile bool commandListRecording;
        volatile LONG presentInFlight;

        CRITICAL_SECTION renderCs;
        bool renderCsReady;

        UINT frameCount;
    };

    extern ModuleState g_state;

    void ResetRuntime();
    void FillDefaultDesc(UrhDx12HookDesc& desc);
    bool NeedsUrhBackend();

    bool ProbeVtables(Dx12HookProbeData& probeData);

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
    void CleanupRenderResources();
    bool InitializeBackends(IDXGISwapChain* swapChain);
    void ShutdownBackends(bool finalShutdown);

    bool IsInteractiveVisible();
    void UpdateDefaultDebugState();
    void RenderDebugUi();
    void TryHookResizeBuffers1(IDXGISwapChain* swapChain);

    PresentFn ResolvePresentFn(IDXGISwapChain* swapChain);
    ExecuteCommandListsFn ResolveExecuteFn(ID3D12CommandQueue* queue);

    HRESULT CallOriginalPresentSafe(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags,
        DWORD* exceptionCode = nullptr);

    HRESULT CallOriginalPresent1Safe(
        IDXGISwapChain1* swapChain,
        UINT syncInterval,
        UINT flags,
        const DXGI_PRESENT_PARAMETERS* presentParameters,
        DWORD* exceptionCode = nullptr);

    bool CallOriginalExecuteSafe(
        ID3D12CommandQueue* queue,
        UINT numCommandLists,
        ID3D12CommandList* const* commandLists);

    void RenderFrame(
        IDXGISwapChain* swapChain,
        ID3D12CommandQueue* renderQueue);

    LRESULT CALLBACK HookedWndProc(
        HWND hwnd,
        UINT msg,
        WPARAM wParam,
        LPARAM lParam);

    HRESULT STDMETHODCALLTYPE HookPresent(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags);

    HRESULT STDMETHODCALLTYPE HookPresent1(
        IDXGISwapChain1* swapChain,
        UINT syncInterval,
        UINT flags,
        const DXGI_PRESENT_PARAMETERS* presentParameters);

    HRESULT STDMETHODCALLTYPE HookResizeBuffers(
        IDXGISwapChain* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags);

    HRESULT STDMETHODCALLTYPE HookResizeBuffers1(
        IDXGISwapChain3* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags,
        const UINT* creationNodeMask,
        IUnknown* const* presentQueue);

    void STDMETHODCALLTYPE HookExecuteCommandLists(
        ID3D12CommandQueue* queue,
        UINT numCommandLists,
        ID3D12CommandList* const* commandLists);
}
