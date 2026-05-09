#include "urh_vulkan_internal.h"

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
        const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(device));
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
            const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(device));
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
                const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(device));
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
        const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(device));
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

        g_state.devices[UrhVulkanCommon::HandleKey(device)] = deviceInfo;
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
        g_state.queues[UrhVulkanCommon::HandleKey(queue)] = queueInfo;
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
}