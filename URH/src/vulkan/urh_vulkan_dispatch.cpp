#include "urh_vulkan_internal.h"

namespace
{
    LONG g_dispatchPresentSkipLogCount = 0;
    LONG g_dispatchPresentIncompleteLogCount = 0;
}

namespace UrhVulkanHookInternal
{
    void DispatchPresent(VkQueue queue, VkSwapchainKHR swapchain, UINT imageIndex)
    {
        UrhVulkanHookSetupCallback setupCallback = nullptr;
        UrhVulkanHookRenderCallback renderCallback = nullptr;
        void* userData = nullptr;
        UrhVulkanHookRuntime runtimeSnapshot = {};
        bool callSetup = false;
        bool callRender = false;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed)
            {
                return;
            }

            MarkBackendRecognizedLocked("DispatchPresent");

            if (!TryBootstrapLateSwapchainLocked(queue, swapchain))
            {
                if (InterlockedIncrement(&g_dispatchPresentSkipLogCount) <= 12)
                {
                    URH_VULKANHOOK_LOG(
                        "DispatchPresent skipped: late swapchain bootstrap failed queue=%p swapchain=%p",
                        queue,
                        reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain)));
                }
                return;
            }

            const auto swapchainIt = g_state.swapchains.find(swapchain);
            if (swapchainIt == g_state.swapchains.end())
            {
                return;
            }

            SwapchainInfo& swapchainInfo = swapchainIt->second;
            VkInstance instance = nullptr;
            HWND hwnd = swapchainInfo.hwnd;
            const auto surfaceIt = g_state.surfaces.find(swapchainInfo.surface);
            if (surfaceIt != g_state.surfaces.end())
            {
                hwnd = surfaceIt->second.hwnd;
                instance = surfaceIt->second.instance;
                swapchainInfo.hwnd = hwnd;
                swapchainInfo.lateAttached = false;
            }

            if (swapchainInfo.lateAttached && (!hwnd || !IsCandidateWindow(hwnd)))
            {
                hwnd = FindProcessRenderWindow();
                swapchainInfo.hwnd = hwnd;
            }

            if (!IsCandidateWindow(hwnd))
            {
                if (InterlockedIncrement(&g_dispatchPresentSkipLogCount) <= 12)
                {
                    URH_VULKANHOOK_LOG(
                        "DispatchPresent skipped: no candidate window queue=%p swapchain=%p hwnd=%p",
                        queue,
                        reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain)),
                        hwnd);
                }
                return;
            }

            if (swapchainInfo.lateAttached)
            {
                UpdateWindowMetrics(hwnd, swapchainInfo.width, swapchainInfo.height);
            }

            swapchainInfo.lastQueue = queue;
            const auto queueIt = g_state.queues.find(UrhVulkanCommon::HandleKey(queue));
            if (queueIt != g_state.queues.end())
            {
                swapchainInfo.queueFamilyIndex = queueIt->second.queueFamilyIndex;
                if (!swapchainInfo.device)
                {
                    swapchainInfo.device = queueIt->second.device;
                }
                const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(swapchainInfo.device));
                if (deviceIt != g_state.devices.end())
                {
                    swapchainInfo.physicalDevice = deviceIt->second.physicalDevice;
                }
            }
            swapchainInfo.lastImageIndex = imageIndex;
            ++swapchainInfo.frameCount;

            ResetRuntime(g_state.runtime);
            g_state.runtime.hwnd = hwnd;
            g_state.runtime.instance = instance;
            g_state.runtime.physicalDevice = swapchainInfo.physicalDevice;
            g_state.runtime.device = swapchainInfo.device;
            g_state.runtime.queue = queue;
            g_state.runtime.swapchain = reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain));
            g_state.runtime.queueFamilyIndex = swapchainInfo.queueFamilyIndex;
            g_state.runtime.swapchainFormat = swapchainInfo.format;
            const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(swapchainInfo.device));
            if (deviceIt != g_state.devices.end())
            {
                g_state.runtime.memoryTypeCount = deviceIt->second.memoryTypeCount;
                for (UINT index = 0; index < deviceIt->second.memoryTypeCount && index < 32u; ++index)
                {
                    g_state.runtime.memoryTypeFlags[index] = deviceIt->second.memoryTypeFlags[index];
                }
            }
            g_state.runtime.imageCount = swapchainInfo.imageCount;
            g_state.runtime.imageIndex = imageIndex;
            g_state.runtime.width = swapchainInfo.width;
            g_state.runtime.height = swapchainInfo.height;
            g_state.runtime.frameCount = swapchainInfo.frameCount;
            const bool runtimeReady = IsRuntimeReadyForCapture(g_state.runtime);
            const bool wasReady = g_state.ready;
            g_state.ready = runtimeReady;
            if (runtimeReady && !wasReady)
            {
                URH_VULKANHOOK_LOG(
                    "DispatchPresent ready: queue=%p swapchain=%p imageIndex=%u size=%.0fx%.0f device=%p",
                    queue,
                    reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain)),
                    imageIndex,
                    g_state.runtime.width,
                    g_state.runtime.height,
                    g_state.runtime.device);
            }
            else if (!runtimeReady && InterlockedIncrement(&g_dispatchPresentIncompleteLogCount) <= 24)
            {
                URH_VULKANHOOK_LOG(
                    "DispatchPresent runtime incomplete: queue=%p swapchain=%p device=%p physicalDevice=%p family=%u memTypes=%u size=%.0fx%.0f hwnd=%p",
                    queue,
                    reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain)),
                    g_state.runtime.device,
                    g_state.runtime.physicalDevice,
                    g_state.runtime.queueFamilyIndex,
                    g_state.runtime.memoryTypeCount,
                    g_state.runtime.width,
                    g_state.runtime.height,
                    g_state.runtime.hwnd);
            }
            runtimeSnapshot = g_state.runtime;

            if (!g_state.setupCalled && swapchainInfo.frameCount > g_state.desc.warmupFrames)
            {
                g_state.setupCalled = true;
                setupCallback = g_state.desc.onSetup;
                callSetup = setupCallback != nullptr;
            }

            renderCallback = g_state.desc.onRender;
            userData = g_state.desc.userData;
            callRender = renderCallback != nullptr && swapchainInfo.frameCount > g_state.desc.warmupFrames;
        }

        if (callSetup)
        {
            setupCallback(&runtimeSnapshot, userData);
        }

        if (callRender)
        {
            renderCallback(&runtimeSnapshot, userData);
        }
    }
}
