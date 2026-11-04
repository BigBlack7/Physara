# NVRHI · 源码索引

[NVRHI 技术目录](../NVRHI_RHI_Vulkan_Analysis.md)

<a id="na"></a>
## A. 源码索引

所有链接固定到本文分析的 commit；行号定位主要入口，相关分支位于同一文件中。正文的代码为结构化伪代码，准确签名、平台条件和异常处理以链接源码为准。

| 引用 | 文件 | 阅读主题 |
|---|---|---|
| <a id="n01"></a>[N01] | `README.md` | 支持后端与特性 |
| <a id="n02"></a>[N02] | `doc/ProgrammingGuide.md` | 编程模型与边界 |
| <a id="n03"></a>[N03] | `include/nvrhi/vulkan.h` | Vulkan 后端入口 |
| <a id="n04"></a>[N04] | `include/nvrhi/nvrhi.h` | 公共对象、描述、命令与能力 |
| <a id="n05"></a>[N05] | `include/nvrhi/common/resource.h` | 引用计数实现 |
| <a id="n06"></a>[N06] | `src/vulkan/vulkan-commandlist.cpp` | 命令录制实例与提交通知 |
| <a id="n07"></a>[N07] | `src/vulkan/vulkan-queue.cpp` | 队列、Timeline、pool 和回收 |
| <a id="n08"></a>[N08] | `include/nvrhi/nvrhi.h` | Layout/Set/VCB/Push ABI |
| <a id="n09"></a>[N09] | `src/vulkan/vulkan-resource-bindings.cpp` | Descriptor、Bindless 和动态偏移 |
| <a id="n10"></a>[N10] | `src/vulkan/vulkan-graphics.cpp` | Framebuffer、dynamic rendering 与图形 Pipeline |
| <a id="n11"></a>[N11] | `src/common/state-tracking.cpp` | 通用状态追踪 |
| <a id="n12"></a>[N12] | `src/vulkan/vulkan-state-tracking.cpp` | Barrier2 与状态要求 |
| <a id="n13"></a>[N13] | `src/common/utils.cpp` | 显式 UAV barrier helper |
| <a id="n14"></a>[N14] | `src/vulkan/vulkan-buffer.cpp` | Buffer、VCB、map/write/copy |
| <a id="n15"></a>[N15] | `src/vulkan/vulkan-upload.cpp` | 上传和 scratch chunk 分配 |
| <a id="n16"></a>[N16] | `src/common/versioning.h` | pending/submitted 编码 |
| <a id="n17"></a>[N17] | `src/vulkan/vulkan-staging-texture.cpp` | Texture staging 与 footprint |
| <a id="n18"></a>[N18] | `src/vulkan/vulkan-queries.cpp` | Event/Timer Query |
| <a id="n19"></a>[N19] | `src/validation/validation-device.cpp` | 验证层 |
| <a id="n20"></a>[N20] | `src/vulkan/vulkan-device.cpp` | 设备、能力和扩展 |
| <a id="n21"></a>[N21] | `src/vulkan/vulkan-allocator.cpp` | 默认内存分配 |
| <a id="n22"></a>[N22] | `src/vulkan/vulkan-texture.cpp` | Texture/ImageView/上传 |
| <a id="n23"></a>[N23] | `doc/memory-queries.md` | 内存与 TLAS 查询支持矩阵 |
| <a id="n24"></a>[N24] | `src/vulkan/vulkan-constants.cpp` | 状态/阶段/access/layout 转换 |
| <a id="n25"></a>[N25] | `src/vulkan/vulkan-shader.cpp` | SPIR-V 与 specialization |
| <a id="n26"></a>[N26] | `src/vulkan/vulkan-compute.cpp` | Compute Pipeline 和 Dispatch |
| <a id="n27"></a>[N27] | `src/vulkan/vulkan-meshlets.cpp` | Mesh Shader 后端 |
| <a id="n28"></a>[N28] | `src/vulkan/vulkan-raytracing.cpp` | AS、RT、SBT 和高级几何 |
| <a id="n29"></a>[N29] | `include/nvrhi/nvrhi.h` | 高级功能公共接口 |
| <a id="n30"></a>[N30] | `src/vulkan/vulkan-backend.h` | Vulkan 内部类、引用、Queue 与命令列表数据结构 |
| [VK01] | Vulkan 规范：vkFlushMappedMemoryRanges | 非 coherent CPU 写入与 atom 范围 |
| [VK02] | Vulkan 规范：vkInvalidateMappedMemoryRanges | GPU 到 Host 的依赖、完成与缓存可见性 |
| [VK03] | Vulkan 规范：vkCmdCopyBuffer | 复制必须位于 render pass instance 外 |

[N01]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/README.md#L5
[N02]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/doc/ProgrammingGuide.md#L7
[N03]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/include/nvrhi/vulkan.h#L50
[N04]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/include/nvrhi/nvrhi.h#L154
[N05]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/include/nvrhi/common/resource.h#L135
[N06]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-commandlist.cpp#L61
[N07]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-queue.cpp#L122
[N08]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/include/nvrhi/nvrhi.h#L2068
[N09]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-resource-bindings.cpp#L316
[N10]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-graphics.cpp#L492
[N11]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/common/state-tracking.cpp#L163
[N12]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-state-tracking.cpp#L214
[N13]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/common/utils.cpp#L149
[N14]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-buffer.cpp#L271
[N15]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-upload.cpp#L64
[N16]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/common/versioning.h#L36
[N17]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-staging-texture.cpp#L48
[N18]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-queries.cpp#L35
[N19]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/validation/validation-device.cpp#L662
[N20]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-device.cpp#L349
[N21]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-allocator.cpp#L92
[N22]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-texture.cpp#L250
[N23]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/doc/memory-queries.md#L1
[N24]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-constants.cpp#L320
[N25]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-shader.cpp#L29
[N26]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-compute.cpp#L112
[N27]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-meshlets.cpp#L28
[N28]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-raytracing.cpp#L341
[N29]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/include/nvrhi/nvrhi.h#L3038
[N30]: https://github.com/NVIDIA-RTX/NVRHI/blob/6b96fb03e07539f08327aea76c56d55f1de9d906/src/vulkan/vulkan-backend.h#L195
[VK01]: https://docs.vulkan.org/refpages/latest/refpages/source/vkFlushMappedMemoryRanges.html
[VK02]: https://docs.vulkan.org/refpages/latest/refpages/source/vkInvalidateMappedMemoryRanges.html
[VK03]: https://docs.vulkan.org/refpages/latest/refpages/source/vkCmdCopyBuffer.html
