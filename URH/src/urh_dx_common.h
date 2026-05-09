#pragma once

#include <Windows.h>

namespace UrhDxCommon
{
    bool PatchVtable(void** vtable, int index, void* hookFn, void** originalFn);
    bool RestoreVtable(void** vtable, int index, void* originalFn);
    bool CreateProbeWindow(const wchar_t* className, const wchar_t* title, WNDCLASSEXW& windowClass, HWND& windowHandle);
    void DestroyProbeWindow(const WNDCLASSEXW& windowClass, HWND& windowHandle);

    template <typename StateT>
    bool IsInteractiveVisible(StateT& state)
    {
        bool visible = false;

        if (!state.desc.isVisible)
        {
            return visible;
        }

        __try
        {
            return visible || state.desc.isVisible(state.desc.userData);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            state.deviceLost = true;
            return visible;
        }
    }
}
