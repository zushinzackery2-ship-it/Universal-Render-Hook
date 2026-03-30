#include "urh_dx11_internal.h"

namespace
{
    UINT GetCurrentBackBufferIndex(IDXGISwapChain* swapChain)
    {
        IDXGISwapChain3* swapChain3 = nullptr;
        if (swapChain && SUCCEEDED(swapChain->QueryInterface(IID_PPV_ARGS(&swapChain3))))
        {
            UINT index = swapChain3->GetCurrentBackBufferIndex();
            swapChain3->Release();
            return index;
        }

        return 0;
    }
}

namespace UrhDx11HookInternal
{
    void UpdateRuntimeSnapshot(IDXGISwapChain* swapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc = {};
        if (swapChain && SUCCEEDED(swapChain->GetDesc(&desc)))
        {
            g_state.gameWindow = desc.OutputWindow;
            g_state.runtime.hwnd = desc.OutputWindow;
            g_state.runtime.width = static_cast<float>(desc.BufferDesc.Width);
            g_state.runtime.height = static_cast<float>(desc.BufferDesc.Height);
            g_state.runtime.bufferCount = desc.BufferCount;
            g_state.runtime.backBufferFormat = desc.BufferDesc.Format;
        }
        else
        {
            g_state.runtime.hwnd = g_state.gameWindow;
            g_state.runtime.bufferCount = g_state.bufferCount;
            g_state.runtime.backBufferFormat = g_state.backBufferFormat;
        }

        g_state.runtime.swapChain = swapChain;
        g_state.runtime.device = g_state.device;
        g_state.runtime.deviceContext = g_state.deviceContext;
        g_state.runtime.frameCount = g_state.frameCount;
        g_state.runtime.backBufferIndex = GetCurrentBackBufferIndex(swapChain);
    }

    bool CreateRenderResources(IDXGISwapChain* swapChain)
    {
        DXGI_SWAP_CHAIN_DESC desc = {};
        if (!swapChain || FAILED(swapChain->GetDesc(&desc)))
        {
            URH_DX11HOOK_LOG("CreateRenderResources failed: swapChain=%p GetDesc failed.", swapChain);
            return false;
        }

        g_state.bufferCount = desc.BufferCount;
        if (g_state.bufferCount == 0)
        {
            URH_DX11HOOK_LOG("CreateRenderResources failed: bufferCount is zero.");
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

        g_state.trackedWidth = desc.BufferDesc.Width;
        g_state.trackedHeight = desc.BufferDesc.Height;

        if (FAILED(swapChain->GetDevice(IID_PPV_ARGS(&g_state.device))))
        {
            URH_DX11HOOK_LOG("CreateRenderResources failed: GetDevice failed.");
            return false;
        }

        g_state.device->GetImmediateContext(&g_state.deviceContext);
        if (!g_state.deviceContext)
        {
            URH_DX11HOOK_LOG("CreateRenderResources failed: GetImmediateContext returned null.");
            return false;
        }

        if (SUCCEEDED(g_state.deviceContext->QueryInterface(IID_PPV_ARGS(&g_state.multithread))) &&
            g_state.multithread)
        {
            g_state.multithread->SetMultithreadProtected(TRUE);
        }

        URH_DX11HOOK_LOG(
            "CreateRenderResources succeeded. swapChain=%p device=%p context=%p hwnd=%p buffers=%u size=%ux%u format=%u",
            swapChain,
            g_state.device,
            g_state.deviceContext,
            desc.OutputWindow,
            static_cast<unsigned int>(g_state.bufferCount),
            static_cast<unsigned int>(g_state.trackedWidth),
            static_cast<unsigned int>(g_state.trackedHeight),
            static_cast<unsigned int>(g_state.backBufferFormat));
        return true;
    }

    bool EnsureRenderTargetView(IDXGISwapChain* swapChain, UINT bufferIndex)
    {
        if (!swapChain || bufferIndex >= g_state.bufferCount)
        {
            return false;
        }

        if (g_state.renderTargetViews[bufferIndex])
        {
            return true;
        }

        ID3D11Texture2D* backBuffer = nullptr;
        if (FAILED(swapChain->GetBuffer(bufferIndex, IID_PPV_ARGS(&backBuffer))))
        {
            URH_DX11HOOK_LOG(
                "EnsureRenderTargetView failed: GetBuffer(%u) failed.",
                static_cast<unsigned int>(bufferIndex));
            return false;
        }

        D3D11_TEXTURE2D_DESC backBufferDesc = {};
        backBuffer->GetDesc(&backBufferDesc);

        HRESULT hr = g_state.device->CreateRenderTargetView(backBuffer, nullptr, &g_state.renderTargetViews[bufferIndex]);
        backBuffer->Release();
        if (FAILED(hr))
        {
            URH_DX11HOOK_LOG(
                "EnsureRenderTargetView failed: CreateRenderTargetView(%u) hr=0x%08X format=%u bind=0x%X misc=0x%X",
                static_cast<unsigned int>(bufferIndex),
                static_cast<unsigned int>(hr),
                static_cast<unsigned int>(backBufferDesc.Format),
                static_cast<unsigned int>(backBufferDesc.BindFlags),
                static_cast<unsigned int>(backBufferDesc.MiscFlags));
            return false;
        }

        URH_DX11HOOK_LOG(
            "EnsureRenderTargetView succeeded: index=%u rtv=%p format=%u bind=0x%X",
            static_cast<unsigned int>(bufferIndex),
            g_state.renderTargetViews[bufferIndex],
            static_cast<unsigned int>(backBufferDesc.Format),
            static_cast<unsigned int>(backBufferDesc.BindFlags));
        return true;
    }

    void CleanupRenderResources()
    {
        URH_DX11HOOK_LOG(
            "CleanupRenderResources called. device=%p context=%p trackedSwapChain=%p",
            g_state.device,
            g_state.deviceContext,
            g_state.trackedSwapChain);
        for (UINT index = 0; index < MaxBackBuffers; ++index)
        {
            if (g_state.renderTargetViews[index])
            {
                g_state.renderTargetViews[index]->Release();
                g_state.renderTargetViews[index] = nullptr;
            }
        }

        if (g_state.multithread)
        {
            g_state.multithread->Release();
            g_state.multithread = nullptr;
        }

        if (g_state.deviceContext)
        {
            g_state.deviceContext->Release();
            g_state.deviceContext = nullptr;
        }

        if (g_state.device)
        {
            g_state.device->Release();
            g_state.device = nullptr;
        }

        g_state.bufferCount = 0;
        g_state.backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g_state.trackedWidth = 0;
        g_state.trackedHeight = 0;
    }
}
