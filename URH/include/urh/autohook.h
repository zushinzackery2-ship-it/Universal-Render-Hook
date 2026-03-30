#pragma once

#include "types.h"

namespace UrhAutoHook
{
    void FillDefaultDesc(UrhAutoHookDesc* desc);
    bool Init(const UrhAutoHookDesc* desc);
    void Shutdown();
    bool IsInstalled();
    bool IsReady();
    const UrhAutoHookRuntime* GetRuntime();
    void GetDiagnostics(UrhAutoHookDiagnostics* diagnostics);
}
