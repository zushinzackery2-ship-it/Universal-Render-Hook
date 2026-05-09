#include "urh_dx12_internal.h"
#include "urh_dx_common.h"

namespace UrhDx12HookInternal
{
    bool InstallHooks()
    {
        if (!ProbeVtables(g_state.probe))
        {
            return false;
        }

        auto rollback = []()
        {
            UrhDxCommon::RestoreVtable(g_state.probe.swapChainVtable, 8, reinterpret_cast<void*>(g_state.originalPresent));
            UrhDxCommon::RestoreVtable(g_state.probe.swapChainVtable, 22, reinterpret_cast<void*>(g_state.originalPresent1));
            UrhDxCommon::RestoreVtable(
                g_state.probe.swapChainVtable,
                13,
                reinterpret_cast<void*>(g_state.originalResizeBuffers));
            UrhDxCommon::RestoreVtable(
                g_state.probe.commandQueueVtable,
                10,
                reinterpret_cast<void*>(g_state.originalExecuteCommandLists));
            g_state.originalPresent = nullptr;
            g_state.originalPresent1 = nullptr;
            g_state.originalResizeBuffers = nullptr;
            g_state.originalExecuteCommandLists = nullptr;
        };

        if (!UrhDxCommon::PatchVtable(
                g_state.probe.swapChainVtable,
                8,
                reinterpret_cast<void*>(&HookPresent),
                reinterpret_cast<void**>(&g_state.originalPresent)))
        {
            return false;
        }

        UrhDxCommon::PatchVtable(
            g_state.probe.swapChainVtable,
            22,
            reinterpret_cast<void*>(&HookPresent1),
            reinterpret_cast<void**>(&g_state.originalPresent1));

        if (!UrhDxCommon::PatchVtable(
                g_state.probe.swapChainVtable,
                13,
                reinterpret_cast<void*>(&HookResizeBuffers),
                reinterpret_cast<void**>(&g_state.originalResizeBuffers)))
        {
            rollback();
            return false;
        }

        if (!UrhDxCommon::PatchVtable(
                g_state.probe.commandQueueVtable,
                10,
                reinterpret_cast<void*>(&HookExecuteCommandLists),
                reinterpret_cast<void**>(&g_state.originalExecuteCommandLists)))
        {
            rollback();
            return false;
        }

        return true;
    }

    void UninstallHooks()
    {
        UrhDxCommon::RestoreVtable(g_state.probe.swapChainVtable, 8, reinterpret_cast<void*>(g_state.originalPresent));
        UrhDxCommon::RestoreVtable(g_state.probe.swapChainVtable, 22, reinterpret_cast<void*>(g_state.originalPresent1));
        UrhDxCommon::RestoreVtable(
            g_state.probe.swapChainVtable,
            13,
            reinterpret_cast<void*>(g_state.originalResizeBuffers));
        UrhDxCommon::RestoreVtable(
            g_state.probe.commandQueueVtable,
            10,
            reinterpret_cast<void*>(g_state.originalExecuteCommandLists));

        if (g_state.trackedSwapChainVtable && g_state.originalResizeBuffers1)
        {
            UrhDxCommon::RestoreVtable(
                g_state.trackedSwapChainVtable,
                39,
                reinterpret_cast<void*>(g_state.originalResizeBuffers1));
        }

        g_state.originalPresent = nullptr;
        g_state.originalPresent1 = nullptr;
        g_state.originalResizeBuffers = nullptr;
        g_state.originalResizeBuffers1 = nullptr;
        g_state.originalExecuteCommandLists = nullptr;
        ZeroMemory(&g_state.probe, sizeof(g_state.probe));
    }
}
