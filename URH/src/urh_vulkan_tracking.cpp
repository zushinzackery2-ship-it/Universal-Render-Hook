#include "urh_vulkan_internal.h"

namespace
{
    LONG g_dispatchPresentSkipLogCount = 0;
    LONG g_dispatchPresentIncompleteLogCount = 0;
    LONG g_runtimeRefreshLogCount = 0;

    bool IsRuntimeReadyForCapture(const UrhVulkanHookRuntime& runtime)
    {
        return runtime.device != nullptr &&
            runtime.queue != nullptr &&
            runtime.swapchain != nullptr &&
            runtime.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
            runtime.swapchainFormat != 0 &&
            runtime.width > 0.0f &&
            runtime.height > 0.0f;
    }

    bool IsCandidateWindow(HWND hwnd)
    {
        if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
        {
            return false;
        }

        if (GetWindow(hwnd, GW_OWNER))
        {
            return false;
        }

        const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
        return (style & WS_CHILD) == 0;
    }

    float UpdateWindowMetrics(HWND hwnd, float& width, float& height)
    {
        RECT clientRect = {};
        if (!hwnd || !GetClientRect(hwnd, &clientRect))
        {
            width = 0.0f;
            height = 0.0f;
            return 0.0f;
        }

        width = static_cast<float>(clientRect.right - clientRect.left);
        height = static_cast<float>(clientRect.bottom - clientRect.top);
        return width * height;
    }

    struct WindowSearchState
    {
        DWORD processId;
        HWND bestWindow;
        float bestArea;
    };

    BOOL CALLBACK EnumProcessWindowsProc(HWND hwnd, LPARAM lParam)
    {
        auto* state = reinterpret_cast<WindowSearchState*>(lParam);
        if (!state || !IsCandidateWindow(hwnd))
        {
            return TRUE;
        }

        DWORD windowProcessId = 0;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId != state->processId)
        {
            return TRUE;
        }

        float width = 0.0f;
        float height = 0.0f;
        const float area = UpdateWindowMetrics(hwnd, width, height);
        if (area <= state->bestArea)
        {
            return TRUE;
        }

        state->bestArea = area;
        state->bestWindow = hwnd;
        return TRUE;
    }

    HWND FindProcessRenderWindow()
    {
        const DWORD processId = GetCurrentProcessId();
        HWND foregroundWindow = GetForegroundWindow();
        if (foregroundWindow && IsCandidateWindow(foregroundWindow))
        {
            DWORD foregroundProcessId = 0;
            GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
            if (foregroundProcessId == processId)
            {
                return foregroundWindow;
            }
        }

        WindowSearchState state = {};
        state.processId = processId;
        EnumWindows(EnumProcessWindowsProc, reinterpret_cast<LPARAM>(&state));
        return state.bestWindow;
    }

    void RefreshRuntimeFromTrackedSwapchainLocked(VkSwapchainKHR swapchain)
    {
        using namespace UrhVulkanHookInternal;

        const auto swapchainIt = g_state.swapchains.find(swapchain);
        if (swapchainIt == g_state.swapchains.end())
        {
            return;
        }

        const SwapchainInfo& swapchainInfo = swapchainIt->second;
        if (!swapchainInfo.device ||
            !swapchainInfo.lastQueue ||
            !swapchainInfo.hwnd ||
            swapchainInfo.width <= 0.0f ||
            swapchainInfo.height <= 0.0f)
        {
            return;
        }

        VkInstance instance = nullptr;
        const auto surfaceIt = g_state.surfaces.find(swapchainInfo.surface);
        if (surfaceIt != g_state.surfaces.end())
        {
            instance = surfaceIt->second.instance;
        }

        ResetRuntime(g_state.runtime);
        g_state.runtime.hwnd = swapchainInfo.hwnd;
        g_state.runtime.instance = instance;
        g_state.runtime.physicalDevice = swapchainInfo.physicalDevice;
        g_state.runtime.device = swapchainInfo.device;
        g_state.runtime.queue = swapchainInfo.lastQueue;
        g_state.runtime.swapchain = reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain));
        g_state.runtime.queueFamilyIndex = swapchainInfo.queueFamilyIndex;
        g_state.runtime.swapchainFormat = swapchainInfo.format;
        const auto deviceIt = g_state.devices.find(HandleKey(swapchainInfo.device));
        if (deviceIt != g_state.devices.end())
        {
            g_state.runtime.memoryTypeCount = deviceIt->second.memoryTypeCount;
            for (UINT index = 0; index < deviceIt->second.memoryTypeCount && index < 32u; ++index)
            {
                g_state.runtime.memoryTypeFlags[index] = deviceIt->second.memoryTypeFlags[index];
            }
        }
        g_state.runtime.imageCount = swapchainInfo.imageCount;
        g_state.runtime.imageIndex = swapchainInfo.lastImageIndex;
        g_state.runtime.width = swapchainInfo.width;
        g_state.runtime.height = swapchainInfo.height;
        g_state.runtime.frameCount = swapchainInfo.frameCount;
        g_state.ready = IsRuntimeReadyForCapture(g_state.runtime);

        if (InterlockedIncrement(&g_runtimeRefreshLogCount) <= 24)
        {
            URH_VULKAN_LOG(
                "Runtime snapshot refreshed: swapchain=%p device=%p queue=%p family=%u format=%u ready=%d",
                reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain)),
                g_state.runtime.device,
                g_state.runtime.queue,
                g_state.runtime.queueFamilyIndex,
                g_state.runtime.swapchainFormat,
                g_state.ready ? 1 : 0);
        }
    }
}

