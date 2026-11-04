# NVRHI：顶层架构与 Vulkan 执行链技术分析

> 分析基线：[NVIDIA-RTX/NVRHI @ 6b96fb03e07539f08327aea76c56d55f1de9d906](https://github.com/NVIDIA-RTX/NVRHI/tree/6b96fb03e07539f08327aea76c56d55f1de9d906)。
> 源码许可：MIT；本地 ThirdParty/nvrhi 是纯参考快照，不加入 CMake 或当前链接。
>
> 本文先建立 RHI 的对象、状态和生存期架构，再沿 Vulkan 的“创建 → 绑定 → 记录 → 同步 → 提交 → 回收”展开。伪代码用于还原程序逻辑，省略模板、错误传播和部分扩展条件；概念辅助函数不代表实际 API。接口和行为以本固定版本源码为准。
>
> 配套文档：[Filament_Rendering_Pipeline_Analysis.md](Filament_Rendering_Pipeline_Analysis.md)。Filament 分析说明渲染层如何决定一帧的工作；本文说明同一类绘制需求进入 RHI 后如何成为可执行、可同步、可回收的 Vulkan 工作。Filament 实际使用自己的 backend，不依赖 NVRHI。

## 按章节阅读

按当前问题打开下列章节。各分卷均可返回本目录或定位到固定版本的源码索引。

- <a id="n0"></a>[0. 整体架构：RHI 如何接住渲染管线的执行需求](NVRHI/InterfacesAndDevice.md#n0)
- <a id="n1"></a>[1. 顶层接口：对象、状态、命令和所有权](NVRHI/InterfacesAndDevice.md#n1)
- <a id="n2"></a>[2. Vulkan 入口：把外部原生设备接入公共架构](NVRHI/InterfacesAndDevice.md#n2)
- <a id="n3"></a>[3. 资源实现：从描述到内存、视图和状态元数据](NVRHI/ResourcesAndPipelines.md#n3)
- <a id="n4"></a>[4. Shader 与绑定：建立跨 API 的资源 ABI](NVRHI/ResourcesAndPipelines.md#n4)
- <a id="n5"></a>[5. Pipeline 与附件：将不可变状态和可变目标分开](NVRHI/ResourcesAndPipelines.md#n5)
- <a id="n6"></a>[6. 记录与上传：CommandList、UploadManager 和常量版本](NVRHI/RecordingAndBarriers.md#n6)
- <a id="n7"></a>[7. 资源状态：从用途推导到 Vulkan Barrier](NVRHI/RecordingAndBarriers.md#n7)
- <a id="n8"></a>[8. 执行状态：Graphics、Compute 与原生 Draw](NVRHI/ExecutionAndLifetime.md#n8)
- <a id="n9"></a>[9. 队列提交：完成值、跨队列等待和线程关系](NVRHI/ExecutionAndLifetime.md#n9)
- <a id="n10"></a>[10. 完成与回收：让 GPU 生存期闭环](NVRHI/ExecutionAndLifetime.md#n10)
- <a id="n11"></a>[11. 扩展路径：原生互操作、稀疏资源和光线追踪](NVRHI/ExtensionsAndWalkthrough.md#n11)
- <a id="n12"></a>[12. 完整例程：把一次上传、绘制和采样贯穿起来](NVRHI/ExtensionsAndWalkthrough.md#n12)
- <a id="na"></a>[A. 源码索引](NVRHI/SourceIndex.md#na)
