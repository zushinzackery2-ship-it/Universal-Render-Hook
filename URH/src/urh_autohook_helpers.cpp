#include "pch.h"

#include "urh_autohook_internal.h"

namespace UrhAutoHookInternal
{
    AutoHookState g_state = {};
    constexpr ULONGLONG VulkanArbitrationDelayMs = 1500;

    namespace
    {
        constexpr UINT MinimumStableFramesToLock = 2;

        struct CandidateView
        {
            UrhAutoHookBackend backend;
            const AutoHookCandidate* candidate;
            int priority;
        };

        float GetCandidateArea(const AutoHookCandidate& candidate)
        {
            return candidate.width * candidate.height;
        }

        int GetBackendPriority(UrhAutoHookBackend backend)
        {
            switch (backend)
            {
            case UrhAutoHookBackend_Vulkan:
                return 3;

            case UrhAutoHookBackend_Dx12:
                return 2;

            case UrhAutoHookBackend_Dx11:
                return 1;

            default:
                return 0;
            }
        }
    }

    bool IsBackendEnabled(const UrhAutoHookDesc& desc, UrhAutoHookBackend backend)
    {
        switch (backend)
        {
        case UrhAutoHookBackend_Dx11:
            return (desc.backendMask & UrhAutoHookBackendMask_Dx11) != 0;

        case UrhAutoHookBackend_Dx12:
            return (desc.backendMask & UrhAutoHookBackendMask_Dx12) != 0;

        case UrhAutoHookBackend_Vulkan:
            return (desc.backendMask & UrhAutoHookBackendMask_Vulkan) != 0;

        default:
            return false;
        }
    }

    void ResetRuntime(UrhAutoHookRuntime& runtime)
    {
        ZeroMemory(&runtime, sizeof(runtime));
        runtime.backend = UrhAutoHookBackend_Unknown;
        runtime.backBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    }

    void ResetStateLocked()
    {
        ZeroMemory(&g_state.desc, sizeof(g_state.desc));
        ResetRuntime(g_state.runtime);
        g_state.installed = false;
        g_state.dx11Installed = false;
        g_state.dx12Installed = false;
        g_state.vulkanInstalled = false;
        g_state.setupCalled = false;
        g_state.installTickCount = 0;
        g_state.lockedBackend = UrhAutoHookBackend_Unknown;
        ZeroMemory(&g_state.dx11Runtime, sizeof(g_state.dx11Runtime));
        ZeroMemory(&g_state.dx12Runtime, sizeof(g_state.dx12Runtime));
        ZeroMemory(&g_state.vulkanRuntime, sizeof(g_state.vulkanRuntime));
        ZeroMemory(&g_state.dx11Candidate, sizeof(g_state.dx11Candidate));
        ZeroMemory(&g_state.dx12Candidate, sizeof(g_state.dx12Candidate));
        ZeroMemory(&g_state.vulkanCandidate, sizeof(g_state.vulkanCandidate));
    }

    void TryLockBackendLocked()
    {
        const UrhAutoHookBackend bestBackend = SelectBestBackend();
        if (bestBackend == UrhAutoHookBackend_Unknown)
        {
            return;
        }

        if (bestBackend != UrhAutoHookBackend_Vulkan &&
            g_state.vulkanInstalled &&
            !g_state.vulkanCandidate.seen &&
            g_state.installTickCount != 0)
        {
            const ULONGLONG elapsedMs = GetTickCount64() - g_state.installTickCount;
            if (elapsedMs < VulkanArbitrationDelayMs)
            {
                return;
            }
        }

        if (g_state.lockedBackend == UrhAutoHookBackend_Unknown)
        {
            g_state.lockedBackend = bestBackend;
            return;
        }

        if (g_state.lockedBackend == UrhAutoHookBackend_Vulkan)
        {
            return;
        }

        if (g_state.lockedBackend == UrhAutoHookBackend_Dx12 &&
            bestBackend != UrhAutoHookBackend_Vulkan)
        {
            return;
        }

        if (g_state.lockedBackend == UrhAutoHookBackend_Dx11 &&
            (bestBackend == UrhAutoHookBackend_Vulkan ||
             bestBackend == UrhAutoHookBackend_Dx12))
        {
            g_state.lockedBackend = bestBackend;
            g_state.setupCalled = false;
            ResetRuntime(g_state.runtime);
        }

        if (g_state.lockedBackend == UrhAutoHookBackend_Dx12 &&
            bestBackend == UrhAutoHookBackend_Vulkan)
        {
            g_state.lockedBackend = bestBackend;
            g_state.setupCalled = false;
            ResetRuntime(g_state.runtime);
        }
    }

    void UpdateCandidate(AutoHookCandidate& candidate, float width, float height)
    {
        const bool sameSize = candidate.seen &&
            candidate.width == width &&
            candidate.height == height;

        candidate.seen = true;
        candidate.width = width;
        candidate.height = height;
        candidate.stableFrames = sameSize ? candidate.stableFrames + 1 : 1;
    }

