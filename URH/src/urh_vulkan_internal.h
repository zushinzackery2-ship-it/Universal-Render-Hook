#pragma once

#include <Windows.h>
#include <Shlwapi.h>

#include <atomic>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

#include "../include/urh/vulkan_hook.h"
#include "urh_console_logger.h"
#include "urh_vulkan_minimal.h"
#define URH_VULKAN_LOG(...) ConsoleLogger::Log(__VA_ARGS__)

namespace UrhVulkanHookInternal
{
    typedef FARPROC(WINAPI* GetProcAddressFn)(HMODULE moduleHandle, LPCSTR procName);

    struct SurfaceInfo
    {
        VkInstance instance;
        HWND hwnd;
    };

    struct SwapchainInfo
    {
        VkDevice device;
        VkPhysicalDevice physicalDevice;
        VkSurfaceKHR surface;
        VkQueue lastQueue;
        UINT queueFamilyIndex;
        VkFormat format;
        HWND hwnd;
        UINT imageCount;
        UINT lastImageIndex;
        UINT frameCount;
        float width;
        float height;
        bool lateAttached;
    };

    struct QueueInfo
    {
        VkDevice device;
        UINT queueFamilyIndex;
    };

    struct DeviceInfo
    {
        VkPhysicalDevice physicalDevice;
        UINT memoryTypeCount;
        VkMemoryPropertyFlags memoryTypeFlags[32];
    };

    struct ModuleState
    {
        UrhVulkanHookDesc desc;
        UrhVulkanHookRuntime runtime;
        std::mutex mutex;
        std::unordered_map<std::uint64_t, SurfaceInfo> surfaces;
        std::unordered_map<std::uint64_t, SwapchainInfo> swapchains;
        std::unordered_map<std::uint64_t, DeviceInfo> devices;
        std::unordered_map<std::uint64_t, QueueInfo> queues;
        HMODULE vulkanModule;
        GetProcAddressFn realGetProcAddress;
        PFN_vkGetInstanceProcAddr realVkGetInstanceProcAddr;
        PFN_vkGetDeviceProcAddr realVkGetDeviceProcAddr;
        std::condition_variable runtimeProbeCondition;
        std::thread runtimeProbeThread;
        std::atomic<DWORD> runtimeProbeThreadId;
        bool runtimeProbeRequested;
        bool runtimeProbeStopRequested;
        bool runtimeProbeCompleted;
        bool layerModeEnabled;
        bool backendRecognized;
        bool installed;
        bool ready;
        bool setupCalled;
    };

    extern ModuleState g_state;

    inline std::uint64_t HandleKey(void* handle)
    {
        return static_cast<std::uint64_t>(reinterpret_cast<UINT_PTR>(handle));
    }

    void FillDefaultDesc(UrhVulkanHookDesc& desc);
    void ResetRuntime(UrhVulkanHookRuntime& runtime);
    void ResetTransientStateLocked();
    void MarkBackendRecognizedLocked(const char* reason);
    void EnsureRuntimeProbeThreadLocked();
    void RequestRuntimeProbeLocked(const char* reason);
    void StopRuntimeProbeThreadLocked(std::thread& threadToJoin);
    void ResolveLoaderFunctionsLocked();
    HMODULE FindVulkanModuleLocked();
    void MarkLayerModeActiveLocked();
    void DispatchPresent(VkQueue queue, VkSwapchainKHR swapchain, UINT imageIndex);
    void TrackSurfaceCreated(VkInstance instance, VkSurfaceKHR surface, HWND hwnd);
    void TrackSurfaceDestroyed(VkSurfaceKHR surface);
    void TrackSwapchainCreated(
        VkDevice device,
        VkFormat format,
        VkSwapchainKHR swapchain,
        VkSurfaceKHR surface,
        UINT imageCount,
        float width,
        float height);
    void TrackSwapchainDestroyed(VkSwapchainKHR swapchain);
    void TrackSwapchainImageCount(VkSwapchainKHR swapchain, UINT imageCount);
    void TrackAcquireImage(VkSwapchainKHR swapchain, UINT imageIndex, VkDevice device = nullptr);
    void TrackDeviceCreated(
        VkDevice device,
        VkPhysicalDevice physicalDevice,
        PFN_vkGetPhysicalDeviceMemoryProperties getPhysicalDeviceMemoryProperties = nullptr);
    void TrackQueueResolved(VkQueue queue, VkDevice device, UINT queueFamilyIndex);
    void TrackLateSwapchainDevice(VkDevice device, VkSwapchainKHR swapchain);
    bool TryBootstrapLateSwapchainLocked(VkQueue queue, VkSwapchainKHR swapchain);

    template <typename T>
    inline T ResolveLoaderExport(const char* procName)
    {
        std::lock_guard<std::mutex> lock(g_state.mutex);
        if (!g_state.vulkanModule)
        {
            g_state.vulkanModule = FindVulkanModuleLocked();
        }

        if (!g_state.realGetProcAddress || !g_state.vulkanModule)
        {
            return nullptr;
        }

        return reinterpret_cast<T>(g_state.realGetProcAddress(g_state.vulkanModule, procName));
    }

    template <typename T>
    inline T ResolveInstanceCommand(VkInstance instance, const char* procName)
    {
        PFN_vkGetInstanceProcAddr realVkGetInstanceProcAddr = nullptr;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.realVkGetInstanceProcAddr && g_state.vulkanModule && g_state.realGetProcAddress)
            {
                g_state.realVkGetInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
                    g_state.realGetProcAddress(g_state.vulkanModule, "vkGetInstanceProcAddr"));
            }

            realVkGetInstanceProcAddr = g_state.realVkGetInstanceProcAddr;
        }

        if (realVkGetInstanceProcAddr)
        {
            return reinterpret_cast<T>(realVkGetInstanceProcAddr(instance, procName));
        }

        return ResolveLoaderExport<T>(procName);
    }

    template <typename T>
    inline T ResolveDeviceCommand(VkDevice device, const char* procName)
    {
        PFN_vkGetDeviceProcAddr realVkGetDeviceProcAddr = nullptr;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.realVkGetDeviceProcAddr && g_state.vulkanModule && g_state.realGetProcAddress)
            {
                g_state.realVkGetDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                    g_state.realGetProcAddress(g_state.vulkanModule, "vkGetDeviceProcAddr"));
            }

            realVkGetDeviceProcAddr = g_state.realVkGetDeviceProcAddr;
        }

        if (realVkGetDeviceProcAddr)
        {
            return reinterpret_cast<T>(realVkGetDeviceProcAddr(device, procName));
        }

        return ResolveLoaderExport<T>(procName);
    }

    inline bool IsSuccessfulResult(VkResult result)
    {
        return result >= 0;
    }
}
