#pragma once

#include <Windows.h>

struct UrhVulkanHookRuntime;

using UrhVulkanHookSetupCallback = void (*)(
    const UrhVulkanHookRuntime* runtime,
    void* userData);

using UrhVulkanHookRenderCallback = void (*)(
    const UrhVulkanHookRuntime* runtime,
    void* userData);

using UrhVulkanHookVisibleCallback = bool (*)(
    void* userData);

using UrhVulkanHookShutdownCallback = void (*)(
    void* userData);

struct UrhVulkanHookRuntime
{
    void* hwnd;
    void* instance;
    void* physicalDevice;
    void* device;
    void* queue;
    void* swapchain;
    UINT queueFamilyIndex;
    UINT swapchainFormat;
    UINT memoryTypeCount;
    UINT memoryTypeFlags[32];
    UINT imageCount;
    UINT imageIndex;
    float width;
    float height;
    UINT frameCount;
};

struct UrhVulkanHookDesc
{
    UrhVulkanHookSetupCallback onSetup;
    UrhVulkanHookRenderCallback onRender;
    UrhVulkanHookVisibleCallback isVisible;
    UrhVulkanHookShutdownCallback onShutdown;
    void* userData;

    bool autoCreateContext;
    bool hookWndProc;
    bool blockInputWhenVisible;
    bool enableDefaultDebugWindow;

    UINT warmupFrames;
    UINT shutdownWaitTimeoutMs;
    UINT toggleVirtualKey;
    bool startVisible;
};
