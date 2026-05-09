#include "urh_vulkan_internal.h"

namespace
{
    LONG g_runtimeRefreshLogCount = 0;

    struct WindowSearchState
    {
        DWORD processId;
        HWND bestWindow;
        float bestArea;
    };

    BOOL CALLBACK EnumProcessWindowsProc(HWND hwnd, LPARAM lParam)
    {
        auto* state = reinterpret_cast<WindowSearchState*>(lParam);
        if (!state || !UrhVulkanHookInternal::IsCandidateWindow(hwnd))
        {
            return TRUE;
        }

        DWORD windowProcessId = 0;
        GetWindowThreadProcessId(hwnd, &windowProcessId);
        if (windowProcessId != state->processId)
        {
            return TRUE;
        }

        float width = 0.0f;
        float height = 0.0f;
        const float area = UrhVulkanHookInternal::UpdateWindowMetrics(hwnd, width, height);
        if (area <= state->bestArea)
        {
            return TRUE;
        }

        state->bestArea = area;
        state->bestWindow = hwnd;
        return TRUE;
    }
}

namespace UrhVulkanHookInternal
{
    bool IsRuntimeReadyForCapture(const UrhVulkanHookRuntime& runtime)
    {
        return runtime.device != nullptr &&
            runtime.queue != nullptr &&
            runtime.swapchain != nullptr &&
            runtime.queueFamilyIndex != VK_QUEUE_FAMILY_IGNORED &&
            runtime.swapchainFormat != 0 &&
            runtime.width > 0.0f &&
            runtime.height > 0.0f;
    }

    bool IsCandidateWindow(HWND hwnd)
    {
        if (!hwnd || !IsWindow(hwnd) || !IsWindowVisible(hwnd) || IsIconic(hwnd))
        {
            return false;
        }

        if (GetWindow(hwnd, GW_OWNER))
        {
            return false;
        }

        const LONG style = GetWindowLongW(hwnd, GWL_STYLE);
        return (style & WS_CHILD) == 0;
    }

    float UpdateWindowMetrics(HWND hwnd, float& width, float& height)
    {
        RECT clientRect = {};
        if (!hwnd || !GetClientRect(hwnd, &clientRect))
        {
            width = 0.0f;
            height = 0.0f;
            return 0.0f;
        }

        width = static_cast<float>(clientRect.right - clientRect.left);
        height = static_cast<float>(clientRect.bottom - clientRect.top);
        return width * height;
    }

    HWND FindProcessRenderWindow()
    {
        const DWORD processId = GetCurrentProcessId();
        HWND foregroundWindow = GetForegroundWindow();
        if (foregroundWindow && IsCandidateWindow(foregroundWindow))
        {
            DWORD foregroundProcessId = 0;
            GetWindowThreadProcessId(foregroundWindow, &foregroundProcessId);
            if (foregroundProcessId == processId)
            {
                return foregroundWindow;
            }
        }

        WindowSearchState state = {};
        state.processId = processId;
        EnumWindows(EnumProcessWindowsProc, reinterpret_cast<LPARAM>(&state));
        return state.bestWindow;
    }

    void RefreshRuntimeFromTrackedSwapchainLocked(VkSwapchainKHR swapchain)
    {
        const auto swapchainIt = g_state.swapchains.find(swapchain);
        if (swapchainIt == g_state.swapchains.end())
        {
            return;
        }

        const SwapchainInfo& swapchainInfo = swapchainIt->second;
        if (!swapchainInfo.device ||
            !swapchainInfo.lastQueue ||
            !swapchainInfo.hwnd ||
            swapchainInfo.width <= 0.0f ||
            swapchainInfo.height <= 0.0f)
        {
            return;
        }

        VkInstance instance = nullptr;
        const auto surfaceIt = g_state.surfaces.find(swapchainInfo.surface);
        if (surfaceIt != g_state.surfaces.end())
        {
            instance = surfaceIt->second.instance;
        }

        ResetRuntime(g_state.runtime);
        g_state.runtime.hwnd = swapchainInfo.hwnd;
        g_state.runtime.instance = instance;
        g_state.runtime.physicalDevice = swapchainInfo.physicalDevice;
        g_state.runtime.device = swapchainInfo.device;
        g_state.runtime.queue = swapchainInfo.lastQueue;
        g_state.runtime.swapchain = reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain));
        g_state.runtime.queueFamilyIndex = swapchainInfo.queueFamilyIndex;
        g_state.runtime.swapchainFormat = swapchainInfo.format;
        const auto deviceIt = g_state.devices.find(UrhVulkanCommon::HandleKey(swapchainInfo.device));
        if (deviceIt != g_state.devices.end())
        {
            g_state.runtime.memoryTypeCount = deviceIt->second.memoryTypeCount;
            for (UINT index = 0; index < deviceIt->second.memoryTypeCount && index < 32u; ++index)
            {
                g_state.runtime.memoryTypeFlags[index] = deviceIt->second.memoryTypeFlags[index];
            }
        }
        g_state.runtime.imageCount = swapchainInfo.imageCount;
        g_state.runtime.imageIndex = swapchainInfo.lastImageIndex;
        g_state.runtime.width = swapchainInfo.width;
        g_state.runtime.height = swapchainInfo.height;
        g_state.runtime.frameCount = swapchainInfo.frameCount;
        g_state.ready = IsRuntimeReadyForCapture(g_state.runtime);

        if (InterlockedIncrement(&g_runtimeRefreshLogCount) <= 24)
        {
            URH_VULKANHOOK_LOG(
                "Runtime snapshot refreshed: swapchain=%p device=%p queue=%p family=%u format=%u ready=%d",
                reinterpret_cast<void*>(static_cast<UINT_PTR>(swapchain)),
                g_state.runtime.device,
                g_state.runtime.queue,
                g_state.runtime.queueFamilyIndex,
                g_state.runtime.swapchainFormat,
                g_state.ready ? 1 : 0);
        }
    }
}
