#include "urh_dx12_internal.h"

namespace UrhDx12HookInternal
{
    namespace
    {
        LONG g_presentLogCount = 0;
    }

    void RenderFrame(IDXGISwapChain* swapChain, ID3D12CommandQueue* renderQueue)
    {
        UNREFERENCED_PARAMETER(swapChain);
        UNREFERENCED_PARAMETER(renderQueue);
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
                g_state.pendingQueue.load(std::memory_order_relaxed),
                g_state.trackedSwapChain);
        }

        if (g_state.unloading.load(std::memory_order_relaxed) || g_state.deviceLost || g_state.suspendRendering.load(std::memory_order_relaxed))
        {
            DWORD exceptionCode = 0;
            HRESULT hr = CallOriginalPresentSafe(swapChain, syncInterval, flags, &exceptionCode);
            if (exceptionCode)
            {
                g_state.deviceLost = true;
            }

            return hr;
        }

        g_state.presentInFlight.fetch_add(1);

        if (g_state.suspendRendering.load(std::memory_order_relaxed))
        {
            g_state.presentInFlight.fetch_sub(1);
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
            g_state.presentInFlight.fetch_sub(1);
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
                g_state.pendingQueue.exchange(nullptr));
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
            if (g_state.commandListRecording.load(std::memory_order_relaxed) && g_state.commandList)
            {
                __try
                {
                    g_state.commandList->Close();
                }
                __except (EXCEPTION_EXECUTE_HANDLER)
                {
                }

                g_state.commandListRecording.store(false);
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

        g_state.presentInFlight.fetch_sub(1);
        return hr;
    }

}
