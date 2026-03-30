#include "urh_dx12_internal.h"

namespace
{
    static constexpr const wchar_t* ProbeWindowClassName = L"UrhDx12HookProbeWindow";

    bool CreateProbeWindow(WNDCLASSEXW& windowClass, HWND& windowHandle)
    {
        ZeroMemory(&windowClass, sizeof(windowClass));
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = ProbeWindowClassName;

        ATOM atom = RegisterClassExW(&windowClass);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }

        windowHandle = CreateWindowExW(
            0,
            ProbeWindowClassName,
            L"UrhDx12HookProbe",
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            windowClass.hInstance,
            nullptr);
        return windowHandle != nullptr;
    }

    void DestroyProbeWindow(const WNDCLASSEXW& windowClass, HWND& windowHandle)
    {
        if (windowHandle)
        {
            DestroyWindow(windowHandle);
            windowHandle = nullptr;
        }

        if (windowClass.lpszClassName)
        {
            UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        }
    }
}

namespace UrhDx12HookInternal
{
    bool ProbeVtables(Dx12HookProbeData& probeData)
    {
        probeData.commandQueueVtable = nullptr;
        probeData.swapChainVtable = nullptr;
        URH_DX12HOOK_LOG("ProbeVtables begin");

        WNDCLASSEXW windowClass = {};
        HWND windowHandle = nullptr;
        if (!CreateProbeWindow(windowClass, windowHandle))
        {
            URH_DX12HOOK_LOG("ProbeVtables failed: CreateProbeWindow");
            return false;
        }

        HMODULE dxgiModule = LoadLibraryW(L"dxgi.dll");
        HMODULE d3d12Module = LoadLibraryW(L"d3d12.dll");
        if (!dxgiModule || !d3d12Module)
        {
            URH_DX12HOOK_LOG("ProbeVtables failed: LoadLibrary dxgi=%p d3d12=%p", dxgiModule, d3d12Module);
            DestroyProbeWindow(windowClass, windowHandle);
            return false;
        }

        auto createDxgiFactory = reinterpret_cast<HRESULT(WINAPI*)(REFIID, void**)>(
            GetProcAddress(dxgiModule, "CreateDXGIFactory"));
        auto d3d12CreateDevice = reinterpret_cast<HRESULT(WINAPI*)(IUnknown*, D3D_FEATURE_LEVEL, REFIID, void**)>(
            GetProcAddress(d3d12Module, "D3D12CreateDevice"));
        if (!createDxgiFactory || !d3d12CreateDevice)
        {
            URH_DX12HOOK_LOG("ProbeVtables failed: missing exports CreateDXGIFactory=%p D3D12CreateDevice=%p", createDxgiFactory, d3d12CreateDevice);
            DestroyProbeWindow(windowClass, windowHandle);
            return false;
        }

        IDXGIFactory* factory = nullptr;
        IDXGIAdapter* adapter = nullptr;
        ID3D12Device* device = nullptr;
        ID3D12CommandQueue* commandQueue = nullptr;
        IDXGISwapChain* swapChain = nullptr;
        bool success = false;

        do
        {
            if (FAILED(createDxgiFactory(__uuidof(IDXGIFactory), reinterpret_cast<void**>(&factory))))
            {
                break;
            }

            if (factory->EnumAdapters(0, &adapter) == DXGI_ERROR_NOT_FOUND)
            {
                break;
            }

            if (FAILED(d3d12CreateDevice(
                    adapter,
                    D3D_FEATURE_LEVEL_11_0,
                    __uuidof(ID3D12Device),
                    reinterpret_cast<void**>(&device))))
            {
                break;
            }

            D3D12_COMMAND_QUEUE_DESC queueDesc = {};
            queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;
            if (FAILED(device->CreateCommandQueue(
                    &queueDesc,
                    __uuidof(ID3D12CommandQueue),
                    reinterpret_cast<void**>(&commandQueue))))
            {
                break;
            }

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
            swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
            swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;

            if (FAILED(factory->CreateSwapChain(commandQueue, &swapChainDesc, &swapChain)))
            {
                break;
            }

            probeData.commandQueueVtable = *reinterpret_cast<void***>(commandQueue);
            probeData.swapChainVtable = *reinterpret_cast<void***>(swapChain);
            success = probeData.commandQueueVtable != nullptr && probeData.swapChainVtable != nullptr;
        }
        while (false);

        if (swapChain)
        {
            swapChain->Release();
        }

        if (commandQueue)
        {
            commandQueue->Release();
        }

        if (device)
        {
            device->Release();
        }

        if (adapter)
        {
            adapter->Release();
        }

        if (factory)
        {
            factory->Release();
        }

        DestroyProbeWindow(windowClass, windowHandle);
        URH_DX12HOOK_LOG(
            "ProbeVtables end: success=%d queueVtable=%p swapChainVtable=%p",
            success ? 1 : 0,
            probeData.commandQueueVtable,
            probeData.swapChainVtable);
        return success;
    }
}
