#pragma once

#include "urh_vulkan_types_minimal.h"

typedef PFN_vkVoidFunction (VKAPI_PTR* PFN_vkGetInstanceProcAddr)(
    VkInstance instance,
    const char* pName);

typedef PFN_vkVoidFunction (VKAPI_PTR* PFN_vkGetDeviceProcAddr)(
    VkDevice device,
    const char* pName);

typedef VkResult (VKAPI_PTR* PFN_vkCreateInstance)(
    const VkInstanceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkInstance* pInstance);

typedef VkResult (VKAPI_PTR* PFN_vkEnumeratePhysicalDevices)(
    VkInstance instance,
    std::uint32_t* pPhysicalDeviceCount,
    VkPhysicalDevice* pPhysicalDevices);

typedef void (VKAPI_PTR* PFN_vkGetPhysicalDeviceQueueFamilyProperties)(
    VkPhysicalDevice physicalDevice,
    std::uint32_t* pQueueFamilyPropertyCount,
    VkQueueFamilyProperties* pQueueFamilyProperties);

typedef void (VKAPI_PTR* PFN_vkGetPhysicalDeviceMemoryProperties)(
    VkPhysicalDevice physicalDevice,
    VkPhysicalDeviceMemoryProperties* pMemoryProperties);

typedef VkResult (VKAPI_PTR* PFN_vkCreateDevice)(
    VkPhysicalDevice physicalDevice,
    const VkDeviceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDevice* pDevice);

typedef void (VKAPI_PTR* PFN_vkGetDeviceQueue)(
    VkDevice device,
    std::uint32_t queueFamilyIndex,
    std::uint32_t queueIndex,
    VkQueue* pQueue);

typedef void (VKAPI_PTR* PFN_vkGetDeviceQueue2)(
    VkDevice device,
    const VkDeviceQueueInfo2* pQueueInfo,
    VkQueue* pQueue);

typedef void (VKAPI_PTR* PFN_vkDestroyDevice)(
    VkDevice device,
    const VkAllocationCallbacks* pAllocator);

typedef void (VKAPI_PTR* PFN_vkDestroyInstance)(
    VkInstance instance,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR* PFN_vkCreateWin32SurfaceKHR)(
    VkInstance instance,
    const VkWin32SurfaceCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSurfaceKHR* pSurface);

typedef void (VKAPI_PTR* PFN_vkDestroySurfaceKHR)(
    VkInstance instance,
    VkSurfaceKHR surface,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR* PFN_vkCreateSwapchainKHR)(
    VkDevice device,
    const VkSwapchainCreateInfoKHR* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkSwapchainKHR* pSwapchain);

typedef void (VKAPI_PTR* PFN_vkDestroySwapchainKHR)(
    VkDevice device,
    VkSwapchainKHR swapchain,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR* PFN_vkGetSwapchainImagesKHR)(
    VkDevice device,
    VkSwapchainKHR swapchain,
    std::uint32_t* pSwapchainImageCount,
    void* pSwapchainImages);

typedef VkResult (VKAPI_PTR* PFN_vkCreateBuffer)(
    VkDevice device,
    const VkBufferCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkBuffer* pBuffer);

typedef void (VKAPI_PTR* PFN_vkDestroyBuffer)(
    VkDevice device,
    VkBuffer buffer,
    const VkAllocationCallbacks* pAllocator);

typedef void (VKAPI_PTR* PFN_vkGetBufferMemoryRequirements)(
    VkDevice device,
    VkBuffer buffer,
    VkMemoryRequirements* pMemoryRequirements);

typedef VkResult (VKAPI_PTR* PFN_vkAllocateMemory)(
    VkDevice device,
    const VkMemoryAllocateInfo* pAllocateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkDeviceMemory* pMemory);

typedef void (VKAPI_PTR* PFN_vkFreeMemory)(
    VkDevice device,
    VkDeviceMemory memory,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR* PFN_vkBindBufferMemory)(
    VkDevice device,
    VkBuffer buffer,
    VkDeviceMemory memory,
    VkDeviceSize memoryOffset);

typedef VkResult (VKAPI_PTR* PFN_vkMapMemory)(
    VkDevice device,
    VkDeviceMemory memory,
    VkDeviceSize offset,
    VkDeviceSize size,
    VkFlags flags,
    void** ppData);

