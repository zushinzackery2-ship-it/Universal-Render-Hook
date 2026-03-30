#pragma once

#include "dx11_types.h"

namespace UrhDx11Hook
{
    void FillDefaultDesc(UrhDx11HookDesc* desc);
    bool Init(const UrhDx11HookDesc* desc);
    void Shutdown();
    bool IsInstalled();
    bool IsReady();
    const UrhDx11HookRuntime* GetRuntime();
}
