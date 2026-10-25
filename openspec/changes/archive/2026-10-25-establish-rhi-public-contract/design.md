# Design

## Context

动机和工作包范围见 [proposal](proposal.md); 行为合同见 [rhi-public-contract](specs/rhi-public-contract/spec.md). 本设计满足 schema 的条件: 完整公共接口的命名/布局迁移、跨构建边界消费及绑定键正确性需要在编码前确定方法和例外.

目前正式构建只有 Core、Platform 和唯一 Runtime 的第一方实现, `RHI/CMakeLists.txt` 为空. 宿主尚无 RHI Device, 现有生命周期不需为本轮增加字段、工厂或关闭分支. 两个主规格继续有效; 旧输入/Runtime change 的 4.1 真实跨屏验收未完成. [现有前置决定](../../establish-window-input-runtime/design.md#closeout) 允许先完成 D02-T01, 适用理由是本轮仅消费源文件与编译器, 不消费窗口尺寸、UI 缩放或 GPU 结果. 该例外的退出仍是回到旧 change 在真实不同缩放双屏验证, 本轮不扩大例外.

本次已核对五个公共源文件、`src/common/misc.cpp`/`utils.cpp` 的定义归属、[Architecture 7.1](../../../../Docs/Architecture.md#rhi)、[7.2](../../../../Docs/Architecture.md#rhi-objects)、[7.7](../../../../Docs/Architecture.md#capabilities)、[Developments D02](../../../../Docs/Developments.md#dev-d02) 及 [NVRHI 分析](../../../../Docs/Reference/NVRHI_RHI_Vulkan_Analysis.md#n1). 下文记录的是选定方案, 不是已运行的实现证据.

## Goals / Non-Goals

**Goals:** 建立完整公共头的实际消费边界, 使机械迁移可按固定输入复核; 让引用、容量、初始化前提和绑定键的 CPU 行为可独立验证; 清楚区分源谱系、类型布局、可链接实现和后端能力.

**Non-Goals:** 不建立 Common/Validation 或任一 native backend; 不移植 D3D/Vulkan 后端入口、Aftermath 实现或 HLSL 编译工具. 不为现有宿主加 RHI owner, 不制造假 Device/CommandList 或 GPU 测试结论, 不建立全仓库测试/生成框架. 原有可选资源内存查询返回 false 等公共默认行为按来源保留, 不将它们误认为新增后端实现.

## Decisions

### 1. 固定五个输入和一个来源归属

公共输入锁定为 NVRHI `6b96fb03e07539f08327aea76c56d55f1de9d906`, MIT 通知逐文件保留. 下表 SHA256 已在本轮只读核对, 算法是严格 UTF-8 解码, 仅将 CRLF 换为 LF, 再按 UTF-8 字节计算; 不移除 BOM、空白、注释或其他字符. 它是公共移植输入的内容身份, 不是 Git blob id 或 ThirdParty/README 中整个 zip 的归档哈希.

| 上游相对 ThirdParty/nvrhi 的路径 | 第一方输出 | SHA256 (LF) |
|---|---|---|
| `include/nvrhi/nvrhi.h` | `RHI/RHI.hpp` | `6fe801bd3ea0d039798065c80fdefabbcfcc4d30ebf3941dab9696529ffadcf8` |
| `include/nvrhi/common/resource.h` | `RHI/Resource.hpp` | `479a9a61586b4ba460fc936e24a469901ab8731311b421109bb4747c2ed1be30` |
| `include/nvrhi/common/containers.h` | `RHI/Containers.hpp` | `6df5867f3755b3e6456fbdf4dba8c40bc35a0f643622d5cd720263a28f894db4` |
| `include/nvrhi/nvrhiHLSL.h` | `RHI/ShaderTypes.hpp` | `4ac8f5407131320645d2301bfa025b4a66fa5f72a517f1e89453bf107320ef8f` |
| `include/nvrhi/utils.h` | `RHI/Utils.hpp` | `929ddbafcd6d18601745980656b5acced38f3d7a3faf409c95997fdd65a08a93` |

实施开始先重核哈希. 变化时停止对应文件迁移并查清输入差异, 不静默更新预期或从浮动上游替换. `RHI/README.md` 承担交付后的当前来源/路径/改名及差异说明, 本 design 保留当时取舍; 不向 ThirdParty 写补丁, 不在 CMake 加逐文件哈希门禁或自动重写源码. 临时变换/差异脚本保留在本 change 的 build/artifacts 目录, 供本机复核.

**备选:** 只标一个上游目录或只记录整个 zip 的 hash 不能解释具体移植文件; 将完整参考目录加入构建违反既有工程边界. 此处五个输入的清单是局部移植记录, 不扩展成依赖管理系统.

### 2. 按源文件职责保留声明, 补齐真实 include 链

```text
Utils.hpp --> RHI.hpp --> Resource.hpp   --> standard library
                    --> Containers.hpp --> standard library
                    --> ShaderTypes.hpp --> standard integer types (C++ branch)
```

- `Resource.hpp`: Object/ObjectTypes、Resource、RefCountPtr、RefCounter 与 ResourceHandle; 不 include Core 或主 RHI 头, 原有 MemoryRequirements 前置声明继续有效.
- `Containers.hpp`: 源固定容量容器, 只依赖它实际使用的标准库头.
- `ShaderTypes.hpp`: 保留主头引用的 GPU 地址、间接 RT/cluster 数据和来源中的跨语言条件定义; C++ 分支自身引入整数类型. 保留原始位域/静态布局检查, 不把这些声明改成项目业务 Shader ABI, 不宣称 HLSL/Slang/GLSL 已编译验证.
- `RHI.hpp`: 完整资源/格式/状态、descriptor、binding、pipeline、draw、query、Device/CommandList 及扩展声明, 包含上面三个公共头.
- `Utils.hpp`: 完整工具声明及已有头内 ScopedMarker 行为; allocator/工具函数仍按其原来的头内/外部定义边界组织.

公开头只使用模块内头和直接需要的标准头. 特别核对 Resource 的 assert/nullptr/移动工具、Containers 的 initializer_list/迭代/移动、RHI 的 hash/数学/尺寸类型, 不依赖先包含另一个头偶然带入. 各头单独作为翻译单元的第一个 include 编译. 准确直接依赖以实际符号和独立编译共同核对, 不靠批量塞入全部标准头.

**备选:** 将主头拆成新的业务接口族会增加循环 include/前置声明并破坏来源对照; 将共享 GPU 类型删掉会使完整 RT 声明缺失. 大主头按同一公共合同保留, 文件尺寸触发的职责评审在此说明, 不为行数机械拆分.

### 3. 使用可枚举的 token 改名, 不做盲目文本替换

| 类别 | 确定规则 |
|---|---|
| 项目命名空间 | `nvrhi` 到 `Physara::RHI`; `utils`、`rt`、`rt::cluster`、`coopvec` 等嵌套命名空间保留来源名称 |
| 接口类型 | 只对源中实际声明的 `IResource`、`IDevice`、`ICommandList` 等接口建立去 I 的白名单, 更新构造/析构、前置声明、继承、Handle 和参数引用 |
| 类型 | `static_vector` 到 `StaticVector`; 其他既有 PascalCase 类型和 Handle 名保留 |
| 普通函数 | `createTexture` 到 `CreateTexture`, `hash_combine` 到 `HashCombine` 等按符号表改名; 包含 `draw/dispatch/resolveTexture/commitBarriers` 到 `Draw/Dispatch/ResolveTexture/CommitBarriers`, 不限于少数动词前缀; 保留运算符和来源已为 PascalCase 的名字 |
| 数据字段 | descriptor/纯数据字段逐字保留, 包括 `arrayElement`、`trackLiveness`、`registerSpaceIsDescriptorSet` 和 `bindingOffsets` |
| 常量/宏 | `c_MaxX` 到 `MaxX`, 其他 `c_`/`k` 常量逐项映射为 PascalCase; `NVRHI_*` 到 `PHYSARA_RHI_*` |
| 内部成员 | 如 `ptr_` 到 `m_Pointer`, `m_refCount` 到 `m_RefCount`, `current_size` 到 `m_CurrentSize`; 模板参数/内部别名按项目命名显式登记 |
| 标准协议 | 保留 `std::hash`、`operator()`、标准容器 aliases 和下述容器操作名, 仅更新其中引用的项目符号 |

保留容器的 `at/front/back/data/begin/cbegin/end/cend/empty/size/max_size/fill/swap/push_back/pop_back/resize/emplace_back` 以及 `value_type/size_type/iterator` 等标准 aliases. 这是与标准库消费者互操作的受限移植例外, 不提供一套重复的 PascalCase 容器 API. 静态绑定工厂如 `Texture_SRV` 与 ResourceType 同名, 保留来源拼写.

容器的来源模板参数 `_max_elements` 映射为 `Capacity`, 非标准容量常量 `max_elements` 映射为 `MaxElements`, 内部基类别名 `base` 映射为 `Base`; 标准 `size_type` 等 aliases 保留. 这些规则和成员改名都进入同一符号表, 不能依赖一个替换顺序偶然避免名称冲突.

普通源码按 4 空格、Allman 和项目列宽整理; 保留版权与有谱系意义的上游注释, 新适配说明使用中文和 ASCII 标点. 不能为统一注释语言重写整份来源说明. 多行宏保留必要反斜线和逐 token 意义, 不因重排生成错误运算符/位域.

公共 `HeaderVersion` 保留来源值 27, 另提供 `OriginalHeaderVersion=27` 标记来源. 二者不是未来 Physara 库的二进制版本协议. `GraphicsAPI` 保留 `D3D11=0`、`D3D12=1`、`VULKAN=2`, 末尾追加独立 `OpenGL=3`. 当前不新增 GL ObjectTypes、工厂或公共窗口函数; 它们有实际后端消费者时再接入. 现有静态/共享导入导出宏仅按前缀映射保留, 不建立本轮没有消费者的 DLL 配置矩阵.

**备选:** 正则删除所有 I 前缀会误伤其他名称, 全面改写嵌套命名空间和标准容器协议只增加无益差异. 增加老名称兼容 alias 会形成两套第一方入口, 本轮不用. 消费者必须使用唯一映射后的合同.

### 4. 保留 CPU 引用模型与明确的初始化前提

RefCountPtr/RefCounter 保留来源的 intrusive 语义: 新 RefCounter 引用为 1; 从裸指针创建 owning handle 会 AddRef; Attach/Create 接管已有引用; Get/箭头只是借用; Detach 交出 owning reference; Reset/析构 Release. 复制增加引用, 移动转移引用, 最后 Release 才删除 CPU 对象. 不替换为 shared_ptr, 不把借用接口改成自动保活.

用只实现最小 Resource 合同的 CPU probe 检查计数、同/异类型复制移动、自赋值/自移动、空句柄、Attach/Detach、Reset 和最终析构次数; 不制作假 Device/CommandList 来冒充 GPU 验证. `operator&` 具有释放旧引用的来源语义, fixture 在需要对象地址时使用 `std::addressof`, 避免测试自己意外改变引用. 原子引用计数不使 descriptor、资源内容或未来 Device 成为可并发写对象, CPU 对象删除也不等于 GPU 完成.

descriptor 中的资源裸指针仍是来源规定的借用, 拷贝 descriptor 或把它放入 unordered 容器不会增加资源引用. CPU probe 使用的指针在检查期保持存活, 不以伪造地址区分身份. 本轮语义 key 的有效前提仅覆盖类型、初始化和活动载荷, 不代表该 descriptor 已通过未来 Device/Validation 的非空资源、设备归属、范围或 usage 验证.

来源的 `BindingSetItem()` 不初始化字段, `HeapDesc::type` 等字段也没有来源默认值. 将这些记录作为先填充后消费的暂存协议保留: 通过来源工厂/合法 setter 完成语义字段和正确 union 成员后才能比较、哈希或交给消费者. 不测试未初始化值, 不补一个猜测的默认 type 来伪造合法资源. 此项保留是源级默认构造行为的局部例外, 不是允许未初始化读取; 在头与来源说明写清前提, 检查所有本轮 fixture 都先完成初始化.

StaticVector 保留来源对有效尺寸/索引和元素操作的前置条件, 包括其数组存储及 pop/resize 的元素保留/重置行为. assert 继续表达内部合同, 不发明公共错误结果或静默扩容. 外部 descriptor 输入的 Release 校验随 D02-T02 和实际后端入口实现. 容量/迭代检查使用简单无分配元素, 不将元素自己的分配算成容器增长; Release 不故意执行越界/空 back 等合同违例.

实施核对时发现来源 initializer-list 构造未像默认/尺寸构造一样初始化 Base, 而 Base::swap 会遍历全部 Capacity 槽. 维护者于 2026-10-25 明确同意局部修正: 仅在 initializer-list 构造的初始化列表加入 Base(), 避免部分填充的标量/指针数组在合法 swap 时读取未初始化槽. 逻辑内容、布局、容量、pop/resize 合同不变; 与绑定键修正分别登记. 检查部分填充的标量/指针构造与 swap、逻辑迭代和容器自身不分配. 初始化覆盖容量带来实际写入成本, 本轮不作性能结论.

同样保留来源 builder 的合法取值前提, 例如写入 16 位 binding size 前调用方必须证明值可表示. 外部值进入 builder 前就要受检, 后续 Validation 不能恢复已被截断的原值. 本轮临时消费者验证合法边界和静态字段宽度, 不以越界窄化后的结果冒充外部输入验证; 首次业务消费者接入时在其输入边界完成检查.

**备选:** 改用另一套智能指针、std::vector 或自动值初始化所有暂存字段会同时改变公开语义与性能成本, 使本轮失去可核对的来源边界. 本轮不声称这些保留选择比其他实现更快.

### 5. 对绑定键做限定、可验证的正确性修正

源 `BindingSetItem::operator==` 和其 `std::hash` 都遗漏 `arrayElement`; 对设置了 `subresources` 或 `range` 的值, 又通过 union 的 `rawData` 做比较/哈希. `BindingSetDesc` 的相等/哈希仅考虑 bindings, 忽略影响保活责任的 `trackLiveness`. 原始位置可由 [主头](../../../../ThirdParty/nvrhi/include/nvrhi/nvrhi.h) 的 Binding Sets 与末尾 hash 特化核对.

本轮选定修正这些头内 CPU 行为, 公开字段/顺序/大小、工厂和 setter 的有效用途保持不变. 实施第一步在 Architecture 7.1 的受控差异处补记本节的局部例外, 然后才写相关实现. 既有全局扩展清单不扩大为任意重写许可; 只允许以下三个已枚举项, 在 RHI/README 和归一化差异中关联本 change.

| 项目 | 明确变化 | 核对 |
|---|---|---|
| 绑定项公共字段 | 相等/哈希都纳入 `arrayElement`, 继续纳入 resourceHandle/slot/type/dimension/format/overrideComponentMapping | 不同数组元素的 key 可同时保存并正确查找 |
| union 载荷 | 根据合法 ResourceType 读取有效语义字段, 不读取别的活动成员或整对象字节 | 所有工厂类别、范围字段与 reserved 变化 |
| 绑定集合 | 相等/哈希都纳入 `trackLiveness`, bindings 顺序保持有意义 | 同 bindings/不同保活策略不互换 |

比较逻辑固定为: 先比较公共语义字段; type 相同且字段相等才选择下表载荷; 不比较 `unused/unused2` 或隐式 padding. 哈希按相同字段顺序组合, 载荷选择与相等使用同一分类, 不能一个读 range、另一个读 rawData.

| ResourceType | 有效载荷参与 key 的字段 |
|---|---|
| Texture_SRV、Texture_UAV、SamplerFeedbackTexture_UAV | subresources 的 baseMipLevel、numMipLevels、baseArraySlice、numArraySlices |
| Typed/Structured/Raw Buffer SRV/UAV、ConstantBuffer、VolatileConstantBuffer、PushConstants | range.byteOffset、range.byteSize |
| None、Sampler、RayTracingAccelStruct | 无范围载荷, 只使用公共语义字段 |

这些操作的前提是合法类型、完成初始化且活动成员与类型匹配. `SetRange/SetSubresources` 的调用者遵守同一前提; 公共 union 无运行期标签可供实现猜测真正活动成员. Count 或任意非法 type 不是可哈希的绑定键, 不能把它作为已接受的 descriptor. 未来 Validation 负责外部输入范围, 本轮不暗加第二套错误 API.

key 检查验证相等关系及 unordered 容器行为, 不以“不同 key 的 hash 一定不同”为断言. 用固定返回值的测试 hasher 强制碰撞, 确认真实 equality 仍区分 key; equal key 则必须得到相等的真实 hash. reserved 字段变化不能影响结果, 纹理/缓冲范围每个语义字段、component mapping 和数组元素分别变化. 同时保留 BindingSetItem 的 40 字节布局.

**备选:** 完全保留遗漏会让不同数组元素/保活策略被当成同一键, 原始 union 访问也不能成为安全实现依据. 删除比较/hash 或禁止所有数组元素会裁剪公共合同. 本方案只修正语义键, 不增加缓存、解析框架或新的公开类型.

### 6. 使用真实的头文件 target, 不提前实现 Common

`RHI/CMakeLists.txt` 建立 `PhysaraRHI INTERFACE`, 只列五个实际公共头、导出 `${CMAKE_CURRENT_SOURCE_DIR}` 和 `cxx_std_20`. 公共 include 形如 `<RHI.hpp>`、`<Resource.hpp>`, 与当前 Platform 的模块入口方式一致. 头作为第一方代码接受诊断, 不标 SYSTEM. INTERFACE 目标不产生 .lib, 但它拥有实际公共头和消费合同, 不是空功能库.

目标不链接 Core/Platform/第三方参考或 native 库, 不向第三方传播全局 /W4 或 /WX. 独立第一方检查消费者自行按现有政策设置 `/W4 /WX /utf-8 /permissive-` 和禁用语言扩展, 从目标取得 C++20/include. 当前产品不需要链接一个未使用的 RHI 入口. D02-T02 加入真实 Common 源时可将相同 target 变成 STATIC, 保持消费名称/头入口, 本轮不预建空 Common 目录或 dummy cpp.

| 本轮实际可运行 | 本轮只检查声明, 定义留给后续 |
|---|---|
| Resource/RefCountPtr/RefCounter、StaticVector、来源已有头内值操作、修正后的绑定比较/hash | VerifyHeaderVersion、GetFormatInfo、范围 Resolve、FramebufferInfo 的外部构造、CoopVec 工具、CommandList 的外部辅助、Utils 的外部工具/allocator |

声明清单不能只靠 `PHYSARA_RHI_API` 筛选, 因为部分外部工具并无导出标记. 对照 misc.cpp/format-info.cpp/utils.cpp 确认每项的定义归属; 继承而来的头内转发若仍调用外部方法, 也不作为可链接用例执行. 所有 Device 纯虚调用都只在编译时核对签名. 保留原来的空 handle/bool/MessageCallback 形式, 不引入 Core::HostError 或第二套 RHI 错误框架.

**备选:** 提前复制 Common 的部分 cpp 或用 inline 空实现消除链接错误, 会越过本工作包前置并掩盖尚未交付的能力. 本轮不承诺完整公共库可被任意 ODR-use, 正确交付是头内行为可链接, 外部定义归属可查.

### 7. 验证按可观察合同分层, 差异核对覆盖整份输入

所有临时源码、变换规则和原始输出在 `build/artifacts/establish-rhi-public-contract/`, 不成为新的产品或持久测试组织. 使用现有 MSVC/CMake 和可用脚本标准库, 不安装解析器/测试框架. 本轮不需要 RenderDoc、图像或性能报告.

1. **输入和机械差异:** 从固定输入及已记录符号表生成预期 token 序列, 忽略格式/非许可注释差异但不忽略字面量、预处理条件、签名、字段或表达式. 变换脚本识别注释/字符串和标识符边界, 不宣称正则能解析任意 C++. 对导入路径、合法命名、头自包含、新版来源标记、OpenGL 枚举和本节局部修正逐项白名单; 其他 token 差异必须消除或先修订设计. 预期来自上游, 不从目标反推预期. 宏展开的工厂、所有命名空间和结尾 std::hash 都在核对范围, 不是只数 Device 虚函数数量.
2. **独立编译:** 五个头各自作为第一个 include, 不用 PCH; 用一个独立消费者只消费 PhysaraRHI. 对完整声明族/重载/const/noexcept/默认参数做 compile-only 或 unevaluated 检查, 包含所有扩展类型与接口. 对应负例确认旧名字、错误 Handle 转换不可隐式使用. 不真的调用 [[deprecated]] 重载触发预期弃用告警来冒充新增警告失败, 其保留以差异/声明检查证明.
3. **值与布局:** 对明确有默认值的字段和全部既有 enum/常量核对; 保持原有 static_assert, 补核 BindingLayoutItem=8、BindingSetItem=40、TextureSubresourceSet/BufferRange=16、rt::InstanceDesc=64 及共享 indirect 记录. 只对 standard-layout 对象使用 offsetof, 位域采用已初始化对象的已知打包值检查; 不把 std::string/vtable 的尺寸检查称为跨编译器 ABI 保证.
4. **CPU 行为:** probe 验证引用转移与最终析构; 简单元素验证容器 0/部分/满容量及 const/mutable 迭代; 显式分配观察只证明容器/handle 操作自身的存储行为. 绑定键覆盖上节全部类别/字段、保活策略、reserved 和强制碰撞. 全部 Debug/Release 运行, 不执行未初始化/越界输入, 不将 CPU 结果当 native conformance.
5. **构建与隔离:** 新 binary 目录离线构建现有产品和临时消费者的 Debug/Release. 检查实际 compile/include/link 元数据, 确认 PhysaraRHI 仅 INTERFACE, 参考源码/native SDK 不被传递, 第一方警告仍有效, 没有新增产品程序. 运行现有产品 help、非法参数与 hidden poll-limit=3 验证 0/2/0 及正常退出; 原生环境不可用时明确未运行, 不伪造绿色记录.
6. **原生工作流收尾:** 按实际实现和执行结果逐项核对 specs/tasks/design, 再运行 OpenSpec strict、受影响 Markdown 链接/锚点和 git diff --check. 结构、CPU、产品启动证据分别记录, D01 的跨屏未覆盖在整个交付中持续可见.

独立 fixture 的配置、消费者源码、完整编译运行命令、编译器/SDK、source/target hash 和原始输出路径在 tasks 的实施记录补齐. CPU 可重复失败用例不需要 GPU; 产品 smoke 需要真实宿主, 两者分开报告. OpenSpec planning complete 只表明四类规划产物存在, 不代表这些验收已执行.

前期每落地一个头即可用显式模块 include 的临时单头编译/CPU fixture 检查, 无需等待正式 target 或复制下一层实现. 只有第 6 节的 target 建立后, 才以另一个不手写 RHI include 路径的消费者验证 target 传播; 前期检查不能冒充这一独立消费证据.

**备选:** 只 include 一个总头、只比较符号数量或只跑 strict validation, 都不能证明依赖隔离、漏字段、引用和绑定键语义. 同时不引入一套为本次移植服务的长期自动化平台.

## Risks / Trade-offs

- 大头文件和宏使漏改/误改难以目测 -> 固定输入、完整 token 对照、宏/重载检查与独立编译共同验收, 不降低警告或只靠统计数.
- 来源的默认未初始化记录和 assert 容器需要调用前提 -> 在头中写明有效构造/容量前提, 本轮消费者严格遵守, 外部数据校验留给其真实入口; 不对未定义输入承诺结果.
- 局部绑定键修正偏离上游实现 -> 先更新当前架构例外, 单独列入来源差异, 用字段/布局/碰撞检查证明范围, 不改 descriptor 或参考文件.
- 完整声明可能让消费者误以为所有方法可链接/设备已可用 -> 模块说明和 Build 明确 header-only 与外部定义边界, 不提供工厂或 backend caps 假实现.
- 标准容器协议/来源子命名空间与项目常规命名不同 -> 仅保留本文列出的互操作和谱系边界, 不建立第二套别名, 对照声明清单检查例外没有蔓延.
- LF 哈希与后续格式化用途不同 -> 输入哈希只规范化换行, token 核对另行处理格式, 两者不互相替代; 来源不匹配必须显式复核.
- CPU/编译证据无法证明 GPU 生存期或 GL 能力 -> 所有完成声明仅限 D02-T01, D01 与后续 D02/D03 验收原样保留.

## Migration Plan

1. 应用阶段先复核固定输入, 将本文选定的共享类型映射/标准协议保留和绑定键局部例外同步到 Architecture 7.1, 明确仍只推进 D02-T01; 不修改旧 change 的历史决定.
2. 按 Resource、Containers、ShaderTypes、完整 RHI、绑定键修正、Utils 的依赖落地, 每项同时完成适用编译/CPU 检查及来源差异说明, 不等全部复制后才第一次验收.
3. 建立公共 target 与独立消费者, 完成整份声明对照和干净构建/产品 smoke. Docs/Build 只记录可用能力/入口, RHI/README 记录当前来源与差异, Developments 修正过期总览并按证据关联 D02-T01.
4. 原生 verify 核对本 change 全部场景和限制, 所有任务实际通过后才标记工作包已验证. 主规格同步/归档沿后续明确工作流, 旧 change 仍等待真实跨屏证据.
5. 如需回退, 撤销本 change 的 RHI 公共文件、target 与对应当前文档更新即可, 因当前产品没有新的 RHI owner 或外部持久化数据, 无资源/数据迁移. 不借回退删除参考来源、旧 change 或尚未完成的 DPI 要求.
