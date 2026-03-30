<div align="center">

# Universal-Render-Hook

**统一 DX11 / DX12 / Vulkan 的图形 Hook 抽象层**

*Headless Hook Core | Backend-Neutral Runtime | Diagnostics*

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

## 目录

```text
Universal-Render-Hook/
└── URH/
    ├── include/urh/
    └── src/
```

## 对外接口

头文件入口：

```cpp
#include <urh/urh.h>
```

主要能力：

- `URH::FillDefaultDesc`
- `URH::Init`
- `URH::Shutdown`
- `URH::IsInstalled`
- `URH::IsReady`
- `URH::GetRuntime`
- `URH::GetDiagnostics`

原生公开类型统一使用 `Urh*` 前缀：

- `UrhAutoHookDesc`
- `UrhAutoHookRuntime`
- `UrhDx11HookDesc`
- `UrhDx12HookDesc`

## 依赖方向

```text
VulkanHook
    ↑
Universal-Render-Hook
```

- `URH` 只向下依赖 `VulkanHook`
- 不反向依赖 `RainGui` 或 `InterRec`

## 许可

MIT，见根目录 `LICENSE`
