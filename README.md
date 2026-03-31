<div align="center">

# Universal-Render-Hook

**DX11 / DX12 / Vulkan 统一图形 Hook 抽象层**

*VTable Patch | Present 拦截 | 后端探测与仲裁*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Backends](https://img.shields.io/badge/Backends-DX11%20%7C%20DX12%20%7C%20Vulkan-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

> [!NOTE]
> **仓库边界**  
> 本仓库是无 UI 的 Hook 核心层，负责 DX11 / DX12 Hook、运行时快照和后端仲裁，不含 GUI、录屏或控制器。

> [!IMPORTANT]
> **Vulkan 边界**  
> 本仓库不实现 Vulkan Layer，Vulkan 运行时跟踪由 `VulkanHook` 提供，本仓库仅负责将其纳入统一仲裁。

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **DX11 VTable Hook** | 创建临时 SwapChain 探测 Present 地址，patch 到 Hook 函数 |
| **DX12 VTable Hook** | 创建临时 Device/SwapChain，hook Present / Present1 / ExecuteCommandLists |
| **Vulkan 接入** | 通过 `VulkanHook` 暴露的 runtime 和回调纳入 AutoHook |
| **AutoHook 仲裁** | 根据 `backendMask` 安装后端，统计稳定帧后锁定最佳后端 |
| **运行时快照** | 暴露 `UrhAutoHookRuntime` / `UrhDx11HookRuntime` / `UrhDx12HookRuntime` |
| **Vulkan 门面** | 暴露 `urh/vulkan*.h`，收口 VulkanHook 公开接口 |
| **WndProc Hook** | 可选阻断输入、可选 debug window |
| **诊断信息** | 输出 seen / stableFrames / size / lockedBackend |

---

## 架构

```
应用程序
  IDXGISwapChain::Present / ExecuteCommandLists / vkQueuePresentKHR
    │
    ├──────────┬──────────┬──────────┐
    ▼          ▼          ▼          ▼
DX11 Hook   DX12 Hook   VulkanHook   Diagnostics
    │          │          │
    └──────────┴──────────┘
               │
               ▼
          UrhAutoHook
            ├─ 后端状态更新
            ├─ 稳定帧判断
            ├─ 后端锁定 / 升级
            └─ onSetup / onRender 分发
```

---

## AutoHook 仲裁流程

```
URH::Init()
    ├─ 根据 backendMask 安装后端
    │   ├─ DX11: Dx11Hook::Init()
    │   ├─ DX12: Dx12Hook::Init()
    │   └─ Vulkan: VkhHook::Init()
    └─ 等待后端帧回调

OnDx11Frame / OnDx12Frame / OnVulkanFrame
    ├─ 更新 seen / width / height
    ├─ 累加 stableFrames
    └─ TryLockBackend()
        ├─ 按 stableFrames 比较
        ├─ 同稳定度按分辨率面积比较
        ├─ 再按优先级 Vulkan > DX12 > DX11
        └─ 更新 lockedBackend
```

锁定规则：`DX11 → DX12 → Vulkan`，不反向降级。

---

## DX11 Hook 原理

```
1. 创建临时 DX11 device + swapchain
2. 读取 swapchain VTable
3. 取出 Present 槽位地址
4. 调整页保护
5. 原子替换为 HookPresent
6. 在 HookPresent 中组装运行时并触发回调
```

## DX12 Hook 原理

```
1. 创建临时 DX12 device / command queue / swapchain
2. 读取 swapchain 与 command queue 的 VTable
3. patch Present / Present1 / ExecuteCommandLists
4. Hook 回调中更新 command queue / buffer index / format / size
5. 通过统一 runtime 暴露给上层
```

---

## 核心 API

| 分类 | API | 说明 |
|:-----|:----|:-----|
| **初始化** | `URH::FillDefaultDesc(desc)` | 填充默认配置 |
|  | `URH::Init(desc)` | 安装后端 |
|  | `URH::Shutdown()` | 卸载 Hook，清理状态 |
| **状态** | `URH::IsInstalled()` | 是否已安装 |
|  | `URH::IsReady()` | 是否已锁定有效后端 |
| **运行时** | `URH::GetRuntime()` | 获取锁定的后端运行时 |
|  | `URH::GetDiagnostics(diag)` | 获取各后端诊断信息 |

独立后端入口：
- `urh/dx11_hook.h` / `urh/dx11_types.h`
- `urh/dx12_hook.h` / `urh/dx12_types.h`
- `urh/vulkan.h` / `urh/vulkan_hook.h` / `urh/vulkan_types.h`

---

## 目录结构

```
Universal-Render-Hook/
└── URH/
    ├── include/urh/
    │   ├── urh.h            # 入口
    │   ├── autohook.h       # AutoHook API
    │   ├── types.h          # 公共类型
    │   ├── dx11_*.h         # DX11 Hook
    │   ├── dx12_*.h         # DX12 Hook
    │   └── vulkan*.h        # Vulkan 门面
    └── src/
        ├── urh_autohook*.cpp
        ├── urh_dx11_*.cpp
        ├── urh_dx12_*.cpp
        └── urh_*internal*.h
```

---

## 集成

源码仓库，不含 GUI / 录屏器 / Controller / 测试载荷。

- 仅 DX11 / DX12：接入 `URH`
- 包含 Vulkan：同时接入 `VulkanHook`

## 依赖方向

```
VulkanHook
    ↑
Universal-Render-Hook  (不反向依赖 RainGui / InterRec)
```

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
