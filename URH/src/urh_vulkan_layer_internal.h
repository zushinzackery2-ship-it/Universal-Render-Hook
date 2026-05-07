#pragma once

#include "urh_vulkan_internal.h"

#include <unordered_map>

#define VK_LAYER_EXPORT extern "C" __declspec(dllexport)

namespace UrhVulkanLayerInternal
{
    inline constexpr char LayerName[] = "VK_LAYER_PVRC_capture";
    inline constexpr char LayerDescription[] = "PVRC Vulkan capture layer";
    inline constexpr std::uint32_t VkMaxNameSize = 256;
    inline constexpr std::uint32_t VkMaxDescriptionSize = 256;

    struct VkLayerPropertiesLocal
    {
        char layerName[VkMaxNameSize];
        std::uint32_t specVersion;
        std::uint32_t implementationVersion;
        char description[VkMaxDescriptionSize];
    };

    struct VkExtensionPropertiesLocal
    {
        char extensionName[VkMaxNameSize];
        std::uint32_t specVersion;
    };

    typedef PFN_vkVoidFunction(VKAPI_PTR* PFN_GetPhysicalDeviceProcAddr)(VkInstance instance, const char* pName);
    typedef VkResult(VKAPI_PTR* PFN_vkEnumerateInstanceLayerProperties)(std::uint32_t*, VkLayerPropertiesLocal*);
    typedef VkResult(VKAPI_PTR* PFN_vkEnumerateInstanceExtensionProperties)(
        const char*,
        std::uint32_t*,
        VkExtensionPropertiesLocal*);
    typedef VkResult(VKAPI_PTR* PFN_vkEnumerateDeviceLayerProperties)(
        VkPhysicalDevice,
        std::uint32_t*,
        VkLayerPropertiesLocal*);
    typedef VkResult(VKAPI_PTR* PFN_vkEnumerateDeviceExtensionProperties)(
        VkPhysicalDevice,
        const char*,
        std::uint32_t*,
        VkExtensionPropertiesLocal*);

    enum VkLayerFunctionLocal
    {
        VkLayerFunctionLocal_LinkInfo = 0
    };

    enum VkNegotiateLayerStructTypeLocal
    {
        VkNegotiateLayerStructTypeLocal_Interface = 1
    };

    struct VkLayerInstanceLinkLocal
    {
        VkLayerInstanceLinkLocal* pNext;
        PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr;
        PFN_GetPhysicalDeviceProcAddr pfnNextGetPhysicalDeviceProcAddr;
    };

    struct VkLayerDeviceLinkLocal
    {
        VkLayerDeviceLinkLocal* pNext;
        PFN_vkGetInstanceProcAddr pfnNextGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr pfnNextGetDeviceProcAddr;
    };

    struct VkLayerInstanceCreateInfoLocal
    {
        VkStructureType sType;
        const void* pNext;
        VkLayerFunctionLocal function;
        union
        {
            VkLayerInstanceLinkLocal* pLayerInfo;
            void* loaderData;
        } u;
    };

    struct VkLayerDeviceCreateInfoLocal
    {
        VkStructureType sType;
        const void* pNext;
        VkLayerFunctionLocal function;
        union
        {
            VkLayerDeviceLinkLocal* pLayerInfo;
            void* loaderData;
        } u;
    };

    struct VkNegotiateLayerInterfaceLocal
    {
        VkNegotiateLayerStructTypeLocal sType;
        void* pNext;
        std::uint32_t loaderLayerInterfaceVersion;
        PFN_vkGetInstanceProcAddr pfnGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr pfnGetDeviceProcAddr;
        PFN_GetPhysicalDeviceProcAddr pfnGetPhysicalDeviceProcAddr;
    };

    struct InstanceDispatch
    {
        PFN_vkGetInstanceProcAddr gipa;
        PFN_vkDestroyInstance destroyInstance;
        PFN_vkEnumeratePhysicalDevices enumeratePhysicalDevices;
        PFN_vkCreateDevice createDevice;
        PFN_vkCreateWin32SurfaceKHR createWin32SurfaceKHR;
        PFN_vkDestroySurfaceKHR destroySurfaceKHR;
        PFN_vkEnumerateDeviceExtensionProperties enumerateDeviceExtensionProperties;
    };

    struct DeviceDispatch
    {
        VkInstance instance;
        PFN_vkGetDeviceProcAddr gdpa;
        PFN_vkDestroyDevice destroyDevice;
        PFN_vkGetDeviceQueue getDeviceQueue;
        PFN_vkGetDeviceQueue2 getDeviceQueue2;
        PFN_vkCreateSwapchainKHR createSwapchainKHR;
        PFN_vkDestroySwapchainKHR destroySwapchainKHR;
        PFN_vkGetSwapchainImagesKHR getSwapchainImagesKHR;
        PFN_vkAcquireNextImageKHR acquireNextImageKHR;
        PFN_vkAcquireNextImage2KHR acquireNextImage2KHR;
        PFN_vkQueuePresentKHR queuePresentKHR;
    };

