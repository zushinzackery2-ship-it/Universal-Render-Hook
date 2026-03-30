#include "urh_dx11_internal.h"

namespace UrhDx11HookInternal
{
    bool IsInteractiveVisible()
    {
        bool visible = false;

        if (!g_state.desc.isVisible)
        {
            return visible;
        }

        __try
        {
            return visible || g_state.desc.isVisible(g_state.desc.userData);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            g_state.deviceLost = true;
            return visible;
        }
    }

    LRESULT CALLBACK HookedWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
    {
        if (g_state.unloading || !g_state.backendReady || !g_state.originalWndProc)
        {
            if (g_state.originalWndProc)
            {
                return CallWindowProcW(g_state.originalWndProc, hwnd, msg, wParam, lParam);
            }

            return DefWindowProcW(hwnd, msg, wParam, lParam);
        }

        if (g_state.desc.blockInputWhenVisible && IsInteractiveVisible())
        {
            switch (msg)
            {
            case WM_LBUTTONDOWN:
            case WM_LBUTTONUP:
            case WM_LBUTTONDBLCLK:
            case WM_RBUTTONDOWN:
            case WM_RBUTTONUP:
            case WM_RBUTTONDBLCLK:
            case WM_MBUTTONDOWN:
            case WM_MBUTTONUP:
            case WM_MBUTTONDBLCLK:
            case WM_MOUSEWHEEL:
            case WM_MOUSEHWHEEL:
            case WM_MOUSEMOVE:
            case WM_KEYDOWN:
            case WM_KEYUP:
            case WM_SYSKEYDOWN:
            case WM_SYSKEYUP:
            case WM_CHAR:
                return TRUE;
            default:
                break;
            }
        }

        return CallWindowProcW(g_state.originalWndProc, hwnd, msg, wParam, lParam);
    }

    PresentFn ResolvePresentFn(IDXGISwapChain* swapChain)
    {
        if (swapChain)
        {
            auto** vtable = *reinterpret_cast<void***>(swapChain);
            if (vtable)
            {
                auto fn = reinterpret_cast<PresentFn>(vtable[8]);
                if (fn && fn != HookPresent)
                {
                    return fn;
                }
            }
        }

        return g_state.originalPresent;
    }

    HRESULT CallOriginalPresentSafe(
        IDXGISwapChain* swapChain,
        UINT syncInterval,
        UINT flags,
        DWORD* exceptionCode)
    {
        if (exceptionCode)
        {
            *exceptionCode = 0;
        }

        PresentFn fn = ResolvePresentFn(swapChain);
        if (!fn)
        {
            return E_FAIL;
        }

        __try
        {
            return fn(swapChain, syncInterval, flags);
        }
        __except (EXCEPTION_EXECUTE_HANDLER)
        {
            if (exceptionCode)
            {
                *exceptionCode = GetExceptionCode();
            }

            return E_FAIL;
        }
    }
}
