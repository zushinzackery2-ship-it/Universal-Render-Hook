#pragma once

#include "dx12_types.h"

namespace UrhDx12Hook
{
    void FillDefaultDesc(UrhDx12HookDesc* desc);
    bool Init(const UrhDx12HookDesc* desc);
    bool InitDefaultTest();
    void Shutdown();
    bool IsInstalled();
    bool IsReady();
    const UrhDx12HookRuntime* GetRuntime();
}
