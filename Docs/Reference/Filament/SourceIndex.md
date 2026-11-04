# Filament · 源码索引

[Filament 技术目录](../Filament_Rendering_Pipeline_Analysis.md)

<a id="fa"></a>
## A. 源码索引

Filament 源码链接固定到本文分析的 commit；补充的 Khronos 扩展和 Sample Renderer 分别固定到 `7cee0fcddd5730e90d488bfb5aa2e52171483dde`、`5650e2c8958801445ac0b38937a83984e3a6b278`，核心语义另链接官方规范。行号定位主要入口，相关分支位于同一文件中。正文的代码为结构化伪代码，准确签名、平台条件和异常处理以链接源码为准；示例算法不自动变成规范要求。

| 引用 | 文件 | 阅读主题 |
|---|---|---|
| <a id="f01"></a>[F01] | `README.md` | 公共 API 和主循环 |
| <a id="f02"></a>[F02] | `filament/src/details/Engine.cpp` | Engine、命令线程和资源准备 |
| <a id="f03"></a>[F03] | `filament/src/details/View.cpp` | View 准备、对象 UBO 和历史 |
| <a id="f04"></a>[F04] | `filament/src/details/Scene.cpp` | Scene SoA 和方向光选择 |
| <a id="f05"></a>[F05] | `filament/src/components/TransformManager.cpp` | 变换层级 |
| <a id="f06"></a>[F06] | `filament/src/components/RenderableManager.cpp` | Renderable 组件与资源 |
| <a id="f07"></a>[F07] | `filament/src/fg/FrameGraph.cpp` | 图编译和执行 |
| <a id="f08"></a>[F08] | `filament/src/details/Renderer.cpp` | 完整帧调度与 Pass 顺序 |
| <a id="f09"></a>[F09] | `filament/src/RenderPass.cpp` | 命令生成、排序和 Executor |
| <a id="f10"></a>[F10] | `filament/src/RenderPass.h` | 绘制命令数据结构 |
| <a id="f11"></a>[F11] | `filament/src/Froxelizer.cpp` | Froxel 构造、压缩和上传 |
| <a id="f12"></a>[F12] | `filament/src/details/Camera.cpp` | 投影、裁剪和物理镜头 |
| <a id="f13"></a>[F13] | `filament/src/Exposure.cpp` | 曝光公式与测光换算 |
| <a id="f14"></a>[F14] | `libs/filabridge/include/private/filament/EngineEnums.h` | Descriptor、灯数量和配置 |
| <a id="f15"></a>[F15] | `libs/filabridge/include/private/filament/UibStructs.h` | CPU/GPU UBO 布局 |
| <a id="f16"></a>[F16] | `filament/src/ds/PerViewDescriptorSetUtils.cpp` | PerView 参数填充 |
| <a id="f17"></a>[F17] | `filament/backend/include/private/backend/CommandStream.h` | CPU 后端命令流 |
| <a id="f18"></a>[F18] | `filament/backend/include/private/backend/DriverAPI.inc` | Driver API 声明 |
| <a id="f19"></a>[F19] | `filament/src/details/Material.cpp` | 材质包、程序与布局 |
| <a id="f20"></a>[F20] | `filament/src/DynamicSpecConstKey.h` | 动态特殊化键 |
| <a id="f21"></a>[F21] | `libs/filamat/src/MaterialBuilder.cpp` | 材质编译流程 |
| <a id="f22"></a>[F22] | `libs/filabridge/include/filament/MaterialEnums.h` | 材质域、混合与 37 个属性 |
| <a id="f23"></a>[F23] | `libs/filamat/src/shaders/ShaderGenerator.cpp` | Shader 生成与默认入口 |
| <a id="f24"></a>[F24] | `shaders/src/surface_main.fs` | 片元主入口 |
| <a id="f25"></a>[F25] | `shaders/src/surface_shading_parameters.fs` | 几何/材质法线准备 |
| <a id="f26"></a>[F26] | `shaders/src/surface_material_inputs.fs` | 材质字段、默认值与 API gate |
| <a id="f27"></a>[F27] | `shaders/src/surface_main.vs` | 顶点、实例和坐标补偿 |
| <a id="f28"></a>[F28] | `shaders/src/surface_depth_main.fs` | 深度与 moments Shader |
| <a id="f29"></a>[F29] | `shaders/src/surface_shading_lit.fs` | PixelParams、Specular AA、光照聚合 |
| <a id="f30"></a>[F30] | `shaders/src/surface_light_directional.fs` | 方向光和额外方向光 |
| <a id="f31"></a>[F31] | `filament/src/ShadowMapManager.cpp` | 阴影组织和矩 Mip 生成 |
| <a id="f32"></a>[F32] | `shaders/src/surface_shadowing.fs` | PCF/EVSM/EVSSM |
| <a id="f33"></a>[F33] | `filament/include/filament/Options.h` | 阴影、AO、TAA 等配置 |
| <a id="f34"></a>[F34] | `filament/src/PostProcessManager.cpp` | 结构、反射与后处理调度 |
| <a id="f35"></a>[F35] | `filament/src/materials/ssao/gtaoImpl.fs` | GTAO 求值 |
| <a id="f36"></a>[F36] | `shaders/src/surface_light_indirect.fs` | IBL、AO、折射和反射融合 |
| <a id="f37"></a>[F37] | `shaders/src/surface_material.fs` | 粗糙度和参数转换 |
| <a id="f38"></a>[F38] | `shaders/src/surface_brdf.fs` | BRDF 与薄膜干涉基础 |
| <a id="f39"></a>[F39] | `filament/src/DFG.cpp` | DFG LUT 加载和格式 |
| <a id="f42"></a>[F42] | `filament/src/details/VertexBuffer.cpp` | 顶点资源与数据上传 |
| <a id="f43"></a>[F43] | `filament/src/details/IndexBuffer.cpp` | 索引资源与上传 |
| <a id="f44"></a>[F44] | `libs/gltfio/src/AssetLoader.cpp` | 资产构建 |
| <a id="f45"></a>[F45] | `libs/gltfio/src/ResourceLoader.cpp` | 资产资源加载 |
| <a id="f46"></a>[F46] | `filament/src/details/Texture.cpp` | 纹理存储和上传 |
| <a id="f47"></a>[F47] | `filament/src/details/MaterialInstance.cpp` | 实例脏更新、UBO batching |
| <a id="f48"></a>[F48] | `shaders/src/surface_shading_lit_custom.fs` | 自定义表面着色 |
| <a id="f49"></a>[F49] | `filament/src/TextureCache.cpp` | 临时纹理缓存 |
| <a id="f50"></a>[F50] | `filament/src/fg/details/ResourceAllocator.h` | FrameGraph 资源构造/释放 |
| <a id="f51"></a>[F51] | `shaders/src/surface_shading_model_cloth.fs` | Cloth |
| <a id="f52"></a>[F52] | `shaders/src/surface_shading_model_subsurface.fs` | Subsurface |
| <a id="f53"></a>[F53] | `shaders/src/surface_shading_unlit.fs` | Unlit 与自发光 |
| <a id="f54"></a>[F54] | `filament/src/components/LightManager.cpp` | 灯单位与强度换算 |
| <a id="f56"></a>[F56] | `shaders/src/surface_light_punctual.fs` | 局部灯、衰减、Froxel 查询 |
| <a id="f57"></a>[F57] | `libs/ibl/src/CubemapIBL.cpp` | 环境预过滤与 DFG 积分编码 |
| <a id="f58"></a>[F58] | `filament/src/details/IndirectLight.cpp` | 间接光和 SH 转换 |
| <a id="f59"></a>[F59] | `shaders/src/surface_ambient_occlusion.fs` | AO、Bent Normal 与多反弹近似 |
| <a id="f60"></a>[F60] | `filament/src/RendererUtils.cpp` | Color/Refraction Pass 拆分 |
| <a id="f61"></a>[F61] | `shaders/src/surface_light_reflections.fs` | 屏幕空间射线追踪 |
| <a id="f62"></a>[F62] | `filament/src/materials/skybox.mat` | Skybox 绘制 |
| <a id="f63"></a>[F63] | `shaders/src/surface_fog.fs` | 雾密度积分与合成 |
| <a id="f64"></a>[F64] | `filament/src/materials/antiAliasing/taa/taa.mat` | TAA/深度历史与选项 |
| <a id="f65"></a>[F65] | `filament/src/materials/dof/dof.mat` | 散景求值 |
| <a id="f66"></a>[F66] | `filament/src/materials/bloom/bloomDownsample.mat` | Bloom 下采样 |
| <a id="f67"></a>[F67] | `filament/src/details/ColorGrading.cpp` | CPU 色彩变换与 LUT |
| <a id="f68"></a>[F68] | `filament/src/ToneMapper.cpp` | 色调映射器 |
| <a id="f69"></a>[F69] | `filament/src/components/LightManager.h` | IES 支持边界 |
| <a id="f70"></a>[F70] | `libs/filamat/src/sca/GLSLTools.cpp` | 材质语义验证 |
| <a id="f71"></a>[F71] | `filament/backend/src/vulkan/VulkanDriver.cpp` | Filament Vulkan Pipeline、上传与原生 Draw |
| <a id="f72"></a>[F72] | `filament/backend/src/vulkan/VulkanCommands.cpp` | Filament Vulkan 提交、完成与引用回收 |
| <a id="f73"></a>[F73] | `shaders/src/surface_shading_model_standard.fs` | 标准 BRDF、各向异性、sheen、清漆与第二波瓣 |
| <a id="f74"></a>[F74] | `filament/src/Froxelizer.h` | Froxel 数据编码与记录范围 |
| <a id="f75"></a>[F75] | `filament/src/ShadowMap.cpp` | 阴影相机聚焦、稳定化、LiSPSM 与纹理映射 |
| <a id="f76"></a>[F76] | `filament/src/details/SkinningBuffer.cpp` | 骨骼变换打包与上传 |
| <a id="f77"></a>[F77] | `filament/src/details/MorphTargetBuffer.cpp` | Morph target 纹理数据上传 |
| <a id="f78"></a>[F78] | `filament/src/materials/antiAliasing/taa/taa.md` | TAA 重建、裁剪、jitter 栅格和深度拒绝的源码配套说明 |
| <a id="f79"></a>[F79] | `filament/src/FrameHistory.h` | TAA 颜色/深度/矩阵/jitter 与 SSR 历史角色 |
| [G01] | Khronos `KHR_materials_specular` | 标准字段、通道、颜色域与 F0/F90 |
| [G02] | Khronos `KHR_materials_ior` | 普通 IOR 和零值兼容模式 |
| [G03] | Khronos `KHR_materials_anisotropy` | 标准方向/强度与两个方向粗糙度 |
| [G04] | Khronos `KHR_materials_clearcoat` | 层、法线与 emission 的关系 |
| [G05] | Khronos `KHR_materials_sheen` | Charlie、albedo scaling 与性能近似 |
| [G06] | Khronos `KHR_materials_transmission` | BTDF、资产语义与非规范性实现说明 |
| [G07] | Khronos `KHR_materials_volume` | mesh-space 厚度和 world-space 吸收距离 |
| [G08] | Khronos Sample Renderer `punctual.glsl` | 逐灯透射与实例厚度变换示例 |
| [G09] | Khronos glTF 2.0 官方规范 | 核心材质缺省值和纹理语义 |
| [G10] | Khronos `KHR_materials_dispersion` | Abbe 字段与非规范性 RGB 三采样示例 |
| [G11] | Khronos `KHR_materials_iridescence` | 薄膜强度、厚度纹理、单位与 IOR |

