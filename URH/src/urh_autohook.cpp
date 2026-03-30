#include "pch.h"

#include "urh_autohook_internal.h"

namespace UrhAutoHook
{
    void FillDefaultDesc(UrhAutoHookDesc* desc)
    {
        if (!desc)
        {
            return;
        }

        ZeroMemory(desc, sizeof(*desc));
        desc->autoCreateContext = true;
        desc->hookWndProc = true;
        desc->blockInputWhenVisible = true;
        desc->enableDefaultDebugWindow = false;
        desc->backendMask = UrhAutoHookBackendMask_All;
        desc->warmupFrames = 5;
        desc->fenceWaitTimeoutMs = 500;
        desc->shutdownWaitTimeoutMs = 5000;
        desc->toggleVirtualKey = VK_INSERT;
        desc->startVisible = true;
    }

    bool Init(const UrhAutoHookDesc* desc)
    {
        using namespace UrhAutoHookInternal;

        UrhAutoHookDesc effectiveDesc = {};
        FillDefaultDesc(&effectiveDesc);
        if (desc)
        {
            effectiveDesc = *desc;
        }

        if (effectiveDesc.backendMask == UrhAutoHookBackendMask_None)
        {
            effectiveDesc.backendMask = UrhAutoHookBackendMask_All;
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (g_state.installed)
            {
                return true;
            }

            ResetStateLocked();
            g_state.desc = effectiveDesc;
            g_state.installTickCount = GetTickCount64();
            g_state.installed = true;
        }

        UrhDx11HookDesc dx11Desc = {};
        UrhDx12HookDesc dx12Desc = {};
        VkhHookDesc vulkanDesc = {};
        FillDx11Desc(effectiveDesc, dx11Desc);
        FillDx12Desc(effectiveDesc, dx12Desc);
        FillVulkanDesc(effectiveDesc, vulkanDesc);

        const bool dx11Installed = (effectiveDesc.backendMask & UrhAutoHookBackendMask_Dx11) != 0
            ? UrhDx11Hook::Init(&dx11Desc)
            : false;
        const bool dx12Installed = (effectiveDesc.backendMask & UrhAutoHookBackendMask_Dx12) != 0
            ? UrhDx12Hook::Init(&dx12Desc)
            : false;
        const bool vulkanInstalled = (effectiveDesc.backendMask & UrhAutoHookBackendMask_Vulkan) != 0
            ? VkhHook::Init(&vulkanDesc)
            : false;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.dx11Installed = dx11Installed;
            g_state.dx12Installed = dx12Installed;
            g_state.vulkanInstalled = vulkanInstalled;

            if (!dx11Installed && !dx12Installed && !vulkanInstalled)
            {
                ResetStateLocked();
                return false;
            }
        }

        return true;
    }

    void Shutdown()
    {
        using namespace UrhAutoHookInternal;

        UrhAutoHookShutdownCallback shutdownCallback = nullptr;
        void* userData = nullptr;
        bool dx11Installed = false;
        bool dx12Installed = false;
        bool vulkanInstalled = false;
        bool callShutdown = false;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            shutdownCallback = g_state.desc.onShutdown;
            userData = g_state.desc.userData;
            dx11Installed = g_state.dx11Installed;
            dx12Installed = g_state.dx12Installed;
            vulkanInstalled = g_state.vulkanInstalled;
            callShutdown = g_state.setupCalled && shutdownCallback != nullptr;
            g_state.installed = false;
        }

        if (dx11Installed)
        {
            UrhDx11Hook::Shutdown();
        }

        if (dx12Installed)
        {
            UrhDx12Hook::Shutdown();
        }

        if (vulkanInstalled)
        {
            VkhHook::Shutdown();
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            ResetStateLocked();
        }

        if (callShutdown)
        {
            shutdownCallback(userData);
        }
    }

    bool IsInstalled()
    {
        using namespace UrhAutoHookInternal;

        std::lock_guard<std::mutex> lock(g_state.mutex);
        return g_state.installed;
    }

    bool IsReady()
    {
        using namespace UrhAutoHookInternal;

        std::lock_guard<std::mutex> lock(g_state.mutex);
        switch (g_state.lockedBackend)
        {
        case UrhAutoHookBackend_Dx11:
            return g_state.dx11Installed && UrhDx11Hook::IsReady();

        case UrhAutoHookBackend_Dx12:
            return g_state.dx12Installed && UrhDx12Hook::IsReady();

        case UrhAutoHookBackend_Vulkan:
            return g_state.vulkanInstalled && VkhHook::IsReady();

        default:
            return false;
        }
    }

    const UrhAutoHookRuntime* GetRuntime()
    {
        using namespace UrhAutoHookInternal;

        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (!g_state.installed)
        {
            return nullptr;
        }

        return &g_state.runtime;
    }

    void GetDiagnostics(UrhAutoHookDiagnostics* diagnostics)
    {
        using namespace UrhAutoHookInternal;

        if (!diagnostics)
        {
            return;
        }

        ZeroMemory(diagnostics, sizeof(*diagnostics));
        diagnostics->lockedBackend = UrhAutoHookBackend_Unknown;

        std::lock_guard<std::mutex> lock(g_state.mutex);
        diagnostics->lockedBackend = g_state.lockedBackend;
        diagnostics->dx11Seen = g_state.dx11Candidate.seen;
        diagnostics->dx11StableFrames = g_state.dx11Candidate.stableFrames;
        diagnostics->dx11Width = g_state.dx11Candidate.width;
        diagnostics->dx11Height = g_state.dx11Candidate.height;
        diagnostics->dx12Seen = g_state.dx12Candidate.seen;
        diagnostics->dx12StableFrames = g_state.dx12Candidate.stableFrames;
        diagnostics->dx12Width = g_state.dx12Candidate.width;
        diagnostics->dx12Height = g_state.dx12Candidate.height;
        diagnostics->vulkanSeen = g_state.vulkanCandidate.seen;
        diagnostics->vulkanStableFrames = g_state.vulkanCandidate.stableFrames;
        diagnostics->vulkanWidth = g_state.vulkanCandidate.width;
        diagnostics->vulkanHeight = g_state.vulkanCandidate.height;
    }
}
