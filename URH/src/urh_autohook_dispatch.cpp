#include "pch.h"

#include "urh_autohook_internal.h"

namespace
{
    using namespace UrhAutoHookInternal;

    AutoHookCandidate* GetCandidateForBackend(UrhAutoHookBackend backend)
    {
        switch (backend)
        {
        case UrhAutoHookBackend_Dx11:
            return &g_state.dx11Candidate;

        case UrhAutoHookBackend_Dx12:
            return &g_state.dx12Candidate;

        case UrhAutoHookBackend_Vulkan:
            return &g_state.vulkanCandidate;

        default:
            return nullptr;
        }
    }

    template <typename RuntimeType, typename FillRuntimeFn>
    void DispatchRuntime(const RuntimeType* runtime, UrhAutoHookBackend backend, FillRuntimeFn fillRuntime)
    {
        UrhAutoHookSetupCallback setupCallback = nullptr;
        UrhAutoHookRenderCallback renderCallback = nullptr;
        void* userData = nullptr;
        UrhAutoHookRuntime runtimeSnapshot = {};
        bool callSetup = false;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed || !runtime)
            {
                return;
            }

            if (!IsBackendEnabled(g_state.desc, backend))
            {
                return;
            }

            AutoHookCandidate* candidate = GetCandidateForBackend(backend);
            if (candidate)
            {
                UpdateCandidate(*candidate, runtime->width, runtime->height);
            }

            TryLockBackendLocked();

            if (!IsLockedBackend(backend))
            {
                return;
            }

            fillRuntime(runtime, g_state.runtime);
            runtimeSnapshot = g_state.runtime;

            if (!g_state.setupCalled)
            {
                g_state.setupCalled = true;
                setupCallback = g_state.desc.onSetup;
                callSetup = setupCallback != nullptr;
            }

            renderCallback = g_state.desc.onRender;
            userData = g_state.desc.userData;
        }

        if (callSetup)
        {
            setupCallback(&runtimeSnapshot, userData);
        }

        if (renderCallback)
        {
            renderCallback(&runtimeSnapshot, userData);
        }
    }
}

namespace UrhAutoHookInternal
{
    void DispatchDx11Render(const UrhDx11HookRuntime* runtime)
    {
        DispatchRuntime(runtime, UrhAutoHookBackend_Dx11, FillRuntimeFromDx11);
    }

    void DispatchDx12Render(const UrhDx12HookRuntime* runtime)
    {
        DispatchRuntime(runtime, UrhAutoHookBackend_Dx12, FillRuntimeFromDx12);
    }

    void DispatchVulkanRender(const VkhHookRuntime* runtime)
    {
        DispatchRuntime(runtime, UrhAutoHookBackend_Vulkan, FillRuntimeFromVulkan);
    }

    void FillDx11Desc(const UrhAutoHookDesc& sourceDesc, UrhDx11HookDesc& targetDesc)
    {
        UrhDx11Hook::FillDefaultDesc(&targetDesc);
        targetDesc.onRender = [](const UrhDx11HookRuntime* runtime, void*)
        {
            DispatchDx11Render(runtime);
        };
        targetDesc.isVisible = [](void*) -> bool
        {
            return QueryAutoVisible(UrhAutoHookBackend_Dx11);
        };
        targetDesc.autoCreateContext = sourceDesc.autoCreateContext;
        targetDesc.hookWndProc = sourceDesc.hookWndProc;
        targetDesc.blockInputWhenVisible = sourceDesc.blockInputWhenVisible;
        targetDesc.enableDefaultDebugWindow = sourceDesc.enableDefaultDebugWindow;
        targetDesc.warmupFrames = sourceDesc.warmupFrames;
        targetDesc.shutdownWaitTimeoutMs = sourceDesc.shutdownWaitTimeoutMs;
        targetDesc.toggleVirtualKey = sourceDesc.toggleVirtualKey;
        targetDesc.startVisible = sourceDesc.startVisible;
    }

    void FillDx12Desc(const UrhAutoHookDesc& sourceDesc, UrhDx12HookDesc& targetDesc)
    {
        UrhDx12Hook::FillDefaultDesc(&targetDesc);
        targetDesc.onRender = [](const UrhDx12HookRuntime* runtime, void*)
        {
            DispatchDx12Render(runtime);
        };
        targetDesc.isVisible = [](void*) -> bool
        {
            return QueryAutoVisible(UrhAutoHookBackend_Dx12);
        };
        targetDesc.autoCreateContext = sourceDesc.autoCreateContext;
        targetDesc.hookWndProc = sourceDesc.hookWndProc;
        targetDesc.blockInputWhenVisible = sourceDesc.blockInputWhenVisible;
        targetDesc.enableDefaultDebugWindow = sourceDesc.enableDefaultDebugWindow;
        targetDesc.warmupFrames = sourceDesc.warmupFrames;
        targetDesc.fenceWaitTimeoutMs = sourceDesc.fenceWaitTimeoutMs;
        targetDesc.shutdownWaitTimeoutMs = sourceDesc.shutdownWaitTimeoutMs;
        targetDesc.toggleVirtualKey = sourceDesc.toggleVirtualKey;
        targetDesc.startVisible = sourceDesc.startVisible;
    }

    void FillVulkanDesc(const UrhAutoHookDesc& sourceDesc, VkhHookDesc& targetDesc)
    {
        VkhHook::FillDefaultDesc(&targetDesc);
        targetDesc.onRender = [](const VkhHookRuntime* runtime, void*)
        {
            DispatchVulkanRender(runtime);
        };
        targetDesc.isVisible = [](void*) -> bool
        {
            return QueryAutoVisible(UrhAutoHookBackend_Vulkan);
        };
        targetDesc.autoCreateContext = sourceDesc.autoCreateContext;
        targetDesc.hookWndProc = sourceDesc.hookWndProc;
        targetDesc.blockInputWhenVisible = sourceDesc.blockInputWhenVisible;
        targetDesc.enableDefaultDebugWindow = sourceDesc.enableDefaultDebugWindow;
        targetDesc.warmupFrames = sourceDesc.warmupFrames;
        targetDesc.shutdownWaitTimeoutMs = sourceDesc.shutdownWaitTimeoutMs;
        targetDesc.toggleVirtualKey = sourceDesc.toggleVirtualKey;
        targetDesc.startVisible = sourceDesc.startVisible;
    }
}
