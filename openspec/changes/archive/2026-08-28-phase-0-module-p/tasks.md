## 1. P.2 GPU 契约检查

- [x] 1.1 在 `Tools/verify_gpu_contracts.py` 中为每对 CPU/GLSL 结构增加硬检查：成员类型序列与数组维度必须一致
- [x] 1.2 为 design.md 列出的已知 UBO / SSBO 声明增加硬检查：`std140` / `std430` 前缀必须正确
- [x] 1.3 从检查脚本、`Engine/Renderer/GPUContracts.hpp`、`Docs/Physara.md` 中删除已失效任务编号（`P.4`、`pre-refactor-validation-guards`）
- [x] 1.4 在仓库根目录运行 `python Tools/verify_gpu_contracts.py`，把通过/失败结果及残留告警记入 TaskDetail P.2

## 2. P.1 台账与锁定配方

- [x] 2.1 按 design.md 扩充 TaskDetail P.1 的锁定配方表（场景、Release、停靠窗口、相机、渲染设置、FXAA Quality、阴影、benchmark 120/300）
- [x] 2.2 增加电脑 B 环境块、预留的电脑 A 环境块，以及三条管线空行（Forward / Forward+ / Deferred），标记 `轮次: 0`，缺项填 `unavailable`
- [x] 2.3 在 TaskDetail P.1 写明操作步骤：加载 `Assets/Scenes/default.scene.json`，锁定停靠布局与相机，把 Scene View 的 `Size` 记为锁定帧缓冲尺寸，逐条管线采样，等 benchmark 完成后再抄数

## 3. P.1 电脑 B 轮次 0 采集

- [x] 3.1 操作者：Release 构建，加载默认场景，套用锁定配方，把硬件信息与首次 Scene View `W x H` 写入电脑 B 环境块（Size 889x611 已锁定；硬件 i7-7700 / GTX 1060 6G / 驱动 580 / 32G）
- [x] 3.2 操作者：把 Forward、Forward+、Deferred 的面板与 benchmark 数字填进 P.1 表；缺失字段标 `unavailable`
- [x] 3.3 操作者：观察默认场景后，为每条管线写视觉通过/缺陷说明（三条管线均正常无异常）
- [x] 3.4 若已提供三条管线的 RenderDoc 抓帧，把文件名和提取的 pass 时间补进同一轮次 0 行；否则 RenderDoc 保持 `unavailable`（三份 rdc 已登记并全部分析：Forward / Forward+ / Deferred）

## 4. P.3 阶段一优先级复核

- [x] 4.1 依据电脑 B 轮次 0 数字，在 TaskDetail P.3 写带一行证据的攻击顺序列表；不要改 Development.md 的任务编号
- [x] 4.2 在 Development.md 模块 P 下加一句指向 P.3 列表的说明；卫生类任务保持原文顺序，除非数字证明它们在热路径上（另新增 3.4 条目记录 GBuffer 合并缺陷，不动既有编号）

## 5. 模块 P 签收

- [x] 5.1 确认电脑 B 三条管线轮次 0 已齐、电脑 A 仍是空预留位、契约脚本运行结果已记录
- [x] 5.2 将 Development.md 的 P.1 / P.2 / P.3 标为 `[√]`，并在 TaskDetail 写完成说明（偏差、缺失 rdc、优先级以 B 机为准）
