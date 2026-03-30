#include "urh_dx12_internal.h"

namespace
{
    void BeginResizeReset()
    {
        using namespace UrhDx12HookInternal;

        g_state.suspendRendering = true;
        if (g_state.renderCsReady)
        {
            EnterCriticalSection(&g_state.renderCs);
        }

        ShutdownBackends(false);
        g_state.frameCount = 0;

        if (g_state.renderCsReady)
        {
            LeaveCriticalSection(&g_state.renderCs);
        }
    }
}

namespace UrhDx12HookInternal
{
    namespace
    {
        LONG g_queueCaptureLogCount = 0;
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers(
        IDXGISwapChain* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags)
    {
        BeginResizeReset();

        HRESULT hr = E_FAIL;
        if (g_state.originalResizeBuffers)
        {
            hr = g_state.originalResizeBuffers(
                swapChain,
                bufferCount,
                width,
                height,
                newFormat,
                swapChainFlags);
        }

        g_state.suspendRendering = false;
        return hr;
    }

    HRESULT STDMETHODCALLTYPE HookResizeBuffers1(
        IDXGISwapChain3* swapChain,
        UINT bufferCount,
        UINT width,
        UINT height,
        DXGI_FORMAT newFormat,
        UINT swapChainFlags,
        const UINT* creationNodeMask,
        IUnknown* const* presentQueue)
    {
        BeginResizeReset();

        HRESULT hr = E_FAIL;
        if (g_state.originalResizeBuffers1)
        {
            hr = g_state.originalResizeBuffers1(
                swapChain,
                bufferCount,
                width,
                height,
                newFormat,
                swapChainFlags,
                creationNodeMask,
                presentQueue);
        }

        g_state.suspendRendering = false;
        return hr;
    }

    void STDMETHODCALLTYPE HookExecuteCommandLists(
        ID3D12CommandQueue* queue,
        UINT numCommandLists,
        ID3D12CommandList* const* commandLists)
    {
        if (!g_state.unloading && !g_state.deviceLost)
        {
            __try
            {
                D3D12_COMMAND_QUEUE_DESC desc = queue->GetDesc();
                if (desc.Type == D3D12_COMMAND_LIST_TYPE_DIRECT)
                {
                    if (InterlockedIncrement(&g_queueCaptureLogCount) <= 8)
                    {
                        URH_DX12HOOK_LOG(
                            "HookExecuteCommandLists captured DIRECT queue=%p numCommandLists=%u",
                            queue,
                            numCommandLists);
                    }
                    queue->AddRef();
                    auto* oldQueue = reinterpret_cast<ID3D12CommandQueue*>(
                        InterlockedExchangePointer(
                            reinterpret_cast<volatile PVOID*>(&g_state.pendingQueue),
                            queue));
                    if (oldQueue)
                    {
                        oldQueue->Release();
                    }
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
                g_state.deviceLost = true;
            }
        }

        if (g_state.originalExecuteCommandLists)
        {
            g_state.originalExecuteCommandLists(queue, numCommandLists, commandLists);
        }
    }
}
