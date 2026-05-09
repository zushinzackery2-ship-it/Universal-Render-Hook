#include "urh_dx11_internal.h"

namespace UrhDx11HookInternal
{
    bool InitializeBackends(IDXGISwapChain* swapChain)
    {
        UpdateRuntimeSnapshot(swapChain);
        URH_DX11HOOK_LOG(
            "InitializeBackends start. swapChain=%p hwnd=%p device=%p context=%p size=%.0fx%.0f",
            swapChain,
            g_state.gameWindow,
            g_state.device,
            g_state.deviceContext,
            g_state.runtime.width,
            g_state.runtime.height);

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
        URH_DX11HOOK_LOG(
            "InitializeBackends succeeded. trackedSwapChain=%p hwnd=%p originalWndProc=%p",
            g_state.trackedSwapChain,
            g_state.gameWindow,
            reinterpret_cast<void*>(g_state.originalWndProc));
        return true;
    }

    void ShutdownBackends(bool finalShutdown)
    {
        URH_DX11HOOK_LOG(
            "ShutdownBackends called. finalShutdown=%d backendReady=%d context=%p hwnd=%p",
            finalShutdown ? 1 : 0,
            g_state.backendReady ? 1 : 0,
            nullptr,
            g_state.gameWindow);

        if (g_state.originalWndProc && g_state.gameWindow && IsWindow(g_state.gameWindow))
        {
            SetWindowLongPtrW(
                g_state.gameWindow,
                GWLP_WNDPROC,
                reinterpret_cast<LONG_PTR>(g_state.originalWndProc));
            g_state.originalWndProc = nullptr;
        }

        CleanupRenderResources();

        g_state.backendReady = false;
        g_state.trackedSwapChain = nullptr;
        g_state.runtime.swapChain = nullptr;
        g_state.runtime.device = nullptr;
        g_state.runtime.deviceContext = nullptr;
        g_state.runtime.bufferCount = 0;

        if (!finalShutdown)
        {
            return;
        }

        if (g_state.desc.onShutdown)
        {
            g_state.desc.onShutdown(g_state.desc.userData);
        }

        g_state.setupCalled = false;
        g_state.gameWindow = nullptr;
        ResetRuntime();
    }
}
