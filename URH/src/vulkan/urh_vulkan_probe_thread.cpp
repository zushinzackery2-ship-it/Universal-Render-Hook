#include "urh_vulkan_internal.h"

namespace
{
    void RuntimeProbeThreadMain()
    {
        using namespace UrhVulkanHookInternal;

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.runtimeProbeThreadId.store(GetCurrentThreadId());
        }

        URH_VULKANHOOK_LOG(
            "Runtime probe thread started: tid=%lu",
            static_cast<unsigned long>(GetCurrentThreadId()));

        for (;;)
        {
            {
                std::unique_lock<std::mutex> lock(g_state.mutex);
                g_state.runtimeProbeCondition.wait(
                    lock,
                    []
                    {
                        return g_state.runtimeProbeStopRequested ||
                            g_state.runtimeProbeRequested;
                    });

                if (g_state.runtimeProbeStopRequested)
                {
                    break;
                }

                g_state.runtimeProbeRequested = false;
            }

            ProbeRuntimeTargetsOnce();
        }

        {
            std::lock_guard<std::mutex> lock(g_state.mutex);
            g_state.runtimeProbeThreadId.store(0);
        }

        URH_VULKANHOOK_LOG("Runtime probe thread stopped");
    }
}

namespace UrhVulkanHookInternal
{
    void EnsureRuntimeProbeThreadLocked()
    {
        if (g_state.layerModeEnabled || g_state.runtimeProbeThread.joinable())
        {
            return;
        }

        g_state.runtimeProbeStopRequested = false;
        g_state.runtimeProbeRequested = false;
        g_state.runtimeProbeThread = std::thread(RuntimeProbeThreadMain);
    }

    void RequestRuntimeProbeLocked(const char* reason)
    {
        if (!g_state.installed || g_state.layerModeEnabled || g_state.runtimeProbeCompleted)
        {
            return;
        }

        EnsureRuntimeProbeThreadLocked();
        g_state.runtimeProbeRequested = true;
        g_state.runtimeProbeCondition.notify_one();
        URH_VULKANHOOK_LOG(
            "Runtime probe requested: reason=%s",
            reason ? reason : "<unknown>");
    }

    void StopRuntimeProbeThreadLocked(std::thread& threadToJoin)
    {
        if (!g_state.runtimeProbeThread.joinable())
        {
            return;
        }

        g_state.runtimeProbeStopRequested = true;
        g_state.runtimeProbeRequested = false;
        g_state.runtimeProbeCondition.notify_all();
        threadToJoin = std::move(g_state.runtimeProbeThread);
    }
}
