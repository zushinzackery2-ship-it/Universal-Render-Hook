#include "urh_dx12_internal.h"

namespace UrhDx12HookInternal
{
    namespace
    {
        LONG g_presentLogCount = 0;
    }

    void RenderFrame(IDXGISwapChain* swapChain, ID3D12CommandQueue* renderQueue)
    {
        if (!g_state.backendReady || g_state.deviceLost)
        {
            return;
        }

        if (g_state.desc.onRender)
        {
            g_state.desc.onRender(&g_state.runtime, g_state.desc.userData);
        }
    }

    HRESULT STDMETHODCALLTYPE HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
    {
        if (InterlockedIncrement(&g_presentLogCount) <= 8)
        {
            URH_DX12HOOK_LOG(
                "HookPresent enter: swapChain=%p sync=%u flags=0x%X backendReady=%d queue=%p pendingQueue=%p trackedSwapChain=%p",
                swapChain,
                syncInterval,
                flags,
                g_state.backendReady ? 1 : 0,
                g_state.commandQueue,
                g_state.pendingQueue,
                g_state.trackedSwapChain);
        }

        if (g_state.unloading || g_state.deviceLost || g_state.suspendRendering)
        {
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresentSafe(swapChain, syncInterval, flags, &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        InterlockedIncrement(&g_state.presentInFlight);

        if (g_state.suspendRendering)
        {
            InterlockedDecrement(&g_state.presentInFlight);
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresentSafe(swapChain, syncInterval, flags, &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        if ((flags & DXGI_PRESENT_TEST) || (g_state.gameWindow && IsIconic(g_state.gameWindow)))
        {
            InterlockedDecrement(&g_state.presentInFlight);
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresentSafe(swapChain, syncInterval, flags, &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        if (g_state.renderCsReady)
        {
            EnterCriticalSection(&g_state.renderCs);
        }

        __try
        {
            auto* pendingQueue = reinterpret_cast<ID3D12CommandQueue*>(
                InterlockedExchangePointer(reinterpret_cast<volatile PVOID*>(&g_state.pendingQueue), nullptr));
            if (pendingQueue)
            {
                if (g_state.commandQueue != pendingQueue)
                {
                    if (g_state.commandQueue)
                    {
                        g_state.commandQueue->Release();
                    }

                    g_state.commandQueue = pendingQueue;
                    URH_DX12HOOK_LOG("HookPresent adopted pendingQueue=%p", g_state.commandQueue);
                }
                else
                {
                    pendingQueue->Release();
                }
            }

            if (g_state.backendReady && g_state.trackedSwapChain != swapChain)
            {
                URH_DX12HOOK_LOG(
                    "HookPresent swapChain changed: old=%p new=%p, resetting backends",
                    g_state.trackedSwapChain,
                    swapChain);
                ShutdownBackends(false);
                g_state.frameCount = 0;
            }

            UpdateRuntimeSnapshot(swapChain);

            if (!g_state.backendReady && g_state.commandQueue)
            {
                const bool resourcesOk = CreateRenderResources(swapChain);
                const bool backendOk = resourcesOk && InitializeBackends(swapChain);
                if (backendOk)
                {
                    TryHookResizeBuffers1(swapChain);
                }
                else
                {
                    URH_DX12HOOK_LOG(
                        "HookPresent backend init path failed: resourcesOk=%d backendOk=%d queue=%p swapChain=%p",
                        resourcesOk ? 1 : 0,
                        backendOk ? 1 : 0,
                        g_state.commandQueue,
                        swapChain);
                    ShutdownBackends(false);
                }
            }

            if (g_state.backendReady && !g_state.deviceLost)
            {
                ++g_state.frameCount;
                UpdateRuntimeSnapshot(swapChain);

                if (g_state.frameCount > g_state.desc.warmupFrames)
                {
                    if (g_state.device && g_state.device->GetDeviceRemovedReason() != S_OK)
                    {
                        g_state.deviceLost = true;
                    }

                    if (!g_state.deviceLost)
                    {
                        RenderFrame(swapChain, g_state.commandQueue);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (g_state.commandListRecording && g_state.commandList)
            {
                __try
                {
                    g_state.commandList->Close();
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }

                g_state.commandListRecording = false;
            }

            g_state.deviceLost = true;
        }

        if (g_state.renderCsReady)
        {
            LeaveCriticalSection(&g_state.renderCs);
        }

        DWORD exceptionCode = 0;
        HRESULT hr = CallOriginalPresentSafe(swapChain, syncInterval, flags, &exceptionCode);
        if (exceptionCode || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            g_state.deviceLost = true;
        }

        InterlockedDecrement(&g_state.presentInFlight);
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookPresent1(
        IDXGISwapChain1* swapChain,
        UINT syncInterval,
        UINT flags,
        const DXGI_PRESENT_PARAMETERS* presentParameters)
    {
        if (InterlockedIncrement(&g_presentLogCount) <= 8)
        {
            URH_DX12HOOK_LOG(
                "HookPresent1 enter: swapChain=%p sync=%u flags=0x%X backendReady=%d queue=%p pendingQueue=%p trackedSwapChain=%p",
                swapChain,
                syncInterval,
                flags,
                g_state.backendReady ? 1 : 0,
                g_state.commandQueue,
                g_state.pendingQueue,
                g_state.trackedSwapChain);
        }

        IDXGISwapChain* baseSwapChain = swapChain;
        if (g_state.unloading || g_state.deviceLost || g_state.suspendRendering)
        {
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresent1Safe(
                swapChain,
                syncInterval,
                flags,
                presentParameters,
                &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        InterlockedIncrement(&g_state.presentInFlight);

        if (g_state.suspendRendering)
        {
            InterlockedDecrement(&g_state.presentInFlight);
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresent1Safe(
                swapChain,
                syncInterval,
                flags,
                presentParameters,
                &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        if ((flags & DXGI_PRESENT_TEST) || (g_state.gameWindow && IsIconic(g_state.gameWindow)))
        {
            InterlockedDecrement(&g_state.presentInFlight);
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresent1Safe(
                swapChain,
                syncInterval,
                flags,
                presentParameters,
                &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        if (g_state.renderCsReady)
        {
            EnterCriticalSection(&g_state.renderCs);
        }

        __try
        {
            auto* pendingQueue = reinterpret_cast<ID3D12CommandQueue*>(
                InterlockedExchangePointer(reinterpret_cast<volatile PVOID*>(&g_state.pendingQueue), nullptr));
            if (pendingQueue)
            {
                if (g_state.commandQueue != pendingQueue)
                {
                    if (g_state.commandQueue)
                    {
                        g_state.commandQueue->Release();
                    }

                    g_state.commandQueue = pendingQueue;
                    URH_DX12HOOK_LOG("HookPresent1 adopted pendingQueue=%p", g_state.commandQueue);
                }
                else
                {
                    pendingQueue->Release();
                }
            }

            if (g_state.backendReady && g_state.trackedSwapChain != baseSwapChain)
            {
                URH_DX12HOOK_LOG(
                    "HookPresent1 swapChain changed: old=%p new=%p, resetting backends",
                    g_state.trackedSwapChain,
                    baseSwapChain);
                ShutdownBackends(false);
                g_state.frameCount = 0;
            }

            UpdateRuntimeSnapshot(baseSwapChain);

            if (!g_state.backendReady && g_state.commandQueue)
            {
                const bool resourcesOk = CreateRenderResources(baseSwapChain);
                const bool backendOk = resourcesOk && InitializeBackends(baseSwapChain);
                if (backendOk)
                {
                    TryHookResizeBuffers1(baseSwapChain);
                }
                else
                {
                    URH_DX12HOOK_LOG(
                        "HookPresent1 backend init path failed: resourcesOk=%d backendOk=%d queue=%p swapChain=%p",
                        resourcesOk ? 1 : 0,
                        backendOk ? 1 : 0,
                        g_state.commandQueue,
                        baseSwapChain);
                    ShutdownBackends(false);
                }
            }

            if (g_state.backendReady && !g_state.deviceLost)
            {
                ++g_state.frameCount;
                UpdateRuntimeSnapshot(baseSwapChain);

                if (g_state.frameCount > g_state.desc.warmupFrames)
                {
                    if (g_state.device && g_state.device->GetDeviceRemovedReason() != S_OK)
                    {
                        g_state.deviceLost = true;
                    }

                    if (!g_state.deviceLost)
                    {
                        RenderFrame(baseSwapChain, g_state.commandQueue);
                    }
                }
            }
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (g_state.commandListRecording && g_state.commandList)
            {
                __try
                {
                    g_state.commandList->Close();
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }

                g_state.commandListRecording = false;
            }

            g_state.deviceLost = true;
        }

        if (g_state.renderCsReady)
        {
            LeaveCriticalSection(&g_state.renderCs);
        }

        DWORD exceptionCode = 0;
        HRESULT hr = CallOriginalPresent1Safe(
            swapChain,
            syncInterval,
            flags,
            presentParameters,
            &exceptionCode);
        if (exceptionCode || hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
        {
            g_state.deviceLost = true;
        }

        InterlockedDecrement(&g_state.presentInFlight);
        return hr;
    }
}