namespace UrhVulkanHookInternal
{
    void TrackSurfaceCreated(VkInstance instance, VkSurfaceKHR surface, HWND hwnd)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        MarkBackendRecognizedLocked("TrackSurfaceCreated");
        SurfaceInfo surfaceInfo = {};
        surfaceInfo.instance = instance;
        surfaceInfo.hwnd = hwnd;
        g_state.surfaces[surface] = surfaceInfo;
    }

    void TrackSurfaceDestroyed(VkSurfaceKHR surface)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.surfaces.erase(surface);
    }

    void TrackSwapchainCreated(
        VkDevice device,
        VkFormat format,
        VkSwapchainKHR swapchain,
        VkSurfaceKHR surface,
        UINT imageCount,
        float width,
        float height)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        MarkBackendRecognizedLocked("TrackSwapchainCreated");
        HWND hwnd = nullptr;
        const auto surfaceIt = g_state.surfaces.find(surface);
        if (surfaceIt != g_state.surfaces.end())
        {
            hwnd = surfaceIt->second.hwnd;
        }

        UINT lastImageIndex = 0;
        UINT frameCount = 0;
        const auto existingIt = g_state.swapchains.find(swapchain);
        if (existingIt != g_state.swapchains.end())
        {
            lastImageIndex = existingIt->second.lastImageIndex;
            frameCount = existingIt->second.frameCount;
        }

        SwapchainInfo swapchainInfo = {};
        swapchainInfo.device = device;
        swapchainInfo.physicalDevice = nullptr;
        swapchainInfo.surface = surface;
        swapchainInfo.lastQueue = nullptr;
        swapchainInfo.queueFamilyIndex = static_cast<UINT>(VK_QUEUE_FAMILY_IGNORED);
        swapchainInfo.format = format;
        swapchainInfo.hwnd = hwnd;
        swapchainInfo.imageCount = imageCount;
        swapchainInfo.lastImageIndex = lastImageIndex;
        swapchainInfo.frameCount = frameCount;
        swapchainInfo.width = width;
        swapchainInfo.height = height;
        swapchainInfo.lateAttached = false;
        const auto deviceIt = g_state.devices.find(HandleKey(device));
        if (deviceIt != g_state.devices.end())
        {
            swapchainInfo.physicalDevice = deviceIt->second.physicalDevice;
        }
        g_state.swapchains[swapchain] = swapchainInfo;
        RefreshRuntimeFromTrackedSwapchainLocked(swapchain);
    }

    void TrackSwapchainDestroyed(VkSwapchainKHR swapchain)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        g_state.swapchains.erase(swapchain);
        if (g_state.swapchains.empty())
        {
            g_state.ready = false;
            ResetRuntime(g_state.runtime);
            g_state.setupCalled = false;
        }
    }

    void TrackSwapchainImageCount(VkSwapchainKHR swapchain, UINT imageCount)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        const auto it = g_state.swapchains.find(swapchain);
        if (it != g_state.swapchains.end())
        {
            it->second.imageCount = imageCount;
        }
    }

    void TrackAcquireImage(VkSwapchainKHR swapchain, UINT imageIndex, VkDevice device)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        MarkBackendRecognizedLocked("TrackAcquireImage");
        auto it = g_state.swapchains.find(swapchain);
        if (it == g_state.swapchains.end() && device)
        {
            SwapchainInfo swapchainInfo = {};
            swapchainInfo.device = device;
            swapchainInfo.physicalDevice = nullptr;
            swapchainInfo.surface = 0;
            swapchainInfo.lastQueue = nullptr;
            swapchainInfo.queueFamilyIndex = static_cast<UINT>(VK_QUEUE_FAMILY_IGNORED);
            swapchainInfo.format = 0;
            swapchainInfo.hwnd = FindProcessRenderWindow();
            swapchainInfo.imageCount = 0;
            swapchainInfo.lastImageIndex = imageIndex;
            swapchainInfo.frameCount = 0;
            UpdateWindowMetrics(swapchainInfo.hwnd, swapchainInfo.width, swapchainInfo.height);
            swapchainInfo.lateAttached = true;
            const auto deviceIt = g_state.devices.find(HandleKey(device));
            if (deviceIt != g_state.devices.end())
            {
                swapchainInfo.physicalDevice = deviceIt->second.physicalDevice;
            }

            g_state.swapchains[swapchain] = swapchainInfo;
            it = g_state.swapchains.find(swapchain);
        }

        if (it != g_state.swapchains.end())
        {
            it->second.lastImageIndex = imageIndex;
            if (device)
            {
                it->second.device = device;
                const auto deviceIt = g_state.devices.find(HandleKey(device));
                if (deviceIt != g_state.devices.end())
                {
                    it->second.physicalDevice = deviceIt->second.physicalDevice;
                }
            }

            RefreshRuntimeFromTrackedSwapchainLocked(swapchain);
        }
    }

    void TrackLateSwapchainDevice(VkDevice device, VkSwapchainKHR swapchain)
    {
        if (!device || !swapchain)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(g_state.mutex);
        MarkBackendRecognizedLocked("TrackLateSwapchainDevice");
        auto it = g_state.swapchains.find(swapchain);
        if (it == g_state.swapchains.end())
        {
            SwapchainInfo swapchainInfo = {};
            swapchainInfo.device = device;
            swapchainInfo.physicalDevice = nullptr;
            swapchainInfo.surface = 0;
            swapchainInfo.lastQueue = nullptr;
            swapchainInfo.queueFamilyIndex = static_cast<UINT>(VK_QUEUE_FAMILY_IGNORED);
            swapchainInfo.format = 0;
            swapchainInfo.hwnd = FindProcessRenderWindow();
            swapchainInfo.imageCount = 0;
            swapchainInfo.lastImageIndex = 0;
            swapchainInfo.frameCount = 0;
            UpdateWindowMetrics(swapchainInfo.hwnd, swapchainInfo.width, swapchainInfo.height);
            swapchainInfo.lateAttached = true;
            g_state.swapchains[swapchain] = swapchainInfo;
            it = g_state.swapchains.find(swapchain);
        }

        if (it == g_state.swapchains.end())
        {
            return;
        }

        it->second.device = device;
        const auto deviceIt = g_state.devices.find(HandleKey(device));
        if (deviceIt != g_state.devices.end())
        {
            it->second.physicalDevice = deviceIt->second.physicalDevice;
        }

        RefreshRuntimeFromTrackedSwapchainLocked(swapchain);
    }

    void TrackDeviceCreated(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties)
    {
        if (!device || !physicalDevice)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(g_state.mutex);
        MarkBackendRecognizedLocked("TrackDeviceCreated");
        DeviceInfo deviceInfo = {};
        deviceInfo.physicalDevice = physicalDevice;
        if (getPhysicalDeviceMemoryProperties)
        {
            VkPhysicalDeviceMemoryProperties memoryProperties = {};
            getPhysicalDeviceMemoryProperties(physicalDevice, &memoryProperties);
            const UINT memoryTypeCount = memoryProperties.memoryTypeCount < 32u ? memoryProperties.memoryTypeCount : 32u;
            deviceInfo.memoryTypeCount = memoryTypeCount;
            for (UINT index = 0; index < memoryTypeCount; ++index)
            {
                deviceInfo.memoryTypeFlags[index] = memoryProperties.memoryTypes[index].propertyFlags;
            }
        }

        g_state.devices[HandleKey(device)] = deviceInfo;
    }

    void TrackQueueResolved(VkQueue queue, VkDevice device, UINT queueFamilyIndex)
    {
        if (!queue)
        {
            return;
        }

        std::lock_guard<std::mutex> lock(g_state.mutex);
        MarkBackendRecognizedLocked("TrackQueueResolved");
        QueueInfo queueInfo = {};
        queueInfo.device = device;
        queueInfo.queueFamilyIndex = queueFamilyIndex;
        g_state.queues[HandleKey(queue)] = queueInfo;
    }

    bool TryBootstrapLateSwapchainLocked(VkQueue queue, VkSwapchainKHR swapchain)
    {
        const auto existingIt = g_state.swapchains.find(swapchain);
        if (existingIt != g_state.swapchains.end())
        {
            if (queue)
            {
                existingIt->second.lastQueue = queue;
            }

            return true;
        }

        HWND hwnd = FindProcessRenderWindow();
        float width = 0.0f;
        float height = 0.0f;
        if (!hwnd || UpdateWindowMetrics(hwnd, width, height) <= 0.0f)
        {
            return false;
        }

        SwapchainInfo swapchainInfo = {};
        swapchainInfo.device = nullptr;
        swapchainInfo.physicalDevice = nullptr;
        swapchainInfo.surface = 0;
        swapchainInfo.lastQueue = queue;
        swapchainInfo.queueFamilyIndex = static_cast<UINT>(VK_QUEUE_FAMILY_IGNORED);
        swapchainInfo.format = 0;
        swapchainInfo.hwnd = hwnd;
        swapchainInfo.imageCount = 0;
        swapchainInfo.lastImageIndex = 0;
        swapchainInfo.frameCount = 0;
        swapchainInfo.width = width;
        swapchainInfo.height = height;
        swapchainInfo.lateAttached = true;
        g_state.swapchains[swapchain] = swapchainInfo;
        return true;
    }

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
                    URH_VULKAN_LOG(
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
                    URH_VULKAN_LOG(
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
            const auto queueIt = g_state.queues.find(HandleKey(queue));
            if (queueIt != g_state.queues.end())
            {
                swapchainInfo.queueFamilyIndex = queueIt->second.queueFamilyIndex;
                if (!swapchainInfo.device)
                {
                    swapchainInfo.device = queueIt->second.device;
                }
                const auto deviceIt = g_state.devices.find(HandleKey(swapchainInfo.device));
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
            const auto deviceIt = g_state.devices.find(HandleKey(swapchainInfo.device));
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
                URH_VULKAN_LOG(
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
                URH_VULKAN_LOG(
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