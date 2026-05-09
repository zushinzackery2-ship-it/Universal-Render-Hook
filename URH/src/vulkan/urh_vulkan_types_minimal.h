#pragma once

#include <Windows.h>

#include <cstdint>

#ifndef VKAPI_CALL
#define VKAPI_CALL __stdcall
#endif

#ifndef VKAPI_PTR
#define VKAPI_PTR VKAPI_CALL
#endif

#ifndef VKAPI_ATTR
#define VKAPI_ATTR
#endif

typedef std::uint32_t VkFlags;
typedef std::uint32_t VkBool32;
typedef std::uint32_t VkFormat;
typedef std::uint32_t VkStructureType;
typedef std::uint32_t VkColorSpaceKHR;
typedef std::uint32_t VkImageUsageFlags;
typedef std::uint32_t VkBufferUsageFlags;
typedef std::uint32_t VkImageAspectFlags;
typedef std::uint32_t VkAccessFlags;
typedef std::uint32_t VkPipelineStageFlags;
typedef std::uint32_t VkImageLayout;
typedef std::uint32_t VkCommandPoolCreateFlags;
typedef std::uint32_t VkCommandBufferLevel;
typedef std::uint32_t VkCommandBufferUsageFlags;
typedef std::uint32_t VkMemoryPropertyFlags;
typedef std::uint32_t VkSharingMode;
typedef std::uint32_t VkSurfaceTransformFlagBitsKHR;
typedef std::uint32_t VkCompositeAlphaFlagBitsKHR;
typedef std::uint32_t VkPresentModeKHR;
typedef std::int32_t VkResult;
typedef std::uint64_t VkDeviceSize;
typedef std::uint64_t VkSurfaceKHR;
typedef std::uint64_t VkSwapchainKHR;
typedef std::uint64_t VkImage;
typedef std::uint64_t VkBuffer;
typedef std::uint64_t VkDeviceMemory;
typedef std::uint64_t VkSemaphore;
typedef std::uint64_t VkFence;
typedef std::uint64_t VkCommandPool;
typedef std::uint32_t VkQueueFlags;

typedef struct VkInstance_T* VkInstance;
typedef struct VkPhysicalDevice_T* VkPhysicalDevice;
typedef struct VkDevice_T* VkDevice;
typedef struct VkQueue_T* VkQueue;
typedef struct VkCommandBuffer_T* VkCommandBuffer;
typedef struct VkAllocationCallbacks VkAllocationCallbacks;

typedef void (VKAPI_PTR* PFN_vkVoidFunction)(void);

enum : VkResult
{
    VK_SUCCESS = 0,
    VK_NOT_READY = 1,
    VK_TIMEOUT = 2,
    VK_ERROR_INITIALIZATION_FAILED = -3
};

enum : VkStructureType
{
    VK_STRUCTURE_TYPE_APPLICATION_INFO = 0,
    VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO = 1,
    VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO = 2,
    VK_STRUCTURE_TYPE_DEVICE_QUEUE_INFO_2 = 1000145003,
    VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO = 3,
    VK_STRUCTURE_TYPE_SUBMIT_INFO = 4,
    VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO = 5,
    VK_STRUCTURE_TYPE_MAPPED_MEMORY_RANGE = 6,
    VK_STRUCTURE_TYPE_FENCE_CREATE_INFO = 8,
    VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO = 12,
    VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO = 47,
    VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO = 48,
    VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER = 45,
    VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO = 39,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO = 40,
    VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO = 42,
    VK_STRUCTURE_TYPE_PRESENT_INFO_KHR = 1000001001,
    VK_STRUCTURE_TYPE_WIN32_SURFACE_CREATE_INFO_KHR = 1000009000,
    VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR = 1000001000,
    VK_STRUCTURE_TYPE_ACQUIRE_NEXT_IMAGE_INFO_KHR = 1000060010
};

enum : VkQueueFlags
{
    VK_QUEUE_GRAPHICS_BIT = 0x00000001,
    VK_QUEUE_COMPUTE_BIT = 0x00000002,
    VK_QUEUE_TRANSFER_BIT = 0x00000004
};

