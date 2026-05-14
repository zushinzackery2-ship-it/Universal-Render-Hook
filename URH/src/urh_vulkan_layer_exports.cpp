#include "urh_vulkan_layer_internal.h"

namespace UrhVulkanLayerInternal
{
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkGetSwapchainImagesKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint32_t* swapchainImageCount,
        void* swapchainImages)
    {
        DeviceDispatch dispatch = {};
        if (!FindDeviceDispatch(device, dispatch) || !dispatch.getSwapchainImagesKHR)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = dispatch.getSwapchainImagesKHR(device, swapchain, swapchainImageCount, swapchainImages);
        if (result == VK_SUCCESS && swapchainImageCount)
        {
            UrhVulkanHookInternal::TrackSwapchainImageCount(swapchain, *swapchainImageCount);
        }

        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkAcquireNextImageKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint64_t timeout,
        VkSemaphore semaphore,
        VkFence fence,
        std::uint32_t* imageIndex)
    {
        DeviceDispatch dispatch = {};
        if (!FindDeviceDispatch(device, dispatch) || !dispatch.acquireNextImageKHR)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result =
            dispatch.acquireNextImageKHR(device, swapchain, timeout, semaphore, fence, imageIndex);
        if (result == VK_SUCCESS && imageIndex)
        {
            UrhVulkanHookInternal::TrackAcquireImage(swapchain, *imageIndex, device);
        }

        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkAcquireNextImage2KHR(
        VkDevice device,
        const VkAcquireNextImageInfoKHR* acquireInfo,
        std::uint32_t* imageIndex)
    {
        DeviceDispatch dispatch = {};
        if (!FindDeviceDispatch(device, dispatch) || !dispatch.acquireNextImage2KHR)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        const VkResult result = dispatch.acquireNextImage2KHR(device, acquireInfo, imageIndex);
        if (result == VK_SUCCESS && acquireInfo && imageIndex)
        {
            UrhVulkanHookInternal::TrackAcquireImage(acquireInfo->swapchain, *imageIndex, device);
        }

        return result;
    }

    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* presentInfo)
    {
        {
            std::lock_guard<std::mutex> lock(UrhVulkanHookInternal::g_state.mutex);
            UrhVulkanHookInternal::MarkLayerModeActiveLocked();
        }

        DeviceDispatch dispatch = {};
        if (!FindQueueDispatch(queue, dispatch) || !dispatch.queuePresentKHR)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (InterlockedIncrement(&g_loggedQueuePresentCalls) <= 8)
        {
            URH_VULKAN_LOG(
                "Layer vkQueuePresentKHR queue=%p swapchainCount=%u",
                queue,
                presentInfo ? presentInfo->swapchainCount : 0);
        }

        if (presentInfo && presentInfo->pSwapchains)
        {
            for (UINT index = 0; index < presentInfo->swapchainCount; ++index)
            {
                const UINT imageIndex = presentInfo->pImageIndices ? presentInfo->pImageIndices[index] : 0;
                UrhVulkanHookInternal::TrackAcquireImage(presentInfo->pSwapchains[index], imageIndex);
                UrhVulkanHookInternal::DispatchPresent(queue, presentInfo->pSwapchains[index], imageIndex);
            }
        }

        return dispatch.queuePresentKHR(queue, presentInfo);
    }

    VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* name)
    {
        if (!name)
        {
            return nullptr;
        }

        if (lstrcmpiA(name, "vkGetInstanceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkGetInstanceProcAddr);
        if (lstrcmpiA(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkGetDeviceProcAddr);
        if (lstrcmpiA(name, "vkCreateInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkCreateInstance);
        if (lstrcmpiA(name, "vkCreateDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkCreateDevice);
        if (lstrcmpiA(name, "vkEnumerateInstanceLayerProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkEnumerateInstanceLayerProperties);
        if (lstrcmpiA(name, "vkEnumerateInstanceExtensionProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkEnumerateInstanceExtensionProperties);
        if (lstrcmpiA(name, "vkEnumerateDeviceLayerProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkEnumerateDeviceLayerProperties);
        if (lstrcmpiA(name, "vkEnumerateDeviceExtensionProperties") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkEnumerateDeviceExtensionProperties);
        if (lstrcmpiA(name, "vkEnumeratePhysicalDevices") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkEnumeratePhysicalDevices);
        if (lstrcmpiA(name, "vkDestroyInstance") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroyInstance);
        if (lstrcmpiA(name, "vkCreateWin32SurfaceKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkCreateWin32SurfaceKHR);
        if (lstrcmpiA(name, "vkDestroySurfaceKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroySurfaceKHR);
        if (lstrcmpiA(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceQueue);
        if (lstrcmpiA(name, "vkGetDeviceQueue2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceQueue2);
        if (lstrcmpiA(name, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkCreateSwapchainKHR);
        if (lstrcmpiA(name, "vkDestroySwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroySwapchainKHR);
        if (lstrcmpiA(name, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetSwapchainImagesKHR);
        if (lstrcmpiA(name, "vkAcquireNextImageKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkAcquireNextImageKHR);
        if (lstrcmpiA(name, "vkAcquireNextImage2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkAcquireNextImage2KHR);
        if (lstrcmpiA(name, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkQueuePresentKHR);

        InstanceDispatch dispatch = {};
        return (instance && FindInstanceDispatch(instance, dispatch) && dispatch.gipa)
            ? dispatch.gipa(instance, name)
            : GetLoaderExport<PFN_vkVoidFunction>(name);
    }

    VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* name)
    {
        if (!name)
        {
            return nullptr;
        }

        if (lstrcmpiA(name, "vkGetDeviceProcAddr") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&vkGetDeviceProcAddr);
        if (lstrcmpiA(name, "vkDestroyDevice") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroyDevice);
        if (lstrcmpiA(name, "vkGetDeviceQueue") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceQueue);
        if (lstrcmpiA(name, "vkGetDeviceQueue2") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetDeviceQueue2);
        if (lstrcmpiA(name, "vkCreateSwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkCreateSwapchainKHR);
        if (lstrcmpiA(name, "vkDestroySwapchainKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkDestroySwapchainKHR);
        if (lstrcmpiA(name, "vkGetSwapchainImagesKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkGetSwapchainImagesKHR);
        if (lstrcmpiA(name, "vkAcquireNextImageKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkAcquireNextImageKHR);
        if (lstrcmpiA(name, "vkAcquireNextImage2KHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkAcquireNextImage2KHR);
        if (lstrcmpiA(name, "vkQueuePresentKHR") == 0) return reinterpret_cast<PFN_vkVoidFunction>(&Layer_vkQueuePresentKHR);

        DeviceDispatch dispatch = {};
        return (device && FindDeviceDispatch(device, dispatch) && dispatch.gdpa)
            ? dispatch.gdpa(device, name)
            : GetLoaderExport<PFN_vkVoidFunction>(name);
    }

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
    vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterfaceLocal* versionStruct)
    {
        URH_VULKAN_LOG("Layer negotiate interface entered");
        if (!versionStruct || versionStruct->loaderLayerInterfaceVersion < 2)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        versionStruct->loaderLayerInterfaceVersion = 2;
        versionStruct->pfnGetInstanceProcAddr = vkGetInstanceProcAddr;
        versionStruct->pfnGetDeviceProcAddr = vkGetDeviceProcAddr;
        versionStruct->pfnGetPhysicalDeviceProcAddr = nullptr;
        return VK_SUCCESS;
    }
}
