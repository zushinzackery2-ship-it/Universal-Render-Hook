#include "urh_vulkan_layer_internal.h"

namespace UrhVulkanLayerInternal
{
    std::mutex g_layerMutex;
    std::unordered_map<std::uint64_t, InstanceDispatch> g_instances;
    std::unordered_map<std::uint64_t, VkInstance> g_physicalDeviceOwners;
    std::unordered_map<std::uint64_t, DeviceDispatch> g_devices;
    std::unordered_map<std::uint64_t, VkDevice> g_queues;
    LONG g_loggedQueuePresentCalls = 0;

    std::uint32_t MakeVersion(std::uint32_t major, std::uint32_t minor, std::uint32_t patch)
    {
        return (major << 22) | (minor << 12) | patch;
    }

    VkLayerInstanceCreateInfoLocal* GetLayerChainInfo(const VkInstanceCreateInfo* createInfo)
    {
        if (!createInfo)
        {
            return nullptr;
        }

        auto* chain = reinterpret_cast<VkLayerInstanceCreateInfoLocal*>(const_cast<void*>(createInfo->pNext));
        for (UINT index = 0; chain && index < 8; ++index)
        {
            if (chain->sType == VK_STRUCTURE_TYPE_LOADER_INSTANCE_CREATE_INFO &&
                chain->function == VkLayerFunctionLocal_LinkInfo &&
                chain->u.pLayerInfo)
            {
                return chain;
            }

            chain = reinterpret_cast<VkLayerInstanceCreateInfoLocal*>(const_cast<void*>(chain->pNext));
        }

        return nullptr;
    }

    VkLayerDeviceCreateInfoLocal* GetLayerChainInfo(const VkDeviceCreateInfo* createInfo)
    {
        if (!createInfo)
        {
            return nullptr;
        }

        auto* chain = reinterpret_cast<VkLayerDeviceCreateInfoLocal*>(const_cast<void*>(createInfo->pNext));
        for (UINT index = 0; chain && index < 8; ++index)
        {
            if (chain->sType == VK_STRUCTURE_TYPE_LOADER_DEVICE_CREATE_INFO &&
                chain->function == VkLayerFunctionLocal_LinkInfo &&
                chain->u.pLayerInfo)
            {
                return chain;
            }

            chain = reinterpret_cast<VkLayerDeviceCreateInfoLocal*>(const_cast<void*>(chain->pNext));
        }

        return nullptr;
    }

    void FillLayerProperties(VkLayerPropertiesLocal& properties)
    {
        ZeroMemory(&properties, sizeof(properties));
        strcpy_s(properties.layerName, LayerName);
        strcpy_s(properties.description, LayerDescription);
        properties.specVersion = MakeVersion(1, 3, 0);
        properties.implementationVersion = 1;
    }

    bool FindInstanceDispatch(VkInstance instance, InstanceDispatch& dispatch)
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        const auto it = g_instances.find(UrhVulkanCommon::HandleKey(instance));
        if (it == g_instances.end())
        {
            return false;
        }

        dispatch = it->second;
        return true;
    }

    bool FindDeviceDispatch(VkDevice device, DeviceDispatch& dispatch)
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        const auto it = g_devices.find(UrhVulkanCommon::HandleKey(device));
        if (it == g_devices.end())
        {
            return false;
        }

        dispatch = it->second;
        return true;
    }

    bool FindQueueDispatch(VkQueue queue, DeviceDispatch& dispatch)
    {
        std::lock_guard<std::mutex> lock(g_layerMutex);
        const auto queueIt = g_queues.find(UrhVulkanCommon::HandleKey(queue));
        if (queueIt == g_queues.end())
        {
            return false;
        }

        const auto deviceIt = g_devices.find(UrhVulkanCommon::HandleKey(queueIt->second));
        if (deviceIt == g_devices.end())
        {
            return false;
        }

        dispatch = deviceIt->second;
        return true;
    }

    VkInstance ResolveOwningInstance(VkPhysicalDevice physicalDevice)
    {
        {
            std::lock_guard<std::mutex> lock(g_layerMutex);
            const auto it = g_physicalDeviceOwners.find(UrhVulkanCommon::HandleKey(physicalDevice));
            if (it != g_physicalDeviceOwners.end())
            {
                return it->second;
            }
        }

        std::lock_guard<std::mutex> lock(g_layerMutex);
        for (const auto& entry : g_instances)
        {
            const VkInstance instance = reinterpret_cast<VkInstance>(static_cast<UINT_PTR>(entry.first));
            const InstanceDispatch& dispatch = entry.second;
            if (!dispatch.enumeratePhysicalDevices)
            {
                continue;
            }

            std::uint32_t physicalDeviceCount = 0;
            if (dispatch.enumeratePhysicalDevices(instance, &physicalDeviceCount, nullptr) != VK_SUCCESS ||
                physicalDeviceCount == 0)
            {
                continue;
            }

            std::vector<VkPhysicalDevice> physicalDevices(physicalDeviceCount);
            if (dispatch.enumeratePhysicalDevices(instance, &physicalDeviceCount, physicalDevices.data()) != VK_SUCCESS)
            {
                continue;
            }

            for (std::uint32_t index = 0; index < physicalDeviceCount; ++index)
            {
                if (physicalDevices[index] == physicalDevice)
                {
                    g_physicalDeviceOwners[UrhVulkanCommon::HandleKey(physicalDevice)] = instance;
                    return instance;
                }
            }
        }

        return nullptr;
    }

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceLayerProperties(
        std::uint32_t* propertyCount,
        VkLayerPropertiesLocal* properties)
    {
        if (!propertyCount)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        if (!properties)
        {
            *propertyCount = 1;
            return VK_SUCCESS;
        }

        if (*propertyCount == 0)
        {
            return VK_ERROR_INITIALIZATION_FAILED;
        }

        FillLayerProperties(properties[0]);
        *propertyCount = 1;
        return VK_SUCCESS;
    }

    VK_LAYER_EXPORT VKAPI_ATTR VkResult VKAPI_CALL vkEnumerateInstanceExtensionProperties(
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

        auto fn = GetLoaderExport<PFN_vkEnumerateInstanceExtensionProperties>("vkEnumerateInstanceExtensionProperties");
        return fn ? fn(layerName, propertyCount, properties) : VK_ERROR_INITIALIZATION_FAILED;
    }
}
