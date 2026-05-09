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
                if (g_state.unloading.load(std::memory_order_relaxed))
                {
                    break;
                }

                if (UrhDx12Hook::Init(&desc))
                {
                    break;
                }

                Sleep(100);
            }

            g_state.bootstrapRequested.store(0);
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
        g_state.unloading.store(false);
        g_state.suspendRendering.store(false);
        g_state.deviceLost = false;
        g_state.presentInFlight.store(0);
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

        LONG expected = 0;
        if (!g_state.bootstrapRequested.compare_exchange_strong(expected, 1))
        {
            return true;
        }

        HANDLE threadHandle = CreateThread(nullptr, 0, DefaultTestBootstrapThread, nullptr, 0, nullptr);
        if (!threadHandle)
        {
            g_state.bootstrapRequested.store(0);
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

        g_state.unloading.store(true);
        g_state.suspendRendering.store(true);
        g_state.bootstrapRequested.store(0);

        DWORD waitedMs = 0;
        while (g_state.presentInFlight.load() > 0 && waitedMs < g_state.desc.shutdownWaitTimeoutMs)
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
