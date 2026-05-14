#include "urh_dx12_internal.h"

namespace UrhDx12HookInternal
{
    ModuleState g_state = {};

    namespace
    {
        DWORD WINAPI DefaultTestBootstrapThread(LPVOID)
        {
            UrhDx12HookDesc desc = {};
            FillDefaultDesc(desc);
            desc.enableDefaultDebugWindow = true;
            desc.startVisible = true;

            for (UINT attempt = 0; attempt < 200; ++attempt)
            {
                if (g_state.unloading)
                {
                    break;
                }

                if (UrhDx12Hook::Init(&desc))
                {
                    break;
                }

                Sleep(100);
            }

            InterlockedExchange(&g_state.bootstrapRequested, 0);
            return 1;
        }
    }

    void FillDefaultDesc(UrhDx12HookDesc& desc)
    {
        ZeroMemory(&desc, sizeof(desc));
        desc.autoCreateContext = true;
        desc.hookWndProc = true;
        desc.blockInputWhenVisible = true;
        desc.enableDefaultDebugWindow = false;
        desc.warmupFrames = 5;
        desc.fenceWaitTimeoutMs = 500;
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
            return false;
        }

        auto rollback = []()
        {
            RestoreVtable(g_state.probe.swapChainVtable, 8, reinterpret_cast<void*>(g_state.originalPresent));
            RestoreVtable(g_state.probe.swapChainVtable, 22, reinterpret_cast<void*>(g_state.originalPresent1));
            RestoreVtable(
                g_state.probe.swapChainVtable,
                13,
                reinterpret_cast<void*>(g_state.originalResizeBuffers));
            RestoreVtable(
                g_state.probe.commandQueueVtable,
                10,
                reinterpret_cast<void*>(g_state.originalExecuteCommandLists));
            g_state.originalPresent = nullptr;
            g_state.originalPresent1 = nullptr;
            g_state.originalResizeBuffers = nullptr;
            g_state.originalExecuteCommandLists = nullptr;
        };

        if (!PatchVtable(
                g_state.probe.swapChainVtable,
                8,
                reinterpret_cast<void*>(&HookPresent),
                reinterpret_cast<void**>(&g_state.originalPresent)))
        {
            return false;
        }

        PatchVtable(
            g_state.probe.swapChainVtable,
            22,
            reinterpret_cast<void*>(&HookPresent1),
            reinterpret_cast<void**>(&g_state.originalPresent1));

        if (!PatchVtable(
                g_state.probe.swapChainVtable,
                13,
                reinterpret_cast<void*>(&HookResizeBuffers),
                reinterpret_cast<void**>(&g_state.originalResizeBuffers)))
        {
            rollback();
            return false;
        }

        if (!PatchVtable(
                g_state.probe.commandQueueVtable,
                10,
                reinterpret_cast<void*>(&HookExecuteCommandLists),
                reinterpret_cast<void**>(&g_state.originalExecuteCommandLists)))
        {
            rollback();
            return false;
        }

        return true;
    }

    void UninstallHooks()
    {
        RestoreVtable(g_state.probe.swapChainVtable, 8, reinterpret_cast<void*>(g_state.originalPresent));
        RestoreVtable(g_state.probe.swapChainVtable, 22, reinterpret_cast<void*>(g_state.originalPresent1));
        RestoreVtable(
            g_state.probe.swapChainVtable,
            13,
            reinterpret_cast<void*>(g_state.originalResizeBuffers));
        RestoreVtable(
            g_state.probe.commandQueueVtable,
            10,
            reinterpret_cast<void*>(g_state.originalExecuteCommandLists));

        if (g_state.trackedSwapChainVtable && g_state.originalResizeBuffers1)
        {
            RestoreVtable(
                g_state.trackedSwapChainVtable,
                39,
                reinterpret_cast<void*>(g_state.originalResizeBuffers1));
        }

        g_state.originalPresent = nullptr;
        g_state.originalPresent1 = nullptr;
        g_state.originalResizeBuffers = nullptr;
        g_state.originalResizeBuffers1 = nullptr;
        g_state.originalExecuteCommandLists = nullptr;
        ZeroMemory(&g_state.probe, sizeof(g_state.probe));
    }
}

namespace UrhDx12Hook
{
    void FillDefaultDesc(UrhDx12HookDesc* desc)
    {
        if (!desc)
        {
            return;
        }

        UrhDx12HookInternal::FillDefaultDesc(*desc);
    }

    bool Init(const UrhDx12HookDesc* desc)
    {
        using namespace UrhDx12HookInternal;

        if (g_state.installed)
        {
            URH_DX12HOOK_LOG("Init called while already installed");
            return true;
        }

        UrhDx12HookInternal::FillDefaultDesc(g_state.desc);
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
            URH_DX12HOOK_LOG("Init failed: InstallHooks returned false");
            if (g_state.renderCsReady)
            {
                DeleteCriticalSection(&g_state.renderCs);
                g_state.renderCsReady = false;
            }

            return false;
        }

        g_state.installed = true;
        URH_DX12HOOK_LOG(
            "Init success: installed=1 present=%p present1=%p resize=%p execute=%p",
            g_state.originalPresent,
            g_state.originalPresent1,
            g_state.originalResizeBuffers,
            g_state.originalExecuteCommandLists);
        return true;
    }

    bool InitDefaultTest()
    {
        using namespace UrhDx12HookInternal;

        if (g_state.installed)
        {
            return true;
        }

        if (InterlockedCompareExchange(&g_state.bootstrapRequested, 1, 0) != 0)
        {
            return true;
        }

        HANDLE threadHandle = CreateThread(nullptr, 0, DefaultTestBootstrapThread, nullptr, 0, nullptr);
        if (!threadHandle)
        {
            InterlockedExchange(&g_state.bootstrapRequested, 0);
            return false;
        }

        CloseHandle(threadHandle);
        return true;
    }

    void Shutdown()
    {
        using namespace UrhDx12HookInternal;

        if (!g_state.installed)
        {
            return;
        }

        g_state.unloading = true;
        g_state.suspendRendering = true;
        InterlockedExchange(&g_state.bootstrapRequested, 0);

        DWORD waitedMs = 0;
        while (g_state.presentInFlight > 0 && waitedMs < g_state.desc.shutdownWaitTimeoutMs)
        {
            Sleep(10);
            waitedMs += 10;
        }

        UninstallHooks();
        ShutdownBackends(true);

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
        URH_DX12HOOK_LOG("Shutdown complete");
    }

    bool IsInstalled()
    {
        return UrhDx12HookInternal::g_state.installed;
    }

    bool IsReady()
    {
        const auto& state = UrhDx12HookInternal::g_state;
        return state.installed && state.backendReady && !state.deviceLost;
    }

    const UrhDx12HookRuntime* GetRuntime()
    {
        if (!UrhDx12HookInternal::g_state.installed)
        {
            return nullptr;
        }

        return &UrhDx12HookInternal::g_state.runtime;
    }
}
