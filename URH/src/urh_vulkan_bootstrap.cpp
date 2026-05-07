#include "urh_vulkan_internal.h"

namespace UrhVulkanHookInternal
{
    ModuleState g_state = {};

    void ResetRuntimeStateLocked(bool preserveTrackedObjects)
    {
        ResetRuntime(g_state.runtime);
        if (!preserveTrackedObjects)
        {
            g_state.surfaces.clear();
            g_state.swapchains.clear();
            g_state.devices.clear();
            g_state.queues.clear();
        }

        g_state.runtimeProbeThreadId.store(0);
        g_state.runtimeProbeRequested = false;
        g_state.runtimeProbeStopRequested = false;
        g_state.runtimeProbeCompleted = false;
        g_state.backendRecognized = preserveTrackedObjects;
        g_state.ready = false;
        g_state.setupCalled = false;
    }

    void MarkBackendRecognizedLocked(const char* reason)
    {
        if (g_state.backendRecognized)
        {
            return;
        }

        g_state.backendRecognized = true;
        URH_VULKAN_LOG(
            "Backend recognized: reason=%s",
            reason ? reason : "<unknown>");
    }

    void FillDefaultDesc(UrhVulkanHookDesc& desc)
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

    void ResetRuntime(UrhVulkanHookRuntime& runtime)
    {
        ZeroMemory(&runtime, sizeof(runtime));
    }

    void ResetTransientStateLocked()
    {
        ResetRuntimeStateLocked(false);
    }

    void ResolveLoaderFunctionsLocked()
    {
        if (!g_state.realGetProcAddress)
        {
            g_state.realGetProcAddress = reinterpret_cast<GetProcAddressFn>(
                ::GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetProcAddress"));
        }

        g_state.vulkanModule = FindVulkanModuleLocked();
        if (g_state.vulkanModule && g_state.realGetProcAddress)
        {
            g_state.realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                g_state.realGetProcAddress(g_state.vulkanModule, "vkGetInstanceProcAddr"));
            g_state.realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                g_state.realGetProcAddress(g_state.vulkanModule, "vkGetDeviceProcAddr"));
        }
    }

    HMODULE FindVulkanModuleLocked()
    {
        return GetModuleHandleW(L"vulkan-1.dll");
    }

    void MarkLayerModeActiveLocked()
    {
        g_state.layerModeEnabled = true;
    }
}

namespace UrhVulkanHook
{
    void FillDefaultDesc(UrhVulkanHookDesc* desc)
    {
        if (!desc)
        {
            return;
        }

        UrhVulkanHookInternal::FillDefaultDesc(*desc);
    }

    bool Init(const UrhVulkanHookDesc* desc)
    {
        using namespace UrhVulkanHookInternal;

        URH_VULKAN_LOG("Init entered");
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (g_state.installed)
            {
                URH_VULKAN_LOG("Init called while already installed");
                return true;
            }

            UrhVulkanHookInternal::FillDefaultDesc(g_state.desc);
            if (desc)
            {
                g_state.desc = *desc;
            }

            const bool preserveTrackedObjects =
                g_state.layerModeEnabled ||
                !g_state.surfaces.empty() ||
                !g_state.swapchains.empty() ||
                !g_state.devices.empty() ||
                !g_state.queues.empty();

            ResetRuntimeStateLocked(preserveTrackedObjects);
            ResolveLoaderFunctionsLocked();
            g_state.layerModeEnabled = true;
            g_state.installed = true;
        }

        URH_VULKAN_LOG("Init success: layer-mode only.");
        return true;
    }

    void Shutdown()
    {
        using namespace UrhVulkanHookInternal;

        std::thread runtimeProbeThread;
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            StopRuntimeProbeThreadLocked(runtimeProbeThread);
        }

        if (runtimeProbeThread.joinable())
        {
            runtimeProbeThread.join();
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            ResetTransientStateLocked();
            ZeroMemory(&g_state.desc, sizeof(g_state.desc));
            g_state.vulkanModule = nullptr;
            g_state.realGetProcAddress = nullptr;
            g_state.realVkGetInstanceProcAddr = nullptr;
            g_state.realVkGetDeviceProcAddr = nullptr;
            g_state.layerModeEnabled = false;
            g_state.installed = false;
        }
    }

    bool IsInstalled()
    {
        std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
        return UrhVulkanHookInternal::g_state.installed;
    }

    bool IsLayerModeEnabled()
    {
        std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
        return UrhVulkanHookInternal::g_state.layerModeEnabled;
    }

    bool HasTrackedActivity()
    {
        std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
        return UrhVulkanHookInternal::g_state.ready ||
            UrhVulkanHookInternal::g_state.backendRecognized ||
            UrhVulkanHookInternal::g_state.layerModeEnabled ||
            !UrhVulkanHookInternal::g_state.surfaces.empty() ||
            !UrhVulkanHookInternal::g_state.swapchains.empty() ||
            !UrhVulkanHookInternal::g_state.devices.empty() ||
            !UrhVulkanHookInternal::g_state.queues.empty();
    }

    bool HasRecognizedBackend()
    {
        std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
        return UrhVulkanHookInternal::g_state.installed &&
            (UrhVulkanHookInternal::g_state.backendRecognized ||
             UrhVulkanHookInternal::g_state.ready);
    }

    bool IsReady()
    {
        std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
        return UrhVulkanHookInternal::g_state.installed && UrhVulkanHookInternal::g_state.ready;
    }

    const UrhVulkanHookRuntime* GetRuntime()
    {
        std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
        if (!UrhVulkanHookInternal::g_state.installed)
        {
            return nullptr;
        }

        return &UrhVulkanHookInternal::g_state.runtime;
    }
}