    extern std::mutex g_layerMutex;
    extern std::unordered_map<std::uint64_t, InstanceDispatch> g_instances;
    extern std::unordered_map<std::uint64_t, VkInstance> g_physicalDeviceOwners;
    extern std::unordered_map<std::uint64_t, DeviceDispatch> g_devices;
    extern std::unordered_map<std::uint64_t, VkDevice> g_queues;
    extern LONG g_loggedQueuePresentCalls;

    std::uint64_t HandleKey(void* handle);
    std::uint32_t MakeVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t patch);

    template <typename T>
    inline T GetLoaderExport(const char* name)
    {
        HMODULE module = GetModuleHandleW(L"vulkan-1.dll");
        return module ? reinterpret_cast<T>(GetProcAddress(module, name)) : nullptr;
    }

    VkLayerInstanceCreateInfoLocal* GetLayerChainInfo(const VkInstanceCreateInfo* createInfo);
    VkLayerDeviceCreateInfoLocal* GetLayerChainInfo(const VkDeviceCreateInfo* createInfo);
    void FillLayerProperties(VkLayerPropertiesLocal& properties);
    bool FindInstanceDispatch(VkInstance instance, InstanceDispatch& dispatch);
    bool FindDeviceDispatch(VkDevice device, DeviceDispatch& dispatch);
    bool FindQueueDispatch(VkQueue queue, DeviceDispatch& dispatch);
    VkInstance ResolveOwningInstance(VkPhysicalDevice physicalDevice);

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
        std::uint32_t* propertyCount,
        VkLayerPropertiesLocal* properties);
    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
        const char* layerName,
        std::uint32_t* propertyCount,
        VkExtensionPropertiesLocal* properties);
    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceLayerProperties(
        VkPhysicalDevice physicalDevice,
        std::uint32_t* propertyCount,
        VkLayerPropertiesLocal* properties);
    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateDeviceExtensionProperties(
        VkPhysicalDevice physicalDevice,
        const char* layerName,
        std::uint32_t* propertyCount,
        VkExtensionPropertiesLocal* properties);

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateInstance(
        const VkInstanceCreateInfo* createInfo,
        const VkAllocationCallbacks* allocator,
        VkInstance* instance);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkEnumeratePhysicalDevices(
        VkInstance instance,
        std::uint32_t* physicalDeviceCount,
        VkPhysicalDevice* physicalDevices);
    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkCreateDevice(
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* createInfo,
        const VkAllocationCallbacks* allocator,
        VkDevice* device);

    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroyInstance(VkInstance instance, const VkAllocationCallbacks* allocator);
    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroyDevice(VkDevice device, const VkAllocationCallbacks* allocator);
    VKAPI_ATTR void VKAPI_CALL Layer_vkGetDeviceQueue(
        VkDevice device,
        std::uint32_t queueFamilyIndex,
        std::uint32_t queueIndex,
        VkQueue* queue);
    VKAPI_ATTR void VKAPI_CALL Layer_vkGetDeviceQueue2(
        VkDevice device,
        const VkDeviceQueueInfo2* queueInfo,
        VkQueue* queue);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkCreateWin32SurfaceKHR(
        VkInstance instance,
        const VkWin32SurfaceCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSurfaceKHR* surface);
    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroySurfaceKHR(
        VkInstance instance,
        VkSurfaceKHR surface,
        const VkAllocationCallbacks* allocator);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkCreateSwapchainKHR(
        VkDevice device,
        const VkSwapchainCreateInfoKHR* createInfo,
        const VkAllocationCallbacks* allocator,
        VkSwapchainKHR* swapchain);
    VKAPI_ATTR void VKAPI_CALL Layer_vkDestroySwapchainKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        const VkAllocationCallbacks* allocator);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkGetSwapchainImagesKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint32_t* swapchainImageCount,
        void* swapchainImages);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkAcquireNextImageKHR(
        VkDevice device,
        VkSwapchainKHR swapchain,
        std::uint64_t timeout,
        VkSemaphore semaphore,
        VkFence fence,
        std::uint32_t* imageIndex);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkAcquireNextImage2KHR(
        VkDevice device,
        const VkAcquireNextImageInfoKHR* acquireInfo,
        std::uint32_t* imageIndex);
    VKAPI_ATTR VkResult VKAPI_CALL Layer_vkQueuePresentKHR(VkQueue queue, const VkPresentInfoKHR* presentInfo);

    VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetInstanceProcAddr(VkInstance instance, const char* name);
    VK_LAYER_EXPORT VKAPI_ATTR PFN_vkVoidFunction VKAPI_CALL vkGetDeviceProcAddr(VkDevice device, const char* name);
    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL
    vkNegotiateLoaderLayerInterfaceVersion(VkNegotiateLayerInterfaceLocal* versionStruct);
}