enum : VkImageUsageFlags
{
    VK_IMAGE_USAGE_TRANSFER_SRC_BIT = 0x00000001,
    VK_IMAGE_USAGE_TRANSFER_DST_BIT = 0x00000002,
    VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT = 0x00000010
};

enum : VkBufferUsageFlags
{
    VK_BUFFER_USAGE_TRANSFER_DST_BIT = 0x00000002
};

enum : VkImageAspectFlags
{
    VK_IMAGE_ASPECT_COLOR_BIT = 0x00000001
};

enum : VkImageLayout
{
    VK_IMAGE_LAYOUT_UNDEFINED = 0,
    VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL = 6,
    VK_IMAGE_LAYOUT_PRESENT_SRC_KHR = 1000001002
};

enum : VkPipelineStageFlags
{
    VK_PIPELINE_STAGE_TRANSFER_BIT = 0x00001000
};

enum : VkAccessFlags
{
    VK_ACCESS_TRANSFER_READ_BIT = 0x00000800
};

enum : VkCommandPoolCreateFlags
{
    VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT = 0x00000002
};

enum : VkCommandBufferLevel
{
    VK_COMMAND_BUFFER_LEVEL_PRIMARY = 0
};

enum : VkCommandBufferUsageFlags
{
    VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT = 0x00000001
};

enum : VkMemoryPropertyFlags
{
    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT = 0x00000002,
    VK_MEMORY_PROPERTY_HOST_COHERENT_BIT = 0x00000004,
    VK_MEMORY_PROPERTY_HOST_CACHED_BIT = 0x00000008
};

enum : VkSharingMode
{
    VK_SHARING_MODE_EXCLUSIVE = 0
};

enum : VkFormat
{
    VK_FORMAT_R8G8B8A8_UNORM = 37,
    VK_FORMAT_R8G8B8A8_SRGB = 43,
    VK_FORMAT_B8G8R8A8_UNORM = 44,
    VK_FORMAT_B8G8R8A8_SRGB = 50
};

enum
{
    VK_TRUE = 1,
    VK_FALSE = 0,
    VK_QUEUE_FAMILY_IGNORED = ~0u,
    VK_FENCE_CREATE_SIGNALED_BIT = 0x00000001
};

struct VkExtent2D
{
    std::uint32_t width;
    std::uint32_t height;
};

struct VkExtent3D
{
    std::uint32_t width;
    std::uint32_t height;
    std::uint32_t depth;
};

struct VkOffset3D
{
    std::int32_t x;
    std::int32_t y;
    std::int32_t z;
};

struct VkApplicationInfo
{
    VkStructureType sType;
    const void* pNext;
    const char* pApplicationName;
    std::uint32_t applicationVersion;
    const char* pEngineName;
    std::uint32_t engineVersion;
    std::uint32_t apiVersion;
};

struct VkInstanceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    const VkApplicationInfo* pApplicationInfo;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
};

struct VkDeviceQueueCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueFamilyIndex;
    std::uint32_t queueCount;
    const float* pQueuePriorities;
};

struct VkDeviceQueueInfo2
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueFamilyIndex;
    std::uint32_t queueIndex;
};

struct VkDeviceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    std::uint32_t queueCreateInfoCount;
    const VkDeviceQueueCreateInfo* pQueueCreateInfos;
    std::uint32_t enabledLayerCount;
    const char* const* ppEnabledLayerNames;
    std::uint32_t enabledExtensionCount;
    const char* const* ppEnabledExtensionNames;
    const void* pEnabledFeatures;
};

struct VkQueueFamilyProperties
{
    VkQueueFlags queueFlags;
    std::uint32_t queueCount;
    std::uint32_t timestampValidBits;
    VkExtent3D minImageTransferGranularity;
};

struct VkMemoryType
{
    VkMemoryPropertyFlags propertyFlags;
    std::uint32_t heapIndex;
};

struct VkMemoryHeap
{
    VkDeviceSize size;
    VkFlags flags;
};

struct VkPhysicalDeviceMemoryProperties
{
    std::uint32_t memoryTypeCount;
    VkMemoryType memoryTypes[32];
    std::uint32_t memoryHeapCount;
    VkMemoryHeap memoryHeaps[16];
};

