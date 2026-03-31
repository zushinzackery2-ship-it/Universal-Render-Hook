<div align="center">

# Universal-Render-Hook

**统一 DX11 / DX12 / Vulkan 候选的图形 Hook 抽象层**

*VTable Patch | Present 拦截 | 后端自动探测与竞争仲裁*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Backends](https://img.shields.io/badge/Backends-DX11%20%7C%20DX12%20%7C%20Vulkan-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

> [!NOTE]
> **仓库定位**  
> 本仓库是无 UI 的 Hook 核心层。  
> 它负责 DX11 / DX12 Hook、运行时快照和后端仲裁，不负责 GUI、录屏、控制器或业务状态机。

> [!IMPORTANT]
> **Vulkan 边界**  
> 本仓库不会自己实现 Vulkan 的底层 Layer。  
> 当 `AutoHook` 启用 Vulkan 候选时，实际 Vulkan runtime 跟踪与回调接入仍由 `VulkanHook` 提供，本仓库只负责把它纳入统一仲裁面。

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **DX11 VTable Hook** | 创建临时 `IDXGISwapChain` 探测 `Present` 地址，再 patch 到 Hook 函数 |
| **DX12 VTable Hook** | 创建临时 DX12 device / swapchain，hook `Present` / `Present1` / `ExecuteCommandLists` |
| **Vulkan 候选接入** | 通过 `VulkanHook` 暴露的 runtime 和回调，把 Vulkan 纳入 `AutoHook` 候选 |
| **AutoHook 仲裁** | 根据 `backendMask` 安装启用后端，统计稳定帧后锁定最佳后端 |
| **运行时快照** | 对外暴露 `UrhAutoHookRuntime`、`UrhDx11HookRuntime`、`UrhDx12HookRuntime` |
| **Vulkan facade** | 对外暴露 `urh/vulkan.h`、`urh/vulkan_hook.h`、`urh/vulkan_types.h`，把 `VulkanHook` 的公开接口收口到 `URH` 入口下 |
| **WndProc Hook** | 可选阻断输入、可选默认 debug window |
| **诊断信息** | 输出 `seen / stableFrames / size / lockedBackend` 供调试与日志使用 |

---

## 架构

```text
应用程序
  IDXGISwapChain::Present / Present1 / ExecuteCommandLists / vkQueuePresentKHR
    |
    +----------+-----------+-----------+
    |          |           |           |
    v          v           v           v
DX11 Hook   DX12 Hook   VulkanHook   Diagnostics
VTable Patch VTable Patch 运行时转接   候选统计
    |          |           |
    +----------+-----------+
               |
               v
           UrhAutoHook
             ├─ 候选更新
             ├─ 稳定帧判断
             ├─ 后端锁定 / 升级
             └─ onSetup / onRender 分发
```

---

## AutoHook 仲裁流程

```text
URH::Init()
    │
    ├─ 根据 backendMask 安装启用的候选
    │   ├─ DX11: Init(Dx11Hook)
    │   ├─ DX12: Init(Dx12Hook)
    │   └─ Vulkan: VkhHook::Init()
    │
    └─ 等待候选帧回调

OnDx11Frame / OnDx12Frame / OnVulkanFrame
    │
    ├─ 更新 candidate.seen / candidate.width / candidate.height
    ├─ 累加 stableFrames
    └─ TryLockBackend()
        │
        ├─ 按 stableFrames 比较
        ├─ 同稳定度时按分辨率面积比较
        ├─ 再按优先级 Vulkan > DX12 > DX11 比较
        └─ 更新 lockedBackend
```

当前锁定规则允许：

- `DX11 -> DX12`
- `DX11 -> Vulkan`
- `DX12 -> Vulkan`

不会反向降级。

---

## DX11 Hook 原理

```text
1. 创建临时 DX11 device + swapchain
2. 读取 swapchain VTable
3. 取出 Present 槽位地址
4. 调整页保护
5. 原子替换为 HookPresent
6. 在 HookPresent 中组装运行时并触发回调
```

## DX12 Hook 原理

```text
1. 创建临时 DX12 device / command queue / swapchain
2. 读取 swapchain 与 command queue 的 VTable
3. patch Present / Present1 / ExecuteCommandLists
4. 在 Hook 回调里更新当前 command queue、buffer index、format、size
5. 通过统一 runtime 对外暴露给上层
```

---

## 核心 API

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **初始化** | `URH::FillDefaultDesc(desc)` | 填充默认配置 |
|  | `URH::Init(desc)` | 安装启用的候选后端 |
|  | `URH::Shutdown()` | 卸载 Hook 并清理状态 |
| **状态查询** | `URH::IsInstalled()` | 是否已安装 |
|  | `URH::IsReady()` | 是否已锁定有效后端 |
| **运行时** | `URH::GetRuntime()` | 获取当前锁定后的运行时 |
|  | `URH::GetDiagnostics(diag)` | 获取各候选的诊断信息 |

DX11 / DX12 也分别暴露独立入口：

- `urh/dx11_hook.h`
- `urh/dx12_hook.h`
- `urh/vulkan.h`
- `urh/vulkan_hook.h`
- `urh/vulkan_types.h`

---

## 目录结构

```text
Universal-Render-Hook/
└── URH/
    ├── include/urh/
    │   ├── urh.h
    │   ├── autohook.h
    │   ├── types.h
    │   ├── dx11_hook.h
    │   ├── dx11_types.h
    │   ├── dx12_hook.h
    │   ├── dx12_types.h
    │   ├── vulkan.h
    │   ├── vulkan_hook.h
    │   └── vulkan_types.h
    └── src/
        ├── urh_autohook.cpp
        ├── urh_autohook_dispatch.cpp
        ├── urh_autohook_helpers.cpp
        ├── urh_dx11_*.cpp
        ├── urh_dx12_*.cpp
        └── urh_*internal*.h
```

---

## 集成

这是源码仓库，不跟踪仓库内 GUI 层、录屏器、controller 或测试载荷。

- 只使用 DX11 / DX12 时，你的工程只需要接入 `URH`
- 需要把 Vulkan 也纳入 `AutoHook` 时，再把 `VulkanHook` 一并接入

---

## 依赖方向

```text
VulkanHook
    ↑
Universal-Render-Hook
```

`Universal-Render-Hook` 不反向依赖 `RainGui` 或 `InterRec`。

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
