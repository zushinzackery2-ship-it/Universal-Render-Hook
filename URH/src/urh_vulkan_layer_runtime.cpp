#include "urh_vulkan_layer_internal.h"

namespace UrhVulkanLayerInternal
{
    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator)
    {
        InstanceDispatch dispatch = {};
        if (FindInstanceDispatch(instance, dispatch) && dispatch.destroyInstance)
        {
            dispatch.destroyInstance(instance, allocator);
        }

        std::lock_guard<std::mutex> lock(g_layerMutex);
        g_instances.erase(HandleKey(instance));
        for (auto it = g_physicalDeviceOwners.begin(); it != g_physicalDeviceOwners.end();)
        {
            it = (it->second == instance) ? g_physicalDeviceOwners.erase(it) : std::next(it);
        }
    }

    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator)
    {
        DeviceDispatch dispatch = {};
        if (FindDeviceDispatch(device, dispatch) && dispatch.destroyDevice)
        {
            dispatch.destroyDevice(device, allocator);
        }

        std::lock_guard<std::mutex> lock(g_layerMutex);
        g_devices.erase(HandleKey(device));
        for (auto it = g_queues.begin(); it != g_queues.end();)
        {
            it = (it->second == device) ? g_queues.erase(it) : std::next(it);
        }
    }

    VKAPI_ATTR void VKAPI_CALL Layer_vkGetDeviceQueue(
        VkDevice device,
        std::uint32_t queueFamilyIndex,
        std::uint32_t queueIndex,
        VkQueue* queue)
    {
        DeviceDispatch dispatch = {};
        if (!FindDeviceDispatch(device, dispatch) || !dispatch.getDeviceQueue)
        {
            return;
        }

        dispatch.getDeviceQueue(device, queueFamilyIndex, queueIndex, queue);
        if (queue && *queue)
        {
            UrhVulkanHookInternal::TrackQueueResolved(*queue, device, queueFamilyIndex);
            URH_VULKAN_LOG(
                "Layer vkGetDeviceQueue mapped queue=%p device=%p family=%u index=%u",
                *queue,
                device,
                queueFamilyIndex,
                queueIndex);
            std::lock_guard<std::mutex> lock(g_layerMutex);
            g_queues[HandleKey(*queue)] = device;
        }
    }

    VKAPI_ATTR void VKAPI_CALL Layer_vkGetDeviceQueue2(
        VkDevice device,
        const VkDeviceQueueInfo2* queueInfo,
        VkQueue* queue)
    {
        DeviceDispatch dispatch = {};
        if (!FindDeviceDispatch(device, dispatch) || !dispatch.getDeviceQueue2)
        {
            return;
        }

        dispatch.getDeviceQueue2(device, queueInfo, queue);
        if (queue && *queue && queueInfo)
        {
            UrhVulkanHookInternal::TrackQueueResolved(*queue, device, queueInfo->queueFamilyIndex);
            URH_VULKAN_LOG(
                "Layer vkGetDeviceQueue2 mapped queue=%p device=%p family=%u index=%u",
                *queue,
                device,
                queueInfo->queueFamilyIndex,
                queueInfo->queueIndex);
            std::lock_guard<std::mutex> lock(g_layerMutex);
            g_queues[HandleKey(*queue)] = device;
        }
    }

    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkCreateWin32SurfaceKHR(
        VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSurfaceKHR* surface)
    {
        InstanceDispatch dispatch = {};
        if (!FindInstanceDispatch(instance, dispatch) || !dispatch.createWin32SurfaceKHR)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = dispatch.createWin32SurfaceKHR(instance, createInfo, allocator, surface);
        if (result == VK_SUCCESS && createInfo && surface)
        {
            UrhVulkanHookInternal::TrackSurfaceCreated(instance, *surface, createInfo->hwnd);
        }

        return result;
    }

    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroySurfaceKHR(
        VkInstance instance,
        VkSurfaceKHR surface,
        const VkAllocationCallbacks* allocator)
    {
        InstanceDispatch dispatch = {};
        if (FindInstanceDispatch(instance, dispatch) && dispatch.destroySurfaceKHR)
        {
            dispatch.destroySurfaceKHR(instance, surface, allocator);
        }

        UrhVulkanHookInternal::TrackSurfaceDestroyed(surface);
    }

    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkCreateSwapchainKHR(
        VkDevice device,
        const VkSwapchainCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSwapchainKHR* swapchain)
    {
        DeviceDispatch dispatch = {};
        if (!FindDeviceDispatch(device, dispatch) || !dispatch.createSwapchainKHR)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = dispatch.createSwapchainKHR(device, createInfo, allocator, swapchain);
        if (result == VK_SUCCESS && createInfo && swapchain)
        {
            UrhVulkanHookInternal::TrackSwapchainCreated(
                device,
                createInfo->imageFormat,
                *swapchain,
                createInfo->surface,
                createInfo->minImageCount,
                static_cast<float>(createInfo->imageExtent.width),
                static_cast<float>(createInfo->imageExtent.height));
        }

        return result;
    }

    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroySwapchainKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        const VkAllocationCallbacks* allocator)
    {
        DeviceDispatch dispatch = {};
        if (FindDeviceDispatch(device, dispatch) && dispatch.destroySwapchainKHR)
        {
            dispatch.destroySwapchainKHR(device, swapchain, allocator);
        }

        UrhVulkanHookInternal::TrackSwapchainDestroyed(swapchain);
    }
}
