#include "urh_dx11_internal.h"

namespace UrhDx11HookInternal
{
    ModuleState g_state = {};

    void FillDefaultDesc(UrhDx11HookDesc& desc)
    {
        ZeroMemory(&desc, sizeof(desc));
        desc.autoCreateContext = true;
        desc.hookWndProc = true;
        desc.blockInputWhenVisible = true;
        desc.enableDefaultDebugWindow = false;
        desc.warmupFrames = 5;
        desc.shutdownWaitTimeoutMs = 5000;
        desc.toggleVirtualKey = VK_INSERT;
        desc.startVisible = true;
    }

    bool NeedsUrhBackend()
    {
        return false;
    }

    void ResetRuntime()
    {
        ZeroMemory(&g_state.runtime, sizeof(g_state.runtime));
        g_state.runtime.backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    bool PatchVtable(void** vtable, int index, void* hookFn, void** originalFn)
    {
        if (!vtable || !hookFn)
        {
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return false;
        }

        if (originalFn)
        {
            *originalFn = vtable[index];
        }

        vtable[index] = hookFn;
        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
        return true;
    }

    bool RestoreVtable(void** vtable, int index, void* originalFn)
    {
        if (!vtable || !originalFn)
        {
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return false;
        }

        vtable[index] = originalFn;
        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
        return true;
    }

    bool InstallHooks()
    {
        if (!ProbeVtables(g_state.probe))
        {
            URH_DX11HOOK_LOG("InstallHooks failed: ProbeVtables returned false.");
            return false;
        }

        if (!PatchVtable(
                g_state.probe.swapChainVtable,
                8,
                reinterpret_cast<void*>(&HookPresent),
                reinterpret_cast<void**>(&g_state.originalPresent)))
        {
            URH_DX11HOOK_LOG(
                "InstallHooks failed: PatchVtable(vtable=%p, index=8) error=%lu",
                g_state.probe.swapChainVtable,
                GetLastError());
            return false;
        }

        URH_DX11HOOK_LOG(
            "InstallHooks succeeded. probeVtable=%p originalPresent=%p hookPresent=%p",
            g_state.probe.swapChainVtable,
            reinterpret_cast<void*>(g_state.originalPresent),
            reinterpret_cast<void*>(&HookPresent));
        return true;
    }

    void UninstallHooks()
    {
        URH_DX11HOOK_LOG(
            "UninstallHooks called. probeVtable=%p originalPresent=%p",
            g_state.probe.swapChainVtable,
            reinterpret_cast<void*>(g_state.originalPresent));
        RestoreVtable(g_state.probe.swapChainVtable, 8, reinterpret_cast<void*>(g_state.originalPresent));
        g_state.originalPresent = nullptr;
        ZeroMemory(&g_state.probe, sizeof(g_state.probe));
    }
}

namespace UrhDx11Hook
{
    void FillDefaultDesc(UrhDx11HookDesc* desc)
    {
        if (!desc)
        {
            return;
        }

        UrhDx11HookInternal::FillDefaultDesc(*desc);
    }

    bool Init(const UrhDx11HookDesc* desc)
    {
        using namespace UrhDx11HookInternal;

        if (g_state.installed)
        {
            return true;
        }

        UrhDx11HookInternal::FillDefaultDesc(g_state.desc);
        if (desc)
        {
            g_state.desc = *desc;
        }

        if (!g_state.renderCsReady)
        {
            InitializeCriticalSection(&g_state.renderCs);
            g_state.renderCsReady = true;
        }

        ResetRuntime();

        g_state.backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
        g_state.unloading = false;
        g_state.suspendRendering = false;
        g_state.deviceLost = false;
        g_state.presentInFlight = 0;
        g_state.frameCount = 0;
        if (!InstallHooks())
        {
            if (g_state.renderCsReady)
            {
                DeleteCriticalSection(&g_state.renderCs);
                g_state.renderCsReady = false;
            }

            return false;
        }

        URH_DX11HOOK_LOG("UrhDx11Hook::Init completed. installed=1");
        g_state.installed = true;
        return true;
    }

    void Shutdown()
    {
        using namespace UrhDx11HookInternal;

        if (!g_state.installed)
        {
            return;
        }

        g_state.unloading = true;
        g_state.suspendRendering = true;

        DWORD waitedMs = 0;
        while (g_state.presentInFlight > 0 && waitedMs < g_state.desc.shutdownWaitTimeoutMs)
        {
            Sleep(10);
            waitedMs += 10;
        }

        UninstallHooks();
        ShutdownBackends(true);

        URH_DX11HOOK_LOG("UrhDx11Hook::Shutdown completed.");
        g_state.installed = false;
        g_state.backendReady = false;
        g_state.deviceLost = false;
        if (g_state.renderCsReady)
        {
            DeleteCriticalSection(&g_state.renderCs);
            g_state.renderCsReady = false;
        }

        ZeroMemory(&g_state.desc, sizeof(g_state.desc));
        ResetRuntime();
    }

    bool IsInstalled()
    {
        return UrhDx11HookInternal::g_state.installed;
    }

    bool IsReady()
    {
        const auto& state = UrhDx11HookInternal::g_state;
        return state.installed && state.backendReady && !state.deviceLost;
    }

    const UrhDx11HookRuntime* GetRuntime()
    {
        if (!UrhDx11HookInternal::g_state.installed)
        {
            return nullptr;
        }

        return &UrhDx11HookInternal::g_state.runtime;
    }
}
