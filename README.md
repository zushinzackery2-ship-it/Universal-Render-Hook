# Universal-Render-Hook

统一 `DX11 / DX12 / Vulkan` 的图形 Hook 抽象层。

## 定位

- 对外提供统一 `AutoHook` 接口
- 统一 runtime 和 diagnostics
- 当前是 headless hook core
- 不负责 GUI、视频编码、IPC、控制器

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

主要接口：

- `URH::FillDefaultDesc`
- `URH::Init`
- `URH::Shutdown`
- `URH::IsInstalled`
- `URH::IsReady`
- `URH::GetRuntime`
- `URH::GetDiagnostics`

原生公开类型现在以 `Urh*` 为规范名，例如：

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

- `URH` 依赖 `VulkanHook`
- 当前不再直接依赖上层 GUI 封装

## 适用场景

- 多后端统一探测
- 统一 render 回调调度
- 给上层录屏或诊断工具提供 backend-neutral runtime

## 许可

MIT，见根目录 `LICENSE`
