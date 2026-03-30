#pragma once

#include <Windows.h>

#include <mutex>

#include "urh_autohook.h"
#include "urh_dx11_hook.h"
#include "urh_dx12_hook.h"
#include "urh_vulkan_hook.h"

namespace UrhAutoHookInternal
{
    struct AutoHookCandidate
    {
        bool seen;
        UINT stableFrames;
        float width;
        float height;
    };

    struct AutoHookState
    {
        UrhAutoHookDesc desc;
        UrhAutoHookRuntime runtime;
        UrhDx11HookRuntime dx11Runtime;
        UrhDx12HookRuntime dx12Runtime;
        VkhHookRuntime vulkanRuntime;
        std::mutex mutex;
        bool installed;
        bool dx11Installed;
        bool dx12Installed;
        bool vulkanInstalled;
        bool setupCalled;
        ULONGLONG installTickCount;
        UrhAutoHookBackend lockedBackend;
        AutoHookCandidate dx11Candidate;
        AutoHookCandidate dx12Candidate;
        AutoHookCandidate vulkanCandidate;
    };

    extern AutoHookState g_state;

    void ResetRuntime(UrhAutoHookRuntime& runtime);
    void ResetStateLocked();
    void TryLockBackendLocked();
    bool IsBackendEnabled(const UrhAutoHookDesc& desc, UrhAutoHookBackend backend);
    void UpdateCandidate(AutoHookCandidate& candidate, float width, float height);
    UrhAutoHookBackend SelectBestBackend();
    void FillRuntimeFromDx11(const UrhDx11HookRuntime* sourceRuntime, UrhAutoHookRuntime& targetRuntime);
    void FillRuntimeFromDx12(const UrhDx12HookRuntime* sourceRuntime, UrhAutoHookRuntime& targetRuntime);
    void FillRuntimeFromVulkan(const VkhHookRuntime* sourceRuntime, UrhAutoHookRuntime& targetRuntime);
    bool IsLockedBackend(UrhAutoHookBackend backend);
    void DispatchDx11Render(const UrhDx11HookRuntime* runtime);
    void DispatchDx12Render(const UrhDx12HookRuntime* runtime);
    void DispatchVulkanRender(const VkhHookRuntime* runtime);
    bool QueryAutoVisible(UrhAutoHookBackend backend);
    void FillDx11Desc(const UrhAutoHookDesc& sourceDesc, UrhDx11HookDesc& targetDesc);
    void FillDx12Desc(const UrhAutoHookDesc& sourceDesc, UrhDx12HookDesc& targetDesc);
    void FillVulkanDesc(const UrhAutoHookDesc& sourceDesc, VkhHookDesc& targetDesc);
}
