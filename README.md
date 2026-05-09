<div align="center">

# Universal-Render-Hook

**DX11 / DX12 / Vulkan 统一图形 Hook 抽象层**

*VTable Patch | Present 拦截 | Vulkan Layer | 后端探测与仲裁*

![C++](https://img.shields.io/badge/C%2B%2B-17-blue?style=flat-square)
![Platform](https://img.shields.io/badge/Platform-Windows%20x64-lightgrey?style=flat-square)
![Backends](https://img.shields.io/badge/Backends-DX11%20%7C%20DX12%20%7C%20Vulkan-green?style=flat-square)
![License](https://img.shields.io/badge/License-MIT-yellow?style=flat-square)

</div>

---

## 项目概述

统一的图形 API Hook 抽象层，通过 VTable Patch（DX11/DX12）和 Implicit Layer（Vulkan）拦截渲染提交点，自动探测并锁定活跃后端，向上层提供统一的 `onSetup` / `onRender` 回调和设备/交换链运行时信息。

> [!NOTE]
> 纯 Hook 核心层，无外部依赖（Vulkan 无需 SDK），产物为单个静态库 `URH.lib`。

## 特性

| 功能 | 说明 |
|:-----|:-----|
| **DX11 VTable Hook** | 创建临时 SwapChain 探测 Present 地址，patch 到 Hook 函数 |
| **DX12 VTable Hook** | 创建临时 Device/SwapChain，hook Present / Present1 / ExecuteCommandLists |
| **Vulkan Implicit Layer** | 通过 `vkNegotiateLoaderLayerInterfaceVersion` 协商版本，拦截 Instance/Device/Swapchain/Queue 生命周期 |
| **Vulkan 运行时跟踪** | Surface / Swapchain / Queue / Device 全链路跟踪，Late Attach 补建 |
| **AutoHook 仲裁** | 根据 `backendMask` 安装后端，统计稳定帧后锁定最佳后端 |
| **运行时快照** | 暴露 `UrhAutoHookRuntime` / `UrhDx11HookRuntime` / `UrhDx12HookRuntime` / `UrhVulkanHookRuntime` |
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
DX11 Hook   DX12 Hook   Vulkan Layer   Diagnostics
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
    │   ├─ DX11: UrhDx11Hook::Init()
    │   ├─ DX12: UrhDx12Hook::Init()
    │   └─ Vulkan: UrhVulkanHook::Init()
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

## Vulkan Hook 原理

```
1. 通过 vkNegotiateLoaderLayerInterfaceVersion 注册为 Implicit Layer
2. vkCreateInstance / vkCreateDevice 期间保存下层 Dispatch 表
3. 拦截 Surface / Swapchain / Queue 创建与销毁，构建跟踪映射
4. vkQueuePresentKHR 时组装 UrhVulkanHookRuntime，warmup 后触发回调
5. Late Attach：Layer 已生效但早期元数据不完整时，在 acquire/present 路径补建
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
├── build.bat                  # 编译脚本 (MSVC x64 Release)
└── URH/
    ├── include/urh/
    │   ├── urh.h              # 入口
    │   ├── autohook.h         # AutoHook API
    │   ├── types.h            # 公共类型
    │   ├── dx11_*.h           # DX11 Hook
    │   ├── dx12_*.h           # DX12 Hook
    │   └── vulkan*.h          # Vulkan Hook API
    └── src/
        ├── urh_autohook*.cpp          # AutoHook 仲裁
        ├── urh_dx_common.h/cpp        # DX11/DX12 公共层 (PatchVtable 等)
        ├── urh_dx11_*.cpp             # DX11 Hook 实现
        ├── urh_dx12_*.cpp             # DX12 Hook 实现
        ├── urh_vulkan_bootstrap.cpp   # Vulkan 初始化 / 状态查询
        ├── urh_vulkan_tracking.cpp    # Vulkan 运行时跟踪 (Swapchain 生命周期)
        ├── urh_vulkan_tracking_helpers.cpp # Vulkan 跟踪辅助 (窗口探测等)
        ├── urh_vulkan_dispatch.cpp    # Vulkan DispatchPresent
        ├── urh_vulkan_probe_helpers.cpp # Vulkan 探测辅助函数
        ├── urh_vulkan_probe_thread.cpp  # Vulkan 探测线程与控制
        ├── urh_vulkan_common.h        # Vulkan 公共工具 (HandleKey)
        ├── urh_vulkan_layer_*.cpp     # Vulkan Layer 导出与 Dispatch
        ├── urh_vulkan_internal.h      # Vulkan 内部状态
        ├── urh_vulkan_layer_internal.h # Vulkan Layer 内部类型
        ├── urh_vulkan_types_minimal.h # 最小 Vulkan 类型定义
        ├── urh_vulkan_pfns_minimal.h  # 最小 Vulkan 函数指针
        ├── urh_vulkan_minimal.h       # 向后兼容 shim
        └── urh_console_logger.h       # 日志
```

---

## 构建

```bat
build.bat
```

输出：`bin/URH.lib` (静态库)

要求：Visual Studio 2022，Windows x64，无需 Vulkan SDK。

---

## 集成

源码仓库，不含 GUI / 录屏器 / Controller / 测试载荷。

- 接入 `URH` 即可同时使用 DX11 / DX12 / Vulkan 后端
- Vulkan 通过 Implicit Layer 方式生效，无需额外注入

## 依赖方向

```
Universal-Render-Hook  (无外部依赖，不反向依赖 RainGui / InterRec)
```

---

<div align="center">

**Platform:** Windows x64 | **License:** MIT

</div>