typedef void (VKAPI_PTR* PFN_vkUnmapMemory)(
    VkDevice device,
    VkDeviceMemory memory);

typedef VkResult (VKAPI_PTR* PFN_vkInvalidateMappedMemoryRanges)(
    VkDevice device,
    std::uint32_t memoryRangeCount,
    const VkMappedMemoryRange* pMemoryRanges);

typedef VkResult (VKAPI_PTR* PFN_vkCreateCommandPool)(
    VkDevice device,
    const VkCommandPoolCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkCommandPool* pCommandPool);

typedef void (VKAPI_PTR* PFN_vkDestroyCommandPool)(
    VkDevice device,
    VkCommandPool commandPool,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR* PFN_vkAllocateCommandBuffers)(
    VkDevice device,
    const VkCommandBufferAllocateInfo* pAllocateInfo,
    VkCommandBuffer* pCommandBuffers);

typedef VkResult (VKAPI_PTR* PFN_vkBeginCommandBuffer)(
    VkCommandBuffer commandBuffer,
    const VkCommandBufferBeginInfo* pBeginInfo);

typedef VkResult (VKAPI_PTR* PFN_vkEndCommandBuffer)(
    VkCommandBuffer commandBuffer);

typedef VkResult (VKAPI_PTR* PFN_vkResetCommandBuffer)(
    VkCommandBuffer commandBuffer,
    VkFlags flags);

typedef VkResult (VKAPI_PTR* PFN_vkCreateFence)(
    VkDevice device,
    const VkFenceCreateInfo* pCreateInfo,
    const VkAllocationCallbacks* pAllocator,
    VkFence* pFence);

typedef void (VKAPI_PTR* PFN_vkDestroyFence)(
    VkDevice device,
    VkFence fence,
    const VkAllocationCallbacks* pAllocator);

typedef VkResult (VKAPI_PTR* PFN_vkWaitForFences)(
    VkDevice device,
    std::uint32_t fenceCount,
    const VkFence* pFences,
    VkBool32 waitAll,
    std::uint64_t timeout);

typedef VkResult (VKAPI_PTR* PFN_vkResetFences)(
    VkDevice device,
    std::uint32_t fenceCount,
    const VkFence* pFences);

typedef VkResult (VKAPI_PTR* PFN_vkAcquireNextImageKHR)(
    VkDevice device,
    VkSwapchainKHR swapchain,
    std::uint64_t timeout,
    VkSemaphore semaphore,
    VkFence fence,
    std::uint32_t* pImageIndex);

typedef VkResult (VKAPI_PTR* PFN_vkAcquireNextImage2KHR)(
    VkDevice device,
    const VkAcquireNextImageInfoKHR* pAcquireInfo,
    std::uint32_t* pImageIndex);

typedef void (VKAPI_PTR* PFN_vkCmdPipelineBarrier)(
    VkCommandBuffer commandBuffer,
    VkPipelineStageFlags srcStageMask,
    VkPipelineStageFlags dstStageMask,
    VkFlags dependencyFlags,
    std::uint32_t memoryBarrierCount,
    const void* pMemoryBarriers,
    std::uint32_t bufferMemoryBarrierCount,
    const void* pBufferMemoryBarriers,
    std::uint32_t imageMemoryBarrierCount,
    const VkImageMemoryBarrier* pImageMemoryBarriers);

typedef void (VKAPI_PTR* PFN_vkCmdCopyImageToBuffer)(
    VkCommandBuffer commandBuffer,
    VkImage srcImage,
    VkImageLayout srcImageLayout,
    VkBuffer dstBuffer,
    std::uint32_t regionCount,
    const VkBufferImageCopy* pRegions);

typedef VkResult (VKAPI_PTR* PFN_vkQueueSubmit)(
    VkQueue queue,
    std::uint32_t submitCount,
    const VkSubmitInfo* pSubmits,
    VkFence fence);

typedef VkResult (VKAPI_PTR* PFN_vkQueueWaitIdle)(
    VkQueue queue);

typedef VkResult (VKAPI_PTR* PFN_vkQueuePresentKHR)(
    VkQueue queue,
    const VkPresentInfoKHR* pPresentInfo);
