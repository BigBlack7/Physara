## 1. 契约校验 — CPU 布局编译期锁定(P.4 / D1)

- [x] 1.1 确定断言归属:在 `Engine/Renderer/GPUContracts.hpp` 就近、或新建配套头 `GPUContractsAsserts.hpp`(被 GPUContracts 包含)
- [x] 1.2 为 8 个 GPU 结构逐一添加 `static_assert(sizeof(...) == N)` 与逐字段 `static_assert(offsetof(...) == N)`(`CameraData`/`ObjectData`/`MaterialGPUData`/`LightData`/`ShadowData`/`IBLData`/`ClusterGridData`/`FrameUniforms`)
- [ ] 1.3 编译验证:临时改动某字段顺序/类型,确认编译失败并定位到违例结构,随后回滚 —— **待用户在 VS 环境构建验证**(本会话 shell 无 cmake/MSBuild)

## 2. 契约校验 — 脚本扩展 CPU↔GLSL 与 binding(P.4 / D1,D2,D3)

- [x] 2.1 扩展 `Tools/verify_gpu_contracts.py`:解析 GLSL 镜像结构字段序列与类型(`Common.glsl`/`FrameUniforms.glsl`/`Material.glsl`)
- [x] 2.2 与 C++ 结构逐字段比对,分歧时以非零退出码失败并报告(结构名/字段/两侧类型)—— 采用**字节布局尺寸**比对以正确处理 C++ 4×u32 ⇔ GLSL uvec4 的有意等价
- [x] 2.3 解析 `PHYSARA_BINDING_*` 定义值,与 C++ `GPUBufferBinding`/`GPUTextureBinding` 枚举值比对,错位则失败(原脚本已具备,保留)
- [x] 2.4 报告死枚举与同值重载清单 —— 运行输出:死枚举 `RenderSettings`/`Shadow`/`IBL`(比计划多发现 RenderSettings),重载 `binding=0`(Camera/FrameUniforms)、`binding=4`(4 项)
- [x] 2.5 布局纪律断言:检测非 `vec4`/`mat4`/`uvec4` 成员与未对齐 16B 的标量组并告警(当前结构零违例)
- [x] 2.6 解析失败按 fail-loud 处理(结构缺失/未知类型即抛异常 exit 1,不静默跳过)
- [x] 2.7 本地运行 `python Tools/verify_gpu_contracts.py` 通过(布局全绿;warnings 为 2.4 的预期死枚举/重载报告,非失败)

## 3. 视觉回归 — 确定性捕获(P.2 / D4)

> **交接规格(本会话 shell 无法构建/运行验证,待用户 VS 环境落地)**:底层 `RendererCapture::CaptureTexture(cmdList, texture, desc)` 已可将纹理写 PNG。组 3 需在其上加**编排**:①经编辑器现有 capture-request 路径(`EditorApp::ProcessCaptureRequests`)加一个"回归捕获"入口;②固定 `RenderView`(相机位姿/分辨率)与默认场景;③**门控 IBL 就绪**(查 `IBLResources`/`Renderer` 的预计算完成标志)+ N 帧 warmup 后再取 `Renderer::GetSceneColorTexture()` 捕获。产物 PNG 交 `Tools/compare_golden.py` 比对。

- [ ] 3.1 定义回归捕获剖面:加载默认场景 + 固定相机位姿 + 固定分辨率 + 关闭抖动 —— **待构建落地**
- [ ] 3.2 复用 `RendererCapture`,在 IBL 预计算完成 + 约定 warmup 帧后再捕获 —— **待构建落地**
- [ ] 3.3 验证可复现:同设备同构建重复捕获,结果在容差内一致 —— **待运行验证**

## 4. 视觉回归 — golden 基准与比对(P.2 / D4)

- [ ] 4.1 约定 golden 基准存储(目录、格式;评估仓库体积对策)—— `compare_golden.py` 已定:golden 存于指定目录 + 同目录 `GOLDEN_REASONS.log` 记录刷新原因;格式 PNG
- [x] 4.2 实现容差比对:逐像素差阈值 + 允许失败像素占比,失败时输出 diff 图与量化指标 —— `Tools/compare_golden.py`(Pillow+numpy,`--tol`/`--fail-ratio`/`--diff`,已 `--help` 验证)
- [ ] 4.3 标定初始容差阈值(本机驱动跑数次取经验值)—— **待真实截图**(需组 3 捕获路径 + 构建运行)
- [x] 4.4 实现显式刷新流程:有意画面变更时重生成 golden 并记录原因,禁止静默覆盖 —— `--update <golden> <candidate> --reason "..."`,无 reason 拒绝执行

## 5. 工作流接入与文档(gate)

- [x] 5.1 确立约定:凡改 GPU 布局/binding 的任务(尤其模块 4.2)前置跑契约校验并须通过 —— 已写入 Development.md P.4 + 顺序原则 #6
- [x] 5.2 确立约定:凡触及渲染 pass 的模块,重构前后各捕获比对 —— 已写入 Development.md P.2 与 visual-regression-testing spec
- [x] 5.3 更新 `Docs/Development.md` 将 P.2/P.4 标记为完成,并补简短使用说明(如何跑校验/回归)—— P.4 标记完成 + 用法;P.2 标记部分实现(比对器完成、捕获编排待落地)+ 用法