struct VkMemoryRequirements
{
    VkDeviceSize size;
    VkDeviceSize alignment;
    std::uint32_t memoryTypeBits;
};

struct VkWin32SurfaceCreateInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    HINSTANCE hinstance;
    HWND hwnd;
};

struct VkSwapchainCreateInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkSurfaceKHR surface;
    std::uint32_t minImageCount;
    VkFormat imageFormat;
    VkColorSpaceKHR imageColorSpace;
    VkExtent2D imageExtent;
    std::uint32_t imageArrayLayers;
    VkImageUsageFlags imageUsage;
    VkSharingMode imageSharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
    VkSurfaceTransformFlagBitsKHR preTransform;
    VkCompositeAlphaFlagBitsKHR compositeAlpha;
    VkPresentModeKHR presentMode;
    VkBool32 clipped;
    VkSwapchainKHR oldSwapchain;
};

struct VkPresentInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    std::uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    std::uint32_t swapchainCount;
    const VkSwapchainKHR* pSwapchains;
    const std::uint32_t* pImageIndices;
    VkResult* pResults;
};

struct VkAcquireNextImageInfoKHR
{
    VkStructureType sType;
    const void* pNext;
    VkSwapchainKHR swapchain;
    std::uint64_t timeout;
    VkSemaphore semaphore;
    VkFence fence;
    std::uint32_t deviceMask;
};

struct VkBufferCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
    VkDeviceSize size;
    VkBufferUsageFlags usage;
    VkSharingMode sharingMode;
    std::uint32_t queueFamilyIndexCount;
    const std::uint32_t* pQueueFamilyIndices;
};

struct VkMemoryAllocateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkDeviceSize allocationSize;
    std::uint32_t memoryTypeIndex;
};

struct VkMappedMemoryRange
{
    VkStructureType sType;
    const void* pNext;
    VkDeviceMemory memory;
    VkDeviceSize offset;
    VkDeviceSize size;
};

struct VkCommandPoolCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandPoolCreateFlags flags;
    std::uint32_t queueFamilyIndex;
};

struct VkCommandBufferAllocateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandPool commandPool;
    VkCommandBufferLevel level;
    std::uint32_t commandBufferCount;
};

struct VkCommandBufferBeginInfo
{
    VkStructureType sType;
    const void* pNext;
    VkCommandBufferUsageFlags flags;
    const void* pInheritanceInfo;
};

struct VkImageSubresourceRange
{
    VkImageAspectFlags aspectMask;
    std::uint32_t baseMipLevel;
    std::uint32_t levelCount;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
};

struct VkImageSubresourceLayers
{
    VkImageAspectFlags aspectMask;
    std::uint32_t mipLevel;
    std::uint32_t baseArrayLayer;
    std::uint32_t layerCount;
};

struct VkBufferImageCopy
{
    VkDeviceSize bufferOffset;
    std::uint32_t bufferRowLength;
    std::uint32_t bufferImageHeight;
    VkImageSubresourceLayers imageSubresource;
    VkOffset3D imageOffset;
    VkExtent3D imageExtent;
};

struct VkImageMemoryBarrier
{
    VkStructureType sType;
    const void* pNext;
    VkAccessFlags srcAccessMask;
    VkAccessFlags dstAccessMask;
    VkImageLayout oldLayout;
    VkImageLayout newLayout;
    std::uint32_t srcQueueFamilyIndex;
    std::uint32_t dstQueueFamilyIndex;
    VkImage image;
    VkImageSubresourceRange subresourceRange;
};

struct VkSubmitInfo
{
    VkStructureType sType;
    const void* pNext;
    std::uint32_t waitSemaphoreCount;
    const VkSemaphore* pWaitSemaphores;
    const VkPipelineStageFlags* pWaitDstStageMask;
    std::uint32_t commandBufferCount;
    const VkCommandBuffer* pCommandBuffers;
    std::uint32_t signalSemaphoreCount;
    const VkSemaphore* pSignalSemaphores;
};

struct VkFenceCreateInfo
{
    VkStructureType sType;
    const void* pNext;
    VkFlags flags;
};
