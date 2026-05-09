#include "urh_dx11_internal.h"
#include "urh_dx_common.h"

namespace UrhDx11HookInternal
{
    bool ProbeVtables(Dx11HookProbeData& probeData)
    {
        probeData.swapChainVtable = nullptr;

        WNDCLASSEXW windowClass = {};
        HWND windowHandle = nullptr;
        if (!UrhDxCommon::CreateProbeWindow(L"UrhDx11HookProbeWindow", L"UrhDx11HookProbe", windowClass, windowHandle))
        {
            return false;
        }

        HMODULE d3d11Module = LoadLibraryW(L"d3d11.dll");
        if (!d3d11Module)
        {
            URH_DX11HOOK_LOG("ProbeVtables failed: LoadLibraryW(d3d11.dll) error=%lu", GetLastError());
            UrhDxCommon::DestroyProbeWindow(windowClass, windowHandle);
            return false;
        }

        auto d3d11CreateDeviceAndSwapChain = reinterpret_cast<decltype(&D3D11CreateDeviceAndSwapChain)>(
            GetProcAddress(d3d11Module, "D3D11CreateDeviceAndSwapChain"));
        if (!d3d11CreateDeviceAndSwapChain)
        {
            URH_DX11HOOK_LOG("ProbeVtables failed: GetProcAddress(D3D11CreateDeviceAndSwapChain)");
            UrhDxCommon::DestroyProbeWindow(windowClass, windowHandle);
            return false;
        }

        IDXGISwapChain* swapChain = nullptr;
        ID3D11Device* device = nullptr;
        ID3D11DeviceContext* deviceContext = nullptr;
        bool success = false;

        const D3D_DRIVER_TYPE driverTypes[] =
        {
            D3D_DRIVER_TYPE_HARDWARE,
            D3D_DRIVER_TYPE_WARP
        };

        for (D3D_DRIVER_TYPE driverType : driverTypes)
        {
            DXGI_SWAP_CHAIN_DESC swapChainDesc = {};
            swapChainDesc.BufferCount = 2;
            swapChainDesc.BufferDesc.Width = 100;
            swapChainDesc.BufferDesc.Height = 100;
            swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
            swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;
            swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;
            swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
            swapChainDesc.OutputWindow = windowHandle;
            swapChainDesc.SampleDesc.Count = 1;
            swapChainDesc.Windowed = TRUE;
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;

            HRESULT hr = d3d11CreateDeviceAndSwapChain(
                nullptr,
                driverType,
                nullptr,
                0,
                nullptr,
                0,
                D3D11_SDK_VERSION,
                &swapChainDesc,
                &swapChain,
                &device,
                nullptr,
                &deviceContext);
            if (FAILED(hr))
            {
                URH_DX11HOOK_LOG(
                    "ProbeVtables D3D11CreateDeviceAndSwapChain failed. driverType=%u hr=0x%08X",
                    static_cast<unsigned int>(driverType),
                    static_cast<unsigned int>(hr));
                continue;
            }

            probeData.swapChainVtable = *reinterpret_cast<void***>(swapChain);
            success = probeData.swapChainVtable != nullptr;
            URH_DX11HOOK_LOG(
                "ProbeVtables captured swapChain=%p vtable=%p driverType=%u",
                swapChain,
                probeData.swapChainVtable,
                static_cast<unsigned int>(driverType));
            break;
        }

        if (deviceContext)
        {
            deviceContext->Release();
        }

        if (device)
        {
            device->Release();
        }

        if (swapChain)
        {
            swapChain->Release();
        }

        UrhDxCommon::DestroyProbeWindow(windowClass, windowHandle);
        if (!success)
        {
            URH_DX11HOOK_LOG("ProbeVtables finished without a usable swapchain vtable.");
        }
        return success;
    }
}
