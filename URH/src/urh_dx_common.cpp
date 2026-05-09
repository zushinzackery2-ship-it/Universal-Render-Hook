#include "urh_dx_common.h"

namespace UrhDxCommon
{
    bool PatchVtable(void** vtable, int index, void* hookFn, void** originalFn)
    {
        if (!vtable || !hookFn)
        {
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return false;
        }

        if (originalFn)
        {
            *originalFn = vtable[index];
        }

        vtable[index] = hookFn;
        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
        return true;
    }

    bool RestoreVtable(void** vtable, int index, void* originalFn)
    {
        if (!vtable || !originalFn)
        {
            return false;
        }

        DWORD oldProtect = 0;
        if (!VirtualProtect(&vtable[index], sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect))
        {
            return false;
        }

        vtable[index] = originalFn;
        VirtualProtect(&vtable[index], sizeof(void*), oldProtect, &oldProtect);
        return true;
    }

    bool CreateProbeWindow(const wchar_t* className, const wchar_t* title, WNDCLASSEXW& windowClass, HWND& windowHandle)
    {
        ZeroMemory(&windowClass, sizeof(windowClass));
        windowClass.cbSize = sizeof(windowClass);
        windowClass.style = CS_HREDRAW | CS_VREDRAW;
        windowClass.lpfnWndProc = DefWindowProcW;
        windowClass.hInstance = GetModuleHandleW(nullptr);
        windowClass.lpszClassName = className;

        ATOM atom = RegisterClassExW(&windowClass);
        if (atom == 0 && GetLastError() != ERROR_CLASS_ALREADY_EXISTS)
        {
            return false;
        }

        windowHandle = CreateWindowExW(
            0,
            className,
            title,
            WS_OVERLAPPEDWINDOW,
            0,
            0,
            100,
            100,
            nullptr,
            nullptr,
            windowClass.hInstance,
            nullptr);
        return windowHandle != nullptr;
    }

    void DestroyProbeWindow(const WNDCLASSEXW& windowClass, HWND& windowHandle)
    {
        if (windowHandle)
        {
            DestroyWindow(windowHandle);
            windowHandle = nullptr;
        }

        if (windowClass.lpszClassName)
        {
            UnregisterClassW(windowClass.lpszClassName, windowClass.hInstance);
        }
    }
}
