#pragma once

#include "types.h"
#include "autohook.h"

namespace URH
{
    using Backend = UrhAutoHookBackend;
    static constexpr Backend BackendUnknown = UrhAutoHookBackend_Unknown;
    static constexpr Backend BackendDx11 = UrhAutoHookBackend_Dx11;
    static constexpr Backend BackendDx12 = UrhAutoHookBackend_Dx12;
    static constexpr Backend BackendVulkan = UrhAutoHookBackend_Vulkan;

    using Runtime = UrhAutoHookRuntime;
    using Diagnostics = UrhAutoHookDiagnostics;
    using Desc = UrhAutoHookDesc;
    using SetupCallback = UrhAutoHookSetupCallback;
    using RenderCallback = UrhAutoHookRenderCallback;
    using VisibleCallback = UrhAutoHookVisibleCallback;
    using ShutdownCallback = UrhAutoHookShutdownCallback;

    inline void FillDefaultDesc(Desc* desc)
    {
        UrhAutoHook::FillDefaultDesc(desc);
    }

    inline bool Init(const Desc* desc)
    {
        return UrhAutoHook::Init(desc);
    }

    inline void Shutdown()
    {
        UrhAutoHook::Shutdown();
    }

    inline bool IsInstalled()
    {
        return UrhAutoHook::IsInstalled();
    }

    inline bool IsReady()
    {
        return UrhAutoHook::IsReady();
    }

    inline const Runtime* GetRuntime()
    {
        return UrhAutoHook::GetRuntime();
    }

    inline void GetDiagnostics(Diagnostics* diagnostics)
    {
        UrhAutoHook::GetDiagnostics(diagnostics);
    }
}
