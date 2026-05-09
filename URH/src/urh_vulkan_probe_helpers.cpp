#include "urh_vulkan_internal.h"

namespace UrhVulkanHookInternal
{
    bool SafeCreateInstance(
        PFN_vkCreateInstance createInstance,
        const VkInstanceCreateInfo* createInfo,
        VkInstance* instance,
        VkResult& result,
        DWORD& exceptionCode)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
        exceptionCode = 0;
        __try
        {
            result = createInstance(createInfo, nullptr, instance);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = GetExceptionCode();
        }

        return false;
    }

    bool SafeCreateDevice(
        PFN_vkCreateDevice createDevice,
        VkPhysicalDevice physicalDevice,
        const VkDeviceCreateInfo* createInfo,
        VkDevice* device,
        VkResult& result,
        DWORD& exceptionCode)
    {
        result = VK_ERROR_INITIALIZATION_FAILED;
        exceptionCode = 0;
        __try
        {
            result = createDevice(physicalDevice, createInfo, nullptr, device);
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = GetExceptionCode();
        }

        return false;
    }

    bool SafeGetDeviceCommand(
        PFN_vkGetDeviceProcAddr getDeviceProcAddr,
        VkDevice device,
        const char* procName,
        void*& address,
        DWORD& exceptionCode)
    {
        address = nullptr;
        exceptionCode = 0;
        __try
        {
            address = reinterpret_cast<void*>(getDeviceProcAddr(device, procName));
            return true;
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            exceptionCode = GetExceptionCode();
        }

        return false;
    }

    bool FindProbeQueueFamilyIndex(
        VkPhysicalDevice physicalDevice,
        PFN_vkGetPhysicalDeviceQueueFamilyProperties getQueueFamilyProperties,
        std::uint32_t& queueFamilyIndex)
    {
        if (!physicalDevice || !getQueueFamilyProperties)
        {
            return false;
        }

        std::uint32_t queueFamilyCount = 0;
        getQueueFamilyProperties(physicalDevice, &queueFamilyCount, nullptr);
        if (queueFamilyCount == 0)
        {
            return false;
        }

        std::vector<VkQueueFamilyProperties> queueFamilies(queueFamilyCount);
        getQueueFamilyProperties(physicalDevice, &queueFamilyCount, queueFamilies.data());
        for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
        {
            if ((queueFamilies[index].queueFlags & VK_QUEUE_GRAPHICS_BIT) != 0 &&
                queueFamilies[index].queueCount > 0)
            {
                queueFamilyIndex = index;
                return true;
            }
        }

        for (std::uint32_t index = 0; index < queueFamilyCount; ++index)
        {
            if (queueFamilies[index].queueCount > 0)
            {
                queueFamilyIndex = index;
                return true;
            }
        }

        return false;
    }

    bool ProbeRuntimeTargetsOnce()
    {
        HMODULE vulkanModule = nullptr;
        GetProcAddressFn realGetProcAddress = nullptr;
        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (!g_state.installed || g_state.layerModeEnabled)
            {
                return false;
            }

            ResolveLoaderFunctionsLocked();
            vulkanModule = g_state.vulkanModule;
            realGetProcAddress = g_state.realGetProcAddress;
        }

        if (!vulkanModule || !realGetProcAddress)
        {
            URH_VULKANHOOK_LOG("Runtime probe skipped: vulkan loader unresolved");
            return false;
        }

        auto createInstance = reinterpret_cast<PFN_vkCreateInstance>(
            realGetProcAddress(vulkanModule, "vkCreateInstance"));
        auto getInstanceProcAddr = reinterpret_cast<PFN_vkGetInstanceProcAddr>(
            realGetProcAddress(vulkanModule, "vkGetInstanceProcAddr"));
        if (!createInstance || !getInstanceProcAddr)
        {
            URH_VULKANHOOK_LOG("Runtime probe skipped: vkCreateInstance/vkGetInstanceProcAddr unresolved");
            return false;
        }

        URH_VULKANHOOK_LOG("Runtime probe begin");

        VkApplicationInfo applicationInfo = {};
        applicationInfo.sType = VK_STRUCTURE_TYPE_APPLICATION_INFO;
        applicationInfo.pApplicationName = "VkhRuntimeProbe";
        applicationInfo.pEngineName = "VkhRuntimeProbe";

        VkInstanceCreateInfo instanceCreateInfo = {};
        instanceCreateInfo.sType = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
        instanceCreateInfo.pApplicationInfo = &applicationInfo;

        VkInstance instance = nullptr;
        VkDevice device = nullptr;
        PFN_vkDestroyInstance destroyInstance = nullptr;
        PFN_vkDestroyDevice destroyDevice = nullptr;
        bool resolvedAnyTarget = false;

        do
        {
            DWORD createInstanceException = 0;
            VkResult instanceResult = VK_ERROR_INITIALIZATION_FAILED;
            if (!SafeCreateInstance(
                    createInstance,
                    &instanceCreateInfo,
                    &instance,
                    instanceResult,
                    createInstanceException))
            {
                URH_VULKANHOOK_LOG(
                    "Runtime probe exception: vkCreateInstance code=0x%08X",
                    createInstanceException);
                break;
            }

            if (instanceResult != VK_SUCCESS || !instance)
            {
                URH_VULKANHOOK_LOG(
                    "Runtime probe failed: vkCreateInstance result=%d instance=%p",
                    instanceResult,
                    instance);
                break;
            }

            destroyInstance = reinterpret_cast<PFN_vkDestroyInstance>(
                getInstanceProcAddr(instance, "vkDestroyInstance"));
            auto enumeratePhysicalDevices =
                reinterpret_cast<PFN_vkEnumeratePhysicalDevices>(
                    getInstanceProcAddr(instance, "vkEnumeratePhysicalDevices"));
            auto getQueueFamilyProperties =
                reinterpret_cast<PFN_vkGetPhysicalDeviceQueueFamilyProperties>(
                    getInstanceProcAddr(instance, "vkGetPhysicalDeviceQueueFamilyProperties"));
            auto createDevice =
                reinterpret_cast<PFN_vkCreateDevice>(
                    getInstanceProcAddr(instance, "vkCreateDevice"));
            if (!enumeratePhysicalDevices || !getQueueFamilyProperties || !createDevice)
            {
                URH_VULKANHOOK_LOG("Runtime probe failed: instance-level commands unresolved");
                break;
            }

            std::uint32_t physicalDeviceCount = 0;
            if (enumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr) != VK_SUCCESS ||
                physicalDeviceCount == 0)
            {
                URH_VULKANHOOK_LOG(
                    "Runtime probe failed: enumeratePhysicalDevices count=%u",
                    physicalDeviceCount);
                break;
            }

            std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
            if (enumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()) != VK_SUCCESS ||
                physicalDevices.empty())
            {
                URH_VULKANHOOK_LOG("Runtime probe failed: enumeratePhysicalDevices(list)");
                break;
            }

            std::uint32_t queueFamilyIndex = 0;
            if (!FindProbeQueueFamilyIndex(
                    physicalDevices[0],
                    getQueueFamilyProperties,
                    queueFamilyIndex))
            {
                URH_VULKANHOOK_LOG("Runtime probe failed: no usable queue family");
                break;
            }

            const float queuePriority = 1.0f;
            VkDeviceQueueCreateInfo queueCreateInfo = {};
            queueCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
            queueCreateInfo.queueFamilyIndex = queueFamilyIndex;
            queueCreateInfo.queueCount = 1;
            queueCreateInfo.pQueuePriorities = &queuePriority;

            const char* deviceExtensions[] =
            {
                "VK_KHR_swapchain"
            };

            VkDeviceCreateInfo deviceCreateInfo = {};
            deviceCreateInfo.sType = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            deviceCreateInfo.queueCreateInfoCount = 1;
            deviceCreateInfo.pQueueCreateInfos = &queueCreateInfo;
            deviceCreateInfo.enabledExtensionCount = 1;
            deviceCreateInfo.ppEnabledExtensionNames = deviceExtensions;

            DWORD createDeviceException = 0;
            VkResult deviceResult = VK_ERROR_INITIALIZATION_FAILED;
            if (!SafeCreateDevice(
                    createDevice,
                    physicalDevices[0],
                    &deviceCreateInfo,
                    &device,
                    deviceResult,
                    createDeviceException))
            {
                URH_VULKANHOOK_LOG(
                    "Runtime probe exception: vkCreateDevice code=0x%08X",
                    createDeviceException);
                break;
            }

            if (deviceResult != VK_SUCCESS || !device)
            {
                URH_VULKANHOOK_LOG(
                    "Runtime probe failed: vkCreateDevice result=%d device=%p",
                    deviceResult,
                    device);
                break;
            }

            auto getDeviceProcAddr = reinterpret_cast<PFN_vkGetDeviceProcAddr>(
                getInstanceProcAddr(instance, "vkGetDeviceProcAddr"));
            destroyDevice = getDeviceProcAddr
                ? reinterpret_cast<PFN_vkDestroyDevice>(getDeviceProcAddr(device, "vkDestroyDevice"))
                : nullptr;
            if (!getDeviceProcAddr)
            {
                URH_VULKANHOOK_LOG("Runtime probe failed: vkGetDeviceProcAddr unresolved");
                break;
            }

            const char* targetSpecs[] =
            {
                "vkGetDeviceQueue",
                "vkGetDeviceQueue2",
                "vkQueuePresentKHR",
                "vkGetSwapchainImagesKHR",
                "vkCreateSwapchainKHR",
                "vkAcquireNextImageKHR",
                "vkAcquireNextImage2KHR"
            };

            for (const char* spec : targetSpecs)
            {
                void* address = nullptr;
                DWORD getProcException = 0;
                if (!SafeGetDeviceCommand(
                        getDeviceProcAddr,
                        device,
                        spec,
                        address,
                        getProcException))
                {
                    URH_VULKANHOOK_LOG(
                        "Runtime probe exception: getDeviceProcAddr(%s) code=0x%08X",
                        spec,
                        getProcException);
                    continue;
                }

                if (!address)
                {
                    address = reinterpret_cast<void*>(getInstanceProcAddr(instance, spec));
                }

                URH_VULKANHOOK_LOG(
                    "Runtime probe result: %s=%p",
                    spec,
                    address);
                if (address)
                {
                    resolvedAnyTarget = true;
                }
            }
        }
        while (false);

        if (device && destroyDevice)
        {
            destroyDevice(device, nullptr);
        }

        if (instance && destroyInstance)
        {
            destroyInstance(instance, nullptr);
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            if (resolvedAnyTarget)
            {
                g_state.runtimeProbeCompleted = true;
            }
        }

        URH_VULKANHOOK_LOG(
            "Runtime probe end: armedAnyTarget=%d",
            resolvedAnyTarget ? 1 : 0);
        return resolvedAnyTarget;
    }
}
