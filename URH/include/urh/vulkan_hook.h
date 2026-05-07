#pragma once

#include "vulkan_types.h"

namespace UrhVulkanHook
{
    void FillDefaultDesc(UrhVulkanHookDesc* desc);
    bool Init(const UrhVulkanHookDesc* desc);
    void Shutdown();
    bool IsInstalled();
    bool IsLayerModeEnabled();
    bool HasTrackedActivity();
    bool HasRecognizedBackend();
    bool IsReady();
    const UrhVulkanHookRuntime* GetRuntime();
}