[F01]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/README.md#L177
[F02]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Engine.cpp#L774
[F03]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/View.cpp#L717
[F04]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Scene.cpp#L80
[F05]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/components/TransformManager.cpp#L229
[F06]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/components/RenderableManager.cpp#L103
[F07]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/fg/FrameGraph.cpp#L144
[F08]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Renderer.cpp#L771
[F09]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/RenderPass.cpp#L186
[F10]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/RenderPass.h#L62
[F11]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/Froxelizer.cpp#L731
[F12]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Camera.cpp#L184
[F13]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/Exposure.cpp#L26
[F14]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filabridge/include/private/filament/EngineEnums.h#L39
[F15]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filabridge/include/private/filament/UibStructs.h#L72
[F16]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/ds/PerViewDescriptorSetUtils.cpp#L41
[F17]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/backend/include/private/backend/CommandStream.h#L196
[F18]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/backend/include/private/backend/DriverAPI.inc#L214
[F19]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Material.cpp#L100
[F20]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/DynamicSpecConstKey.h#L31
[F21]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filamat/src/MaterialBuilder.cpp#L1298
[F22]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filabridge/include/filament/MaterialEnums.h#L255
[F23]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filamat/src/shaders/ShaderGenerator.cpp#L340
[F24]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_main.fs#L40
[F25]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_parameters.fs#L59
[F26]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_material_inputs.fs#L20
[F27]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_main.vs#L9
[F28]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_depth_main.fs#L21
[F29]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_lit.fs#L295
[F30]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_directional.fs#L106
[F31]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/ShadowMapManager.cpp#L75
[F32]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shadowing.fs#L210
[F33]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/include/filament/Options.h#L731
[F34]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/PostProcessManager.cpp#L747
[F35]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/ssao/gtaoImpl.fs#L123
[F36]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_indirect.fs#L717
[F37]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_material.fs#L2
[F38]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_brdf.fs#L54
[F39]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/DFG.cpp#L41
[F42]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/VertexBuffer.cpp#L263
[F43]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/IndexBuffer.cpp#L102
[F44]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/gltfio/src/AssetLoader.cpp#L371
[F45]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/gltfio/src/ResourceLoader.cpp#L82
[F46]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/Texture.cpp#L109
[F47]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/MaterialInstance.cpp#L220
[F48]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_lit_custom.fs#L18
[F49]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/TextureCache.cpp#L59
[F50]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/fg/details/ResourceAllocator.h#L80
[F51]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_model_cloth.fs#L12
[F52]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_model_subsurface.fs#L8
[F53]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_unlit.fs#L38
[F54]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/components/LightManager.cpp#L349
[F56]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_punctual.fs#L126
[F57]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/ibl/src/CubemapIBL.cpp#L789
[F58]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/IndirectLight.cpp#L45
[F59]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_ambient_occlusion.fs#L142
[F60]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/RendererUtils.cpp#L312
[F61]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_light_reflections.fs#L60
[F62]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/skybox.mat#L1
[F63]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_fog.fs#L8
[F64]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/antiAliasing/taa/taa.mat#L35
[F65]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/dof/dof.mat#L1
[F66]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/bloom/bloomDownsample.mat#L1
[F67]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/ColorGrading.cpp#L743
[F68]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/ToneMapper.cpp#L694
[F69]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/components/LightManager.h#L173
[F70]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/libs/filamat/src/sca/GLSLTools.cpp#L316
[F71]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/backend/src/vulkan/VulkanDriver.cpp#L2758
[F72]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/backend/src/vulkan/VulkanCommands.cpp#L418
[F73]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/shaders/src/surface_shading_model_standard.fs#L118
[F74]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/Froxelizer.h#L152
[F75]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/ShadowMap.cpp#L128
[F76]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/SkinningBuffer.cpp#L175
[F77]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/details/MorphTargetBuffer.cpp#L183
[F78]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/materials/antiAliasing/taa/taa.md
[F79]: https://github.com/google/filament/blob/b4dfdb073d5543d8569d37e0ee26f79cf794db14/filament/src/FrameHistory.h#L33
[G01]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_specular/README.md
[G02]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_ior/README.md
[G03]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_anisotropy/README.md
[G04]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_clearcoat/README.md
[G05]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_sheen/README.md
[G06]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_transmission/README.md
[G07]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_volume/README.md
[G08]: https://github.com/KhronosGroup/glTF-Sample-Renderer/blob/5650e2c8958801445ac0b38937a83984e3a6b278/source/Renderer/shaders/punctual.glsl#L82
[G09]: https://registry.khronos.org/glTF/specs/2.0/glTF-2.0.html#reference-material-pbrmetallicroughness
[G10]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_dispersion/README.md
[G11]: https://github.com/KhronosGroup/glTF/blob/7cee0fcddd5730e90d488bfb5aa2e52171483dde/extensions/2.0/Khronos/KHR_materials_iridescence/README.md