    UrhAutoHookBackend SelectBestBackend()
    {
        const CandidateView candidates[] =
        {
            { UrhAutoHookBackend_Vulkan, &g_state.vulkanCandidate, 3 },
            { UrhAutoHookBackend_Dx12, &g_state.dx12Candidate, 2 },
            { UrhAutoHookBackend_Dx11, &g_state.dx11Candidate, 1 }
        };

        const CandidateView* bestCandidate = nullptr;
        for (const CandidateView& candidateView : candidates)
        {
            if (!IsBackendEnabled(g_state.desc, candidateView.backend) ||
                !candidateView.candidate->seen ||
                candidateView.candidate->stableFrames < MinimumStableFramesToLock)
            {
                continue;
            }

            if (!bestCandidate)
            {
                bestCandidate = &candidateView;
                continue;
            }

            if (candidateView.candidate->stableFrames > bestCandidate->candidate->stableFrames)
            {
                bestCandidate = &candidateView;
                continue;
            }

            if (candidateView.candidate->stableFrames < bestCandidate->candidate->stableFrames)
            {
                continue;
            }

            const float candidateArea = GetCandidateArea(*candidateView.candidate);
            const float bestArea = GetCandidateArea(*bestCandidate->candidate);
            if (candidateArea > bestArea)
            {
                bestCandidate = &candidateView;
                continue;
            }

            if (candidateArea < bestArea)
            {
                continue;
            }

            if (candidateView.priority > bestCandidate->priority)
            {
                bestCandidate = &candidateView;
            }
        }

        if (bestCandidate)
        {
            return bestCandidate->backend;
        }

        return UrhAutoHookBackend_Unknown;
    }

    void FillRuntimeFromDx11(
        const UrhDx11HookRuntime* sourceRuntime,
        UrhAutoHookRuntime& targetRuntime)
    {
        ResetRuntime(targetRuntime);
        g_state.dx11Runtime = *sourceRuntime;
        targetRuntime.backend = UrhAutoHookBackend_Dx11;
        targetRuntime.hwnd = sourceRuntime->hwnd;
        targetRuntime.nativeRuntime = &g_state.dx11Runtime;
        targetRuntime.bufferCount = sourceRuntime->bufferCount;
        targetRuntime.backBufferIndex = sourceRuntime->backBufferIndex;
        targetRuntime.backBufferFormat = sourceRuntime->backBufferFormat;
        targetRuntime.width = sourceRuntime->width;
        targetRuntime.height = sourceRuntime->height;
        targetRuntime.frameCount = sourceRuntime->frameCount;
    }

    void FillRuntimeFromDx12(
        const UrhDx12HookRuntime* sourceRuntime,
        UrhAutoHookRuntime& targetRuntime)
    {
        ResetRuntime(targetRuntime);
        g_state.dx12Runtime = *sourceRuntime;
        targetRuntime.backend = UrhAutoHookBackend_Dx12;
        targetRuntime.hwnd = sourceRuntime->hwnd;
        targetRuntime.nativeRuntime = &g_state.dx12Runtime;
        targetRuntime.bufferCount = sourceRuntime->bufferCount;
        targetRuntime.backBufferIndex = sourceRuntime->backBufferIndex;
        targetRuntime.backBufferFormat = sourceRuntime->backBufferFormat;
        targetRuntime.width = sourceRuntime->width;
        targetRuntime.height = sourceRuntime->height;
        targetRuntime.frameCount = sourceRuntime->frameCount;
    }

    void FillRuntimeFromVulkan(
        const VkhHookRuntime* sourceRuntime,
        UrhAutoHookRuntime& targetRuntime)
    {
        ResetRuntime(targetRuntime);
        g_state.vulkanRuntime = *sourceRuntime;
        targetRuntime.backend = UrhAutoHookBackend_Vulkan;
        targetRuntime.hwnd = sourceRuntime->hwnd;
        targetRuntime.nativeRuntime = &g_state.vulkanRuntime;
        targetRuntime.bufferCount = sourceRuntime->imageCount;
        targetRuntime.backBufferIndex = sourceRuntime->imageIndex;
        targetRuntime.width = sourceRuntime->width;
        targetRuntime.height = sourceRuntime->height;
        targetRuntime.frameCount = sourceRuntime->frameCount;
    }

    bool IsLockedBackend(UrhAutoHookBackend backend)
    {
        return g_state.lockedBackend == backend;
    }

    bool QueryAutoVisible(UrhAutoHookBackend backend)
    {
        UrhAutoHookVisibleCallback visibleCallback = nullptr;
        void* userData = nullptr;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed || !IsLockedBackend(backend))
            {
                return false;
            }

            visibleCallback = g_state.desc.isVisible;
            userData = g_state.desc.userData;
        }

        return visibleCallback ? visibleCallback(userData) : false;
    }
}
