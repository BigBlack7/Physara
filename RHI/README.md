# RHI 公共合同与来源

本模块按 [Architecture 7.1](../Docs/Architecture.md#rhi) 移植固定 NVRHI 的公共合同. 当前交付边界是五个自包含公共头和 PhysaraRHI INTERFACE 消费入口, 不产生 RHI .lib. 头内值/容器/引用行为可独立使用; 外部函数定义、Common/Validation 和 Device/backend 尚属后续工作包. API 或枚举存在不代表设备能力已实现, 也不保证任意外部方法可以链接.

## 固定输入

来源为 NVIDIA-RTX/NVRHI commit `6b96fb03e07539f08327aea76c56d55f1de9d906`, 原始 HeaderVersion 为 27. MIT 版权及许可保留在每份移植头开头; 完整来源许可见 [参考快照许可](../ThirdParty/nvrhi/LICENSE.txt). 下表路径相对 ThirdParty/nvrhi; SHA256 对 UTF-8 内容仅规范化 CRLF 为 LF, 保留 BOM、空白和注释, 不等于 Git blob 或整个源码 zip 的哈希.

| 来源 | 目标 | SHA256 (LF) |
|---|---|---|
| `include/nvrhi/nvrhi.h` | `RHI.hpp` | `6fe801bd3ea0d039798065c80fdefabbcfcc4d30ebf3941dab9696529ffadcf8` |
| `include/nvrhi/common/resource.h` | `Resource.hpp` | `479a9a61586b4ba460fc936e24a469901ab8731311b421109bb4747c2ed1be30` |
| `include/nvrhi/common/containers.h` | `Containers.hpp` | `6df5867f3755b3e6456fbdf4dba8c40bc35a0f643622d5cd720263a28f894db4` |
| `include/nvrhi/nvrhiHLSL.h` | `ShaderTypes.hpp` | `4ac8f5407131320645d2301bfa025b4a66fa5f72a517f1e89453bf107320ef8f` |
| `include/nvrhi/utils.h` | `Utils.hpp` | `929ddbafcd6d18601745980656b5acced38f3d7a3faf409c95997fdd65a08a93` |

来源输入改变时重新审查映射与全部差异, 不自动接受漂移. ThirdParty/nvrhi 始终仅作参考, 正式 target 不 include 或链接该目录. 复核方法及实施证据见 [本次 change](../openspec/changes/archive/2026-10-25-establish-rhi-public-contract/tasks.md); 不在 CMake 运行移植脚本或哈希门禁.

## 完整改名规则

- 项目根命名空间 `nvrhi` 映射为 `Physara::RHI`, `utils/rt/rt::cluster/coopvec` 等源子命名空间保留.
- 只对源声明的接口类去掉 I: Resource、Heap、Texture、StagingTexture、SamplerFeedbackTexture、InputLayout、Buffer、Shader、ShaderLibrary、Sampler、Framebuffer、OpacityMicromap、AccelStruct、BindingLayout、BindingSet、DescriptorTable、GraphicsPipeline、ComputePipeline、MeshletPipeline、EventQuery、TimerQuery、Pipeline、ShaderTable、MessageCallback、Device、CommandListLifetimeTracker、CommandList. 构造/析构、继承、前置声明、Handle 和全部类型引用同步映射; 不对其他 I 开头标识符做猜测.
- 非标准协议的普通函数将首字母改为大写, snake_case 自由函数 `hash_combine` 改为 `HashCombine`; 包含 CommandList 的 `draw/dispatch/resolveTexture/commitBarriers` 到 `Draw/Dispatch/ResolveTexture/CommitBarriers`, 不限于动词前缀匹配. 来源已为 PascalCase 的名称保留. 根据函数声明/调用位置改名, 同名 `width/height` 数据字段和参数保持原样.
- `static_vector` 改为 `StaticVector`; 其模板参数 `_max_elements` 改为 `Capacity`, 容量常量 `max_elements` 改为 `MaxElements`, 内部别名 `base` 改为 `Base`.
- 标准容器操作 `at/front/back/data/begin/cbegin/end/cend/empty/size/max_size/fill/swap/push_back/pop_back/resize/emplace_back`、标准 aliases、`std::hash` 与运算符保留; 不提供另一套同义 API. 静态工厂如 `Texture_SRV` 保留与上游协议类型对应的拼写.
- `c_` 常量去掉前缀, cluster 的 `k` 前缀常量去掉 k; 枚举项和值保持上游拼写及数值. `NVRHI_*` 宏统一改为 `PHYSARA_RHI_*`, 导入导出条件不扩展为本轮 DLL 构建能力.
- 句柄/容器内部成员 `ptr_/m_refCount/current_size/m_commandList` 分别改为 `m_Pointer/m_RefCount/m_CurrentSize/m_CommandList`; RefCountPtr::Create 的局部 Ptr 改为 ptr. descriptor 字段和原有纯数据记录字段不改名.
- 只替换代码标识符及引用方法名的弃用诊断, 不在字符串/注释里盲替换普通英文词. 许可和来源注释保留, 新适配说明用中文. 格式采用项目的 4 空格、Allman、120 列与控制体大括号; 大主头保持单一来源合同, 规模评审见 design.

## 限定适配与行为前提

1. 五个头补齐直接标准库依赖、改写模块内 include; ShaderTypes 保留跨语言条件并补 C++ 整数头, 本轮只验证 C++20/MSVC profile.
2. `HeaderVersion` 保留 27, `OriginalHeaderVersion=27` 识别来源; GraphicsAPI 末尾追加 `OpenGL=3`, 既有 D3D11/D3D12/VULKAN 仍为 0/1/2. 不声明 NVRHI 二进制 ABI 兼容, 不新增 GL 对象类型或工厂占位.
3. 绑定项相等和哈希纳入 `arrayElement`, 公共字段及按 type 选择的有效 subresources/range 均按语义比较/组合. 不读取不同活动 union 成员、reserved 或 padding. None/Sampler/AccelStruct 无范围载荷参与 key.
4. 绑定集合相等和哈希纳入 `trackLiveness`, bindings 顺序有意义. 相同 key 的 hash 相同; 不同 key 可以碰撞, equality 必须正确区分.
5. 资源裸指针为借用. RefCountPtr 的 copy/retain 增加引用, move/Attach/Create/Detach 按源合同转移, Get/箭头不增引用. 引用仅保护 CPU wrapper, 不证明 GPU 完成, 原子计数不表示资源内容可以并发修改.
6. BindingSetItem 默认构造及 HeapDesc::type 等来源无默认值字段保持暂存前提, 须先通过工厂/赋值完成初始化. key 的 type/活动载荷匹配不等于已通过未来 Device/Validation 的资源、范围或能力验证. 进入窄字段的外部值须在 builder 之前受检.
7. StaticVector 保持合法容量/索引及元素操作前提, 包括原数组槽的保留/重置语义; 不静默扩容, Release 不允许越界调用. 标准协议、源子命名空间和暂存构造的保留只限于本次移植边界.
8. 经维护者确认, initializer-list 构造补 Base() 初始化, 与默认/尺寸构造一致; 防止部分填充的标量/指针容器在整数组 swap 中读未初始化槽. 布局、逻辑内容和容量不变, 不作性能结论.

上述非机械差异的原因、范围、替代方案和验证见 [design](../openspec/changes/archive/2026-10-25-establish-rhi-public-contract/design.md). 不据此普遍允许重写来源算法.

## 外部定义归属

以下声明保留但本轮不提供定义, 不应用成功空操作补齐链接:

| 声明 | 后续来源/归属 |
|---|---|
| VerifyHeaderVersion、TextureSlice/TextureSubresourceSet/BufferRange 的 Resolve、IsEntireTexture | `src/common/misc.cpp`, D02-T02 |
| BlendState/RenderTarget 的 UsesConstantColor、FramebufferInfo/Ex 的外部构造 | `src/common/misc.cpp`, D02-T02 |
| CommandList::SetResourceStatesForFramebuffer、coopvec::GetDataTypeSize/GetOptimalMatrixStride | `src/common/misc.cpp`, D02-T02 |
| GetFormatInfo | `src/common/format-info.cpp`, D02-T02 |
| utils 的自由函数、BitSetAllocator 构造/Allocate/Release | `src/common/utils.cpp`, D02-T02 |
| Device、CommandList 等纯虚方法、AftermathCrashDumpHelper 的实现 | 实际后端/可选能力工作包, 当前无实例 |

ScopedMarker 和 descriptor 的头内转发仍需满足其调用对象/外部定义前提, 不因函数体在头里就保证所有使用方式可链接. 公共头不依赖 Core、Platform、Editor、GLFW、GLAD、ImGui 或 Vulkan SDK. 构建/运行说明集中在 [Build](../Docs/Build.md), 当前工作包状态见 [Developments](../Docs/Developments.md#d02-t01).
