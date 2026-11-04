# Filament 渲染管线：架构、数据流与执行逻辑

> 分析基线：[google/filament @ b4dfdb073d5543d8569d37e0ee26f79cf794db14](https://github.com/google/filament/tree/b4dfdb073d5543d8569d37e0ee26f79cf794db14)。
> 源码许可：Apache-2.0；分析/伪代码不改变上游版权与移植时的来源保留要求。
>
> 本文按“资源进入引擎 → 一帧 CPU 准备 → Pass 与绘制命令组织 → 后端执行 → GPU 着色 → 输出与帧间复用”分析现有实现。伪代码保留依赖关系与关键分支，省略错误处理、模板和平台条件；标注“概念”的记录或辅助函数只用于解释代码，不是项目新增接口。
>
> 配套文档：[NVRHI_RHI_Vulkan_Analysis.md](NVRHI_RHI_Vulkan_Analysis.md)。两份文档分别研究渲染前端和 RHI 执行层的工作。Filament 使用自己的 backend，并不调用 NVRHI；对应关系在第 8、15 节明确说明。
>
> 本文保留上游的动画、Skin、Morph 等能力分析，不表示 Physara 采用这些功能。Physara 的静态场景范围、标准资产映射和时域采纳合同以 [Architecture.md](../Architecture.md) 为准；下文单独标出的适配边界用于解释技术依据，不替代项目架构。

## 按章节阅读

按当前问题打开下列章节。各分卷均可返回本目录或定位到固定版本的源码索引。

- <a id="f0"></a>[0. 整体架构：一帧如何贯穿全部模块](Filament/FrameSetup.md#f0)
- <a id="f1"></a>[1. 资源进入引擎：建立可被一帧引用的对象](Filament/FrameSetup.md#f1)
- <a id="f2"></a>[2. 帧开始：提交材质变化并建立执行上下文](Filament/FrameSetup.md#f2)
- <a id="f3"></a>[3. View 准备：从 Camera 与 Scene 得到统一的帧数据](Filament/FrameSetup.md#f3)
- <a id="f4"></a>[4. 可见性与灯光：把场景缩减为本帧的工作集](Filament/VisibilityAndCommands.md#f4)
- <a id="f5"></a>[5. GPU 数据准备：将工作集转换为 Uniform 和描述符](Filament/VisibilityAndCommands.md#f5)
- <a id="f6"></a>[6. 绘制命令：把可见 Primitive 转成排序后的执行记录](Filament/VisibilityAndCommands.md#f6)
- <a id="f7"></a>[7. FrameGraph：把 Draw 列表放入有依赖的 Pass](Filament/FrameGraphAndPasses.md#f7)
- <a id="f8"></a>[8. 后端边界：从 Pass 回调到原生 GPU 命令](Filament/FrameGraphAndPasses.md#f8)
- <a id="f9"></a>[9. GPU 前置阶段：深度、阴影、AO 与反射输入](Filament/FrameGraphAndPasses.md#f9)
- <a id="f10"></a>[10. 主表面阶段：顶点数据如何变成可着色片元](Filament/FrameGraphAndPasses.md#f10)
- <a id="f11"></a>[11. 光照求值：材质响应、直接光和 IBL 的完整调用链](Filament/Lighting.md#f11)
- <a id="f12"></a>[12. 颜色合成：折射、透明、天空、雾与自发光](Filament/CompositionAndDisplay.md#f12)
- <a id="f13"></a>[13. 图像输出：Resolve、TAA、镜头和显示变换](Filament/CompositionAndDisplay.md#f13)
- <a id="f14"></a>[14. 帧结束：提交、呈现、历史保存和资源回收](Filament/FrameEndAndDraw.md#f14)
- <a id="f15"></a>[15. 将一次 Draw 从头追踪到尾](Filament/FrameEndAndDraw.md#f15)
- <a id="fa"></a>[A. 源码索引](Filament/SourceIndex.md#fa)
