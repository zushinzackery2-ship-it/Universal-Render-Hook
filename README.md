<div align="center">

# Universal-Render-Hook

**统一 DX11 / DX12 / Vulkan 的图形 Hook 抽象层**

*VTable Patch | Present 拦截 | 后端自动探测与竞争仲裁*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Backends](https://img.shields.io/badge/Backends-DX11%20%7C%20DX12%20%7C%20Vulkan-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

> [!NOTE]
> **仓库定位**  
> 本仓库是 headless hook core，不负责 GUI、录屏、IPC、控制器。  
> 上层业务只应通过公开头 `urh/*` 使用它。

> [!IMPORTANT]
> **Hook 原理说明**  
> DX11 / DX12 通过创建临时 SwapChain 探测 VTable 地址，然后 Patch `Present` / `Present1` / `ExecuteCommandLists` 等函数指针。  
> Vulkan 路径委托给 `VulkanHook` 库，通过 Implicit Layer 机制注入。

---

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **DX11 VTable Hook** | 创建临时 `D3D11CreateDeviceAndSwapChain` + `IDXGISwapChain`，读取 VTable 中 `Present` 函数指针（索引 8），修改页面保护后原子替换为 Hook 函数 |
| **DX12 VTable Hook** | 创建临时 `D3D12CreateDevice` + `IDXGISwapChain3`，Hook `Present` / `Present1`（索引 8 / 22）以及 `ID3D12CommandQueue::ExecuteCommandLists`（索引 10）|
| **Vulkan 路径** | 委托 `VulkanHook` 库，通过 Layer 拦截 `vkQueuePresentKHR`，本库只做回调适配 |
| **后端自动探测** | `AutoHook` 模式同时安装 DX11 / DX12 / Vulkan 三个后端，监听首个稳定渲染的后端并锁定 |
| **竞争仲裁** | 部分引擎同时初始化多个后端，`AutoHook` 通过 `stableFrames` 计数 + 分辨率比较选择主后端，避免在错误后端上渲染 |
| **Warmup 帧** | 跳过前 N 帧（默认 3），等待后端资源稳定后再调用 `onSetup` / `onRender` |
| **RenderTargetView 管理** | DX11 每帧自动获取 BackBuffer 并创建 `ID3D11RenderTargetView`，支持 Resize 自动重建 |
| **CommandAllocator / Fence** | DX12 为每个 BackBuffer 创建独立 `ID3D12CommandAllocator`，使用 `ID3D12Fence` 同步 GPU 完成状态 |
| **Multithread 保护** | DX11 自动获取 `ID3D11Multithread` 接口并在渲染期间加锁，避免与游戏线程冲突 |
| **WndProc Hook** | 可选拦截窗口消息，支持 `blockInputWhenVisible` 阻止鼠标键盘传递给游戏 |
| **诊断信息** | `GetDiagnostics()` 返回各后端的探测状态、稳定帧数、分辨率，便于调试 |

---

## 核心 API

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **初始化** | `URH::FillDefaultDesc(desc)` | 填充默认配置 |
|  | `URH::Init(desc)` | 安装 Hook 并开始监听 |
|  | `URH::Shutdown()` | 卸载 Hook，清理资源 |
| **状态查询** | `URH::IsInstalled()` | 是否已调用 Init |
|  | `URH::IsReady()` | 是否已锁定有效后端 |
| **运行时** | `URH::GetRuntime()` | 获取当前 `UrhAutoHookRuntime` |
|  | `URH::GetDiagnostics(diag)` | 获取后端诊断信息 |

---

## 回调类型

```cpp
using UrhAutoHookSetupCallback    = void (*)(const UrhAutoHookRuntime* runtime, void* userData);
using UrhAutoHookRenderCallback   = void (*)(const UrhAutoHookRuntime* runtime, void* userData);
using UrhAutoHookVisibleCallback  = bool (*)(void* userData);
using UrhAutoHookShutdownCallback = void (*)(void* userData);
```

**UrhAutoHookRuntime 结构**：

| 字段 | 类型 | 说明 |
|:-----|:-----|:-----|
| `backend` | `UrhAutoHookBackend` | 当前后端类型（DX11 / DX12 / Vulkan） |
| `hwnd` | `void*` | 渲染窗口句柄 |
| `nativeRuntime` | `void*` | 底层 Runtime 指针（`UrhDx11HookRuntime*` / `UrhDx12HookRuntime*` / `VkhHookRuntime*`） |
| `bufferCount` | `UINT` | SwapChain BackBuffer 数量 |
| `backBufferIndex` | `UINT` | 当前帧 BackBuffer 索引 |
| `backBufferFormat` | `DXGI_FORMAT` | BackBuffer 格式 |
| `width` / `height` | `float` | 渲染分辨率 |
| `frameCount` | `UINT` | 累计帧数 |

---

## 架构

```
应用程序
  IDXGISwapChain::Present / vkQueuePresentKHR / ...
    |
    +----------+-----------+
    |          |           |
    v          v           v
DX11 Hook   DX12 Hook   Vulkan Hook
VTable Patch VTable Patch (VulkanHook)
Present(8)  Present(8/22) Layer 拦截
    |          |           |
    +----------+-----------+
               |
               v
AutoHook
  后端仲裁: 监听 dx11/dx12/vulkan 帧回调，选择稳定后端锁定
    |
    v
  用户回调 onSetup / onRender
```

---

## 后端探测与仲裁流程

```
URH::Init()
    │
    ├─ 根据 backendMask 安装启用的后端
    │   ├─ DX11: 创建临时 SwapChain → 探测 VTable → Patch Present
    │   ├─ DX12: 创建临时 Device/SwapChain → 探测 VTable → Patch Present/ExecuteCommandLists
    │   └─ Vulkan: 调用 VHK::Init()，注册 Layer 回调
    │
    └─ 等待帧回调

OnDx11Frame / OnDx12Frame / OnVulkanFrame
    │
    ├─ 更新对应 Candidate 的 stableFrames / width / height
    │
    └─ TryLockBackend()
        │
        ├─ 若 lockedBackend == Unknown:
        │   ├─ 选择 stableFrames 最高且分辨率最大的后端
        │   └─ 锁定后端，后续忽略其他后端的帧回调
        │
        └─ 若已锁定:
            └─ 只处理锁定后端的帧，调用 onSetup (首次) / onRender (每帧)
```

---

## VTable Hook 原理 (DX11)

```cpp
// 1. 创建临时 SwapChain 获取 VTable
D3D11CreateDeviceAndSwapChain(..., &device, &swapChain);
void** vtable = *reinterpret_cast<void***>(swapChain);

// 2. 保存原始函数指针
originalPresent = reinterpret_cast<PresentFn>(vtable[8]);

// 3. 修改页面保护
VirtualProtect(vtable + 8, sizeof(void*), PAGE_EXECUTE_READWRITE, &oldProtect);

// 4. 原子替换
InterlockedExchangePointer(&vtable[8], HookPresent);

// 5. Hook 函数
HRESULT HookPresent(IDXGISwapChain* swapChain, UINT syncInterval, UINT flags)
{
    // 渲染 Overlay
    RenderFrame(swapChain);
    // 调用原始函数
    return originalPresent(swapChain, syncInterval, flags);
}
```

---

## 目录结构

```
Universal-Render-Hook/
└── URH/
    ├── include/urh/
    │   ├── urh.h                    # 主入口，URH 命名空间封装
    │   ├── autohook.h               # UrhAutoHook 静态类声明
    │   ├── types.h                  # UrhAutoHookDesc / UrhAutoHookRuntime 定义
    │   ├── dx11_hook.h              # UrhDx11Hook 静态类声明
    │   ├── dx11_types.h             # UrhDx11HookDesc / UrhDx11HookRuntime 定义
    │   ├── dx12_hook.h              # UrhDx12Hook 静态类声明
    │   └── dx12_types.h             # UrhDx12HookDesc / UrhDx12HookRuntime 定义
    └── src/
        ├── urh_autohook.cpp         # AutoHook 主逻辑
        ├── urh_autohook_dispatch.cpp # 后端回调分发
        ├── urh_autohook_helpers.cpp # 后端仲裁 / Desc 转换
        ├── urh_autohook_internal.h  # AutoHookState / Candidate 定义
        ├── urh_dx11_bootstrap.cpp   # DX11 临时 SwapChain 创建
        ├── urh_dx11_probe.cpp       # DX11 VTable 探测
        ├── urh_dx11_hooks_present.cpp # DX11 Present Hook 实现
        ├── urh_dx11_hooks_common.cpp  # DX11 Hook 安装 / 卸载
        ├── urh_dx11_resources.cpp   # DX11 RenderTargetView 管理
        ├── urh_dx11_context.cpp     # DX11 DeviceContext 封装
        ├── urh_dx11_internal.h      # DX11 ModuleState 定义
        ├── urh_dx12_bootstrap.cpp   # DX12 临时 Device/SwapChain 创建
        ├── urh_dx12_probe.cpp       # DX12 VTable 探测
        ├── urh_dx12_hooks_present.cpp # DX12 Present Hook 实现
        ├── urh_dx12_hooks_aux.cpp   # DX12 ExecuteCommandLists Hook
        ├── urh_dx12_hooks_common.cpp  # DX12 Hook 安装 / 卸载
        ├── urh_dx12_resources.cpp   # DX12 CommandAllocator / Fence 管理
        ├── urh_dx12_context.cpp     # DX12 CommandList 封装
        └── urh_dx12_internal.h      # DX12 ModuleState 定义
```

---

## 快速开始

```cpp
#include <urh/urh.h>

void OnSetup(const UrhAutoHookRuntime* runtime, void* userData)
{
    // 后端锁定后首次调用
    // runtime->backend 指示当前后端类型
}

void OnRender(const UrhAutoHookRuntime* runtime, void* userData)
{
    // 每帧 Present 前调用
    // 可通过 runtime->nativeRuntime 获取底层资源
}

int main()
{
    UrhAutoHookDesc desc;
    URH::FillDefaultDesc(&desc);
    desc.onSetup = OnSetup;
    desc.onRender = OnRender;
    desc.backendMask = UrhAutoHookBackendMask_All; // DX11 | DX12 | Vulkan
    desc.warmupFrames = 3;

    if (!URH::Init(&desc))
    {
        return 1;
    }

    // ... 等待游戏渲染 ...

    URH::Shutdown();
    return 0;
}
```

---

## 依赖方向

```
VulkanHook
    ↑
Universal-Render-Hook (本仓库)
    ↑
RainGui / InterRec (可选上层)
```

- `URH` 只向下依赖 `VulkanHook`
- 不反向依赖 `RainGui` 或 `InterRec`

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
