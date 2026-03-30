#include "urh_dx12_internal.h"

namespace UrhDx12HookInternal
{
    bool InitializeBackends(IDXGISwapChain* swapChain)
    {
        UpdateRuntimeSnapshot(swapChain);
        URH_DX12HOOK_LOG(
            "InitializeBackends begin: swapChain=%p hwnd=%p device=%p queue=%p buffers=%u format=%u",
            swapChain,
            g_state.gameWindow,
            g_state.device,
            g_state.commandQueue,
            g_state.bufferCount,
            static_cast<unsigned int>(g_state.backBufferFormat));

        if (!g_state.setupCalled && g_state.desc.onSetup)
        {
            g_state.desc.onSetup(&g_state.runtime, g_state.desc.userData);
        }

        if ((g_state.desc.hookWndProc || g_state.desc.blockInputWhenVisible) &&
            g_state.gameWindow)
        {
            WNDPROC currentProc = reinterpret_cast<WNDPROC>(
                GetWindowLongPtrW(g_state.gameWindow, GWLP_WNDPROC));
            if (currentProc != HookedWndProc)
            {
                g_state.originalWndProc = reinterpret_cast<WNDPROC>(
                    SetWindowLongPtrW(
                        g_state.gameWindow,
                        GWLP_WNDPROC,
                        reinterpret_cast<LONG_PTR>(HookedWndProc)));
            }
        }

        g_state.trackedSwapChain = swapChain;
        g_state.backendReady = true;
        g_state.deviceLost = false;
        URH_DX12HOOK_LOG(
            "InitializeBackends success: trackedSwapChain=%p hwnd=%p backendReady=%d",
            g_state.trackedSwapChain,
            g_state.gameWindow,
            g_state.backendReady ? 1 : 0);
        return true;
    }

    void ShutdownBackends(bool finalShutdown)
    {
        if (g_state.originalWndProc && g_state.gameWindow && IsWindow(g_state.gameWindow))
        {
            SetWindowLongPtrW(
                g_state.gameWindow,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(g_state.originalWndProc));
            g_state.originalWndProc = nullptr;
        }

        CleanupRenderResources();

        if (g_state.device)
        {
            g_state.device->Release();
            g_state.device = nullptr;
        }

        g_state.backendReady = false;
        g_state.trackedSwapChain = nullptr;
        g_state.runtime.swapChain = nullptr;
        g_state.runtime.device = nullptr;
        g_state.runtime.bufferCount = 0;

        if (!finalShutdown)
        {
            return;
        }

        if (g_state.desc.onShutdown)
        {
            g_state.desc.onShutdown(g_state.desc.userData);
        }

        if (g_state.commandQueue)
        {
            g_state.commandQueue->Release();
            g_state.commandQueue = nullptr;
        }

        auto* pendingQueue = reinterpret_cast<ID3D12CommandQueue*>(
            InterlockedExchangePointer(reinterpret_cast<volatile PVOID*>(&g_state.pendingQueue), nullptr));
        if (pendingQueue)
        {
            pendingQueue->Release();
        }

        g_state.setupCalled = false;
        g_state.gameWindow = nullptr;
        ResetRuntime();
    }
}
