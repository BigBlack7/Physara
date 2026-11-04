# Filament · 帧结束与一次 Draw 的执行链

[Filament 技术目录](../Filament_Rendering_Pipeline_Analysis.md) · [源码索引](SourceIndex.md#fa)

<a id="f14"></a>
## 14. 帧结束：提交、呈现、历史保存和资源回收

### 14.1 模块架构：帧内资源与帧间资源走不同出口

FrameGraph 临时资源可以在最后使用后交还分配器；下一帧要读的 TAA/SSR 图像必须从图的临时生命周期中分离。Renderer 的历史保存回调将纹理 detach 到当前 View 历史条目，renderJob 结束后提交历史环；旧条目通过受控销毁机制释放。[F03](SourceIndex.md#f03) [F07](SourceIndex.md#f07) [F08](SourceIndex.md#f08)

```cpp
// 图执行时：把需要持续存在的实体资源移交给 View。
historyPass.execute = [&](Resources& R) {
    R.detach(historyTexture, &view.currentHistory.color, &view.currentHistory.desc);
    view.currentHistory.camera = currentStableCamera;
    // TAA 深度等附加资源按功能路径保存。
};
fg.compile().execute(...);
view.commitFrameHistory(engine); // 轮转条目，处理最旧历史
```

普通纹理缓存复用、图资源 last-use 释放、View 的历史环和 Vulkan 在途对象引用是四个不同层次。图内“最后一次 CPU 使用”不能自行证明原生资源已不被 GPU 使用。

固定 TAA 的导出还包括启用 depthDisocclusion 时的 raw depth，并记录同次渲染的 stable projection、jitter 与采样序列位置；FrameHistory 的颜色、深度和元数据不能分别轮转。Renderer 在 renderJob 完成后调用 commitFrameHistory，这属于上游前端时序，不表示 GPU 已完成。Physara 用实际成功提交来发布完整历史代、失败不推进，并以 ticket 保护旧对象，是架构对 RHI 执行模型的采纳合同。[F03](SourceIndex.md#f03) [F08](SourceIndex.md#f08) [F34](SourceIndex.md#f34) [F79](SourceIndex.md#f79)

### 14.2 endFrame 的控制逻辑

```cpp
FRenderer::endFrame() {
    ensureDeferredBeginFrameWorkHasRunIfNeeded();
    swapChain.commit(driver);           // 呈现相关操作进入后端
    finishFrameTimingAndFrameSkipper();
    engine.submitFrame(...);
    driver.endFrame(...);

    driver.tick();                      // 后端周期任务推进
    textureAllocator.gc();              // 纹理分配器的回收策略
    engine.flush();                     // 推进 CPU 命令流
    engine.gc(...);                     // 引擎对象回收相关工作
}
```

这是控制逻辑概要，省略同步模式、性能提示与平台分支。GPU 的原生命令提交也可能因后端 flush、SwapChain 操作或其他条件发生，不能把所有 vkQueueSubmit 都等同于单一 endFrame 调用。[F08](SourceIndex.md#f08) [F72](SourceIndex.md#f72)

| 完成事件 | 能证明什么 | 不能据此证明什么 |
|---|---|---|
| 上传回调执行 | 对应 CPU 输入内存可按回调契约复用 | 整帧所有 GPU 工作完成 |
| FrameGraph 回调返回 | 对应后端调用已经生成 | 图像已被 GPU 渲染 |
| 后端命令提交 | 队列已接收相应工作 | 呈现图像已可见 |
| fence / 受追踪提交完成 | 对应原生工作已完成，可释放相关在途引用 | 用户已经看见显示器上的结果 |
| present 请求 | 交给呈现系统 | 与 CPU 返回瞬间同步显示 |

VulkanCommands 在完成后回收命令缓冲、同步对象和被命令引用的资源。这个阶段正是配套文档进一步拆解的 RHI 问题：记录、提交、完成值、资源保活和原生对象回收必须形成闭环。[F72](SourceIndex.md#f72)

<a id="f15"></a>
## 15. 将一次 Draw 从头追踪到尾

### 15.1 一个对象的完整技术路径

以下跟踪“带法线贴图的 Lit 不透明网格，接收局部光和阴影”的常规路径。可选效果只改变对应节点的输入或分支，不改变整条链的责任边界。

| 步骤 | 进入的数据 | 本层实际处理 | 传给下一层的结果 |
|---|---|---|---|
| 资源创建 | 顶点、索引、纹理、材质包 | Handle 创建、CPU 数据上传、参数绑定 | Geometry、MaterialInstance、Renderable |
| 帧开始 | 脏材质参数 | commit Uniform/Descriptor | 本帧可引用的材质状态 |
| Scene 准备 | Entity、Transform | 世界变换、AABB、SoA 收集 | 连续的候选对象与灯 |
| View 准备 | 相机、Scene SoA | 裁剪、可见分区、阴影计划、Froxel job | 本帧工作集 |
| GPU 参数准备 | 工作集、曝光、相机 | PerView/PerRenderable、Descriptor | UBO 区间和资源绑定 |
| Draw 构造 | 可见 Primitive | 选变体、排序 key、Primitive 索引 | 有序 DrawCommand |
| 图组织 | Draw 范围、效果选项 | 声明阴影/Structure/AO/颜色依赖 | 编译后的活动 Pass |
| 图执行 | 实际 Texture Handle | 补纹理 Descriptor、等待 Froxel、执行 Draw | DriverApi 调用 |
| 后端 | Program、Pipeline、Set、offset、Primitive | 状态缓存、原生命令记录/提交、资源保活 | GPU 可执行命令 |
| 顶点 | buffer、对象矩阵、相机 | 变形、世界/裁剪空间、切线框架 | 插值输入 |
| 片元 | 插值、贴图、材质参数 | prepareMaterial、PixelParams、IBL/逐灯/阴影 | 线性预曝光 HDR |
| 输出 | HDR、深度、历史 | TAA/镜头效果/色彩映射 | RenderTarget / SwapChain |
| 完成 | 在途提交、历史资源 | 呈现、历史轮转、引用回收 | 可复用资源与下一帧输入 |

### 15.2 “渲染管线”和“RHI”在什么位置相接

Filament 的 Executor 已经决定了要画什么、用哪个 Program、绑定哪些数据、写哪个目标。backend 继续解决这些意图怎样变成具体 API 的命令与资源操作。NVRHI 暴露的正是后者这一类能力；它不会替代 Scene/View/Froxel/BRDF。[F09](SourceIndex.md#f09) [F71](SourceIndex.md#f71)

| 已由 Filament 渲染层决定的事项 | backend 接收的执行需求 | 配套 NVRHI 分析的对应概念 |
|---|---|---|
| 可见 Primitive 与绘制顺序 | 顶点/索引、draw 参数 | GraphicsState、drawIndexed |
| Shader 变体与固定状态 | Program、PipelineState | Shader、GraphicsPipeline |
| PerView/PerMaterial/PerRenderable | Set、buffer、texture、动态偏移 | BindingLayout、BindingSet、动态 Uniform |
| 图中某个颜色/深度 Pass | 附件、clear/load/store | FramebufferInfo、dynamic rendering |
| Pass 间读写依赖 | 资源可用状态与同步 | ResourceStates、barrier、queue wait |
| 资源最后 CPU 使用 | 需要继续保留在途对象 | TrackedCommandBuffer、提交版本、GC |

这是职责对照，不是项目间可直接替换的 ABI。Filament Vulkan 当前使用自己的命令流、缓存和 VkRenderPass 路径；NVRHI Vulkan 使用自己的接口和 dynamic rendering。理解它们共同解决的执行问题，再分别沿实际源码阅读，才能把“对象为何被画出”和“这次 Draw 为何能安全执行”连成一条技术线。
