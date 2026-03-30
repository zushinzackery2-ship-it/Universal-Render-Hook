#include "urh_dx12_internal.h"

namespace UrhDx12HookInternal
{
    void UpdateRuntimeSnapshot(IDXGISwapChain* swapChain)
    {
        g_state.runtime.hwnd = g_state.gameWindow;
        g_state.runtime.swapChain = swapChain;
        g_state.runtime.device = g_state.device;
        g_state.runtime.commandQueue = g_state.commandQueue;
        g_state.runtime.bufferCount = g_state.bufferCount;
        g_state.runtime.backBufferFormat = g_state.backBufferFormat;
        g_state.runtime.frameCount = g_state.frameCount;

        DXGI_SWAP_CHAIN_DESC desc = {};
        if (swapChain && SUCCEEDED(swapChain->GetDesc(&desc)))
        {
            g_state.gameWindow = desc.OutputWindow;
            g_state.runtime.hwnd = desc.OutputWindow;
            g_state.runtime.width = static_cast<float>(desc.BufferDesc.Width);
            g_state.runtime.height = static_cast<float>(desc.BufferDesc.Height);
        }

        IDXGISwapChain3* swapChain3 = nullptr;
        if (swapChain && SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
        {
            g_state.runtime.backBufferIndex = swapChain3->GetCurrentBackBufferIndex();
            swapChain3->Release();
        }
    }

    bool CreateRenderResources(IDXGISwapChain* swapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc = {};
        if (!swapChain || FAILED(swapChain->GetDesc(&desc)))
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: GetDesc swapChain=%p", swapChain);
            return false;
        }

        g_state.bufferCount = desc.BufferCount;
        if (g_state.bufferCount == 0)
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: bufferCount=0");
            return false;
        }

        if (g_state.bufferCount > MaxBackBuffers)
        {
            g_state.bufferCount = MaxBackBuffers;
        }

        g_state.backBufferFormat = desc.BufferDesc.Format;
        if (g_state.backBufferFormat == DXGI_FORMAT_UNKNOWN)
        {
            g_state.backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        }

        if (!g_state.device && FAILED(swapChain->GetDevice(IID_PPV_ARGS(&g_state.device))))
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: GetDevice");
            return false;
        }

        if (!NeedsUrhBackend())
        {
            URH_DX12HOOK_LOG(
                "CreateRenderResources headless success: swapChain=%p device=%p buffers=%u format=%u",
                swapChain,
                g_state.device,
                g_state.bufferCount,
                static_cast<unsigned int>(g_state.backBufferFormat));
            return true;
        }

        D3D12_DESCRIPTOR_HEAP_DESC rtvDesc = {};
        rtvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
        rtvDesc.NumDescriptors = g_state.bufferCount;
        if (FAILED(g_state.device->CreateDescriptorHeap(&rtvDesc, IID_PPV_ARGS(&g_state.rtvHeap))))
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: CreateDescriptorHeap RTV");
            return false;
        }

        g_state.rtvDescriptorSize = g_state.device->GetDescriptorHandleIncrementSize(
            D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

        D3D12_DESCRIPTOR_HEAP_DESC srvDesc = {};
        srvDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
        srvDesc.NumDescriptors = 1;
        srvDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
        if (FAILED(g_state.device->CreateDescriptorHeap(&srvDesc, IID_PPV_ARGS(&g_state.srvHeap))))
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: CreateDescriptorHeap SRV");
            return false;
        }

        D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = g_state.rtvHeap->GetCPUDescriptorHandleForHeapStart();
        for (UINT index = 0; index < g_state.bufferCount; ++index)
        {
            if (FAILED(g_state.device->CreateCommandAllocator(
                    D3D12_COMMAND_LIST_TYPE_DIRECT,
                    IID_PPV_ARGS(&g_state.commandAllocators[index]))))
            {
                URH_DX12HOOK_LOG("CreateRenderResources failed: CreateCommandAllocator index=%u", index);
                return false;
            }

            if (FAILED(swapChain->GetBuffer(index, IID_PPV_ARGS(&g_state.backBuffers[index]))))
            {
                URH_DX12HOOK_LOG("CreateRenderResources failed: GetBuffer index=%u", index);
                return false;
            }

            g_state.rtvHandles[index] = rtvHandle;
            g_state.device->CreateRenderTargetView(g_state.backBuffers[index], nullptr, rtvHandle);
            rtvHandle.ptr += g_state.rtvDescriptorSize;
        }

        if (FAILED(g_state.device->CreateCommandList(
                0,
                D3D12_COMMAND_LIST_TYPE_DIRECT,
                g_state.commandAllocators[0],
                nullptr,
                IID_PPV_ARGS(&g_state.commandList))))
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: CreateCommandList");
            return false;
        }

        g_state.commandList->Close();

        if (FAILED(g_state.device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&g_state.fence))))
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: CreateFence");
            return false;
        }

        g_state.fenceEvent = CreateEventW(nullptr, FALSE, FALSE, nullptr);
        if (!g_state.fenceEvent)
        {
            URH_DX12HOOK_LOG("CreateRenderResources failed: CreateEventW");
            return false;
        }

        g_state.fenceCounter = 0;
        for (UINT index = 0; index < g_state.bufferCount; ++index)
        {
            g_state.fenceValues[index] = 0;
        }

        URH_DX12HOOK_LOG(
            "CreateRenderResources success: swapChain=%p device=%p buffers=%u format=%u",
            swapChain,
            g_state.device,
            g_state.bufferCount,
            static_cast<unsigned int>(g_state.backBufferFormat));
        return true;
    }

    void CleanupRenderResources()
    {
        if (g_state.fence && g_state.commandQueue && g_state.fenceEvent)
        {
            UINT64 waitValue = ++g_state.fenceCounter;
            DWORD waitMs = g_state.desc.shutdownWaitTimeoutMs;
            __try
            {
                g_state.commandQueue->Signal(g_state.fence, waitValue);
                if (g_state.fence->GetCompletedValue() < waitValue)
                {
                    g_state.fence->SetEventOnCompletion(waitValue, g_state.fenceEvent);
                    WaitForSingleObject(g_state.fenceEvent, waitMs);
                }
            }
            __except (EXCEPTION_EXECUTE_HANDLER)
            {
            }
        }

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

        for (UINT index = 0; index < MaxBackBuffers; ++index)
        {
            if (g_state.backBuffers[index])
            {
                g_state.backBuffers[index]->Release();
                g_state.backBuffers[index] = nullptr;
            }

            if (g_state.commandAllocators[index])
            {
                g_state.commandAllocators[index]->Release();
                g_state.commandAllocators[index] = nullptr;
            }

            g_state.fenceValues[index] = 0;
        }

        if (g_state.commandList)
        {
            g_state.commandList->Release();
            g_state.commandList = nullptr;
        }

        if (g_state.rtvHeap)
        {
            g_state.rtvHeap->Release();
            g_state.rtvHeap = nullptr;
        }

        if (g_state.srvHeap)
        {
            g_state.srvHeap->Release();
            g_state.srvHeap = nullptr;
        }

        if (g_state.fence)
        {
            g_state.fence->Release();
            g_state.fence = nullptr;
        }

        if (g_state.fenceEvent)
        {
            CloseHandle(g_state.fenceEvent);
            g_state.fenceEvent = nullptr;
        }

        g_state.bufferCount = 0;
        g_state.fenceCounter = 0;
    }

}
