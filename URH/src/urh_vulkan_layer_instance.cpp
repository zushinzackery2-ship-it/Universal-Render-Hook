#include "urh_vulkan_layer_internal.h"

namespace UrhVulkanLayerInternal
{
    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
        VkPhysicalDevice,
        std::uint32_t* propertyCount,
        VkLayerPropertiesLocal* properties)
    {
        return vkEnumerateInstanceLayerProperties(propertyCount, properties);
    }

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
        VkPhysicalDevice physicalDevice,
        const char* layerName,
        std::uint32_t* propertyCount,
        VkExtensionPropertiesLocal* properties)
    {
        if (!propertyCount)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (layerName && lstrcmpiA(layerName, LayerName) == 0)
        {
            *propertyCount = 0;
            return VK_SUCCESS;
        }

        const VkInstance instance = ResolveOwningInstance(physicalDevice);

        InstanceDispatch dispatch = {};
        return (instance && FindInstanceDispatch(instance, dispatch) && dispatch.enumerateDeviceExtensionProperties)
            ? dispatch.enumerateDeviceExtensionProperties(physicalDevice, layerName, propertyCount, properties)
            : VK_ERROR_INITIALIZATION_FAILED;
    }

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
        const VkInstanceCreateInfo* createInfo,
        const VkAllocationCallbacks* allocator,
        VkInstance* instance)
    {
        URH_VULKANHOOK_LOG("Layer vkCreateInstance entered");
        {
            std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
            UrhVulkanHookInternal::MarkLayerModeActiveLocked();
        }

        auto* chain = GetLayerChainInfo(createInfo);
        if (!chain || !chain->u.pLayerInfo)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        PFN_vkGetInstanceProcAddr nextGipa = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
        PFN_vkCreateInstance nextCreateInstance =
            nextGipa ? reinterpret_cast<PFN_vkCreateInstance>(nextGipa(nullptr, "vkCreateInstance")) : nullptr;
        if (!nextCreateInstance)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;
        const VkResult result = nextCreateInstance(createInfo, allocator, instance);
        if (result == VK_SUCCESS && instance && *instance)
        {
            InstanceDispatch dispatch = {};
            dispatch.gipa = nextGipa;
            dispatch.destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(nextGipa(*instance, "vkDestroyInstance"));
            dispatch.enumeratePhysicalDevices =
                reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(nextGipa(*instance, "vkEnumeratePhysicalDevices"));
            dispatch.createDevice = reinterpret_cast<PFN_vkCreateDevice>(nextGipa(*instance, "vkCreateDevice"));
            dispatch.createWin32SurfaceKHR =
                reinterpret_cast<PFN_vkCreateWin32SurfaceKHR>(nextGipa(*instance, "vkCreateWin32SurfaceKHR"));
            dispatch.destroySurfaceKHR =
                reinterpret_cast<PFN_vkDestroySurfaceKHR>(nextGipa(*instance, "vkDestroySurfaceKHR"));
            dispatch.enumerateDeviceExtensionProperties = reinterpret_cast<PFN_vkEnumerateDeviceExtensionProperties>(
                nextGipa(*instance, "vkEnumerateDeviceExtensionProperties"));

            std::lock_guard<std::mutex> lock(g_layerMutex);
            g_instances[UrhVulkanCommon::HandleKey(*instance)] = dispatch;
        }

        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkEnumeratePhysicalDevices(
        VkInstance instance,
        std::uint32_t* physicalDeviceCount,
        VkPhysicalDevice* physicalDevices)
    {
        InstanceDispatch dispatch = {};
        if (!FindInstanceDispatch(instance, dispatch) || !dispatch.enumeratePhysicalDevices)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = dispatch.enumeratePhysicalDevices(instance, physicalDeviceCount, physicalDevices);
        if (result == VK_SUCCESS && physicalDeviceCount && physicalDevices)
        {
            std::lock_guard<std::mutex> lock(g_layerMutex);
            for (std::uint32_t index = 0; index < *physicalDeviceCount; ++index)
            {
                g_physicalDeviceOwners[UrhVulkanCommon::HandleKey(physicalDevices[index])] = instance;
            }
        }

        return result;
    }

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* createInfo,
        const VkAllocationCallbacks* allocator,
        VkDevice* device)
    {
        URH_VULKANHOOK_LOG("Layer vkCreateDevice entered");
        {
            std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
            UrhVulkanHookInternal::MarkLayerModeActiveLocked();
        }

        auto* chain = GetLayerChainInfo(createInfo);
        if (!chain || !chain->u.pLayerInfo)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        VkInstance instance = ResolveOwningInstance(physicalDevice);
        PFN_vkGetInstanceProcAddr nextGipa = chain->u.pLayerInfo->pfnNextGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr nextGdpa = chain->u.pLayerInfo->pfnNextGetDeviceProcAddr;
        PFN_vkCreateDevice nextCreateDevice = nextGipa
            ? reinterpret_cast<PFN_vkCreateDevice>(nextGipa(instance, "vkCreateDevice"))
            : nullptr;
        if (!nextCreateDevice)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        chain->u.pLayerInfo = chain->u.pLayerInfo->pNext;
        const VkResult result = nextCreateDevice(physicalDevice, createInfo, allocator, device);
        if (result == VK_SUCCESS && device && *device)
        {
            UrhVulkanHookInternal::TrackDeviceCreated(
                *device,
                physicalDevice,
                reinterpret_cast<PFN_vkGetPhysicalDeviceMemoryProperties>(
                    nextGipa(instance, "vkGetPhysicalDeviceMemoryProperties")));
            DeviceDispatch dispatch = {};
            dispatch.instance = instance;
            dispatch.gdpa = nextGdpa;
            dispatch.destroyDevice = reinterpret_cast<PFN_vkDestroyDevice>(nextGdpa(*device, "vkDestroyDevice"));
            dispatch.getDeviceQueue = reinterpret_cast<PFN_vkGetDeviceQueue>(nextGdpa(*device, "vkGetDeviceQueue"));
            dispatch.getDeviceQueue2 = reinterpret_cast<PFN_vkGetDeviceQueue2>(nextGdpa(*device, "vkGetDeviceQueue2"));
            dispatch.createSwapchainKHR =
                reinterpret_cast<PFN_vkCreateSwapchainKHR>(nextGdpa(*device, "vkCreateSwapchainKHR"));
            dispatch.destroySwapchainKHR =
                reinterpret_cast<PFN_vkDestroySwapchainKHR>(nextGdpa(*device, "vkDestroySwapchainKHR"));
            dispatch.getSwapchainImagesKHR =
                reinterpret_cast<PFN_vkGetSwapchainImagesKHR>(nextGdpa(*device, "vkGetSwapchainImagesKHR"));
            dispatch.acquireNextImageKHR =
                reinterpret_cast<PFN_vkAcquireNextImageKHR>(nextGdpa(*device, "vkAcquireNextImageKHR"));
            dispatch.acquireNextImage2KHR =
                reinterpret_cast<PFN_vkAcquireNextImage2KHR>(nextGdpa(*device, "vkAcquireNextImage2KHR"));
            dispatch.queuePresentKHR = reinterpret_cast<PFN_vkQueuePresentKHR>(nextGdpa(*device, "vkQueuePresentKHR"));

            std::lock_guard<std::mutex> lock(g_layerMutex);
            g_devices[UrhVulkanCommon::HandleKey(*device)] = dispatch;
        }

        return result;
    }
}
