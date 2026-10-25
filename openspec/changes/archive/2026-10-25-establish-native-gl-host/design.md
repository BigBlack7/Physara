# Design

## Context

动机与工作包范围见 [proposal.md](proposal.md), 行为合同见 [native-window-host](specs/native-window-host/spec.md). 变更起点的 `Engine/Platform/RHI/Editor/Runtime` 模块 CMakeLists 与 `Runtime/Main.cpp` 为空; 固定依赖已提供 GLFW 3.5.1、GLAD 2.0.8 的 GL 4.6 Core loader 和 spdlog 1.17.0. 本方案跨 Core 与 Platform 并涉及进程级原生状态, 在实施前固定所有权和失败协议.

对照依据为 Architecture 的 [依赖与约定](../../../../Docs/Architecture.md#conventions)、[Core](../../../../Docs/Architecture.md#core)、[宿主启动](../../../../Docs/Architecture.md#platform-start)、[关闭](../../../../Docs/Architecture.md#shutdown), 以及 Developments 的 [D01-T01/T02](../../../../Docs/Developments.md#d01-t01). 架构中的 `CreateRhiDeviceFromCurrentContext` 属于完整启动流程; 开发计划明确将它放到 D02, 因此本增量完成 context 后就结束启动准备.

## Goals / Non-Goals

**Goals:** 只有完整有效的宿主才能离开工厂; 失败结果自持必要上下文; 同一 owner 负责进入、回滚和关闭; 检查代码与未来产品共用实际原生实现.

**Non-Goals:** 提前创建 RHI 空对象、输入快照、PresentColor、帧槽、作业/arena/watcher 或 Runtime 主循环. 本方案只核实创建期事实, 不证明渲染能力、颜色输出或 GPU 完成协议.

## Decisions

### 1. 状态按真正的 owner 归属

| 位置 | 本轮职责 | 所有权与边界 |
|---|---|---|
| `Engine/Core/Identity.hpp` | 最小类型化 index/generation 值及受检递增 | 不提供资源 registry, 不知道 Window 或 RHI |
| `Engine/Core/Error.hpp` | 有界自持错误上下文 | 原生 code 用数值记录, 不 include GLFW; 领域错误枚举由使用方定义 |
| `Engine/Core/Diagnostics.hpp/.cpp` | 有限日志、秒时间戳和丢失计数 | 显式 owner, 内部使用已有 spdlog, 公开头不暴露其类型 |
| `Platform/DeviceHost.hpp/.cpp` | 工厂、宿主身份、线程/唯一性、状态和退出 | 唯一公开有状态平台入口 |
| `Platform/DeviceHostInternal.hpp` | cpp 共享的私有 Impl 与已取得资源 | 隔离 GLFW/GLAD, 不提供通用服务上下文 |
| `Platform/GlfwWindow.cpp` | GLFW 初始化、窗口和原生错误捕获 | 本轮设置稳定 user pointer, 输入回调随 T03 增加 |
| `Platform/OpenGLDeviceHost.cpp` | current、入口加载、环境查询与 swap interval 请求 | 本轮不创建 RHI Device 或发出 Draw |
| `Platform/GlfwAccess.hpp` | 受限的类型化窗口借用 | 本轮 native fixture 使用, 后续 Editor 私有适配复用 |

`DeviceHost` 不可复制/移动, 工厂返回独占 owner. Impl 使用 Pimpl 的理由是隔离原生头和保持 GLFW user pointer 地址稳定; 它承担真实状态, 不是另一层 Application/IWindow/IInput. `Diagnostics` 由调用者先创建并活到 host 关闭之后, host 只借用它. 临时 fixture 是本轮装配者, 之后 Runtime 按相同顺序装配.

环境信息返回自持的值快照, 不暴露内部可变容器或原生字符串. `GlfwAccess` 借用仅在 Ready、owner 线程及当前 owner 生存期内有效; 不能缓存到关闭之后, 也不能传入 Engine. 不引入 shared_ptr 来代替主要 owner.

**备选:** 多个窗口/输入接口会分散当前唯一状态; 公共头直接包含 GLFW/GLAD 会扩大原生边界. 两者都没有本轮收益.

### 2. C++20 结果和 Core 保持最小

- 使用具体的标准库结果组合: 创建结果为 `std::variant<std::unique_ptr<DeviceHost>, HostError>`, 非空 owner 是成功不变量; 显式关闭用 `std::optional<HostError>`, 无错误表示成功. 这两个别名只属于 Platform, 不建设 Result 类继承体系, 不使用 C++23 `std::expected`.
- `HostError` 包含领域原因、阶段、可空 attempt identity 和 Core 错误上下文. 上下文保存原生数值 code、源位置及最多 2048 字节自持描述; 无原生 code 时显式表示缺失, 不把 0 猜成某个 GLFW 错误. `std::source_location` 的编译期源位置不接受临时字符串指针.
- 原因至少区分 InvalidConfiguration、AlreadyActive、WrongThread、IdentityExhausted、OutOfMemory、GlfwInitialization、WindowCreation、ContextActivation、EntryLoading、UnsupportedContext、EnvironmentQuery、SwapInterval 和 Cleanup. 原因不从日志字符串推断; GL error code 与 GLFW error code 标记各自来源.
- 使用 `std::chrono::steady_clock` 和 `std::chrono::duration<double>` 表达 elapsed seconds. Diagnostics 持有自己的时间起点, 创建阶段只保存必要时间点; 不为 `now()` 建立转发类或引入 GLFW 计时依赖. 等待计量/帧 dt 在实际消费者出现时加入.
- `Identity<Tag>` 使用 `uint32_t index` 与 `uint64_t generation`, 仅支持同域相等与有效性检查, 不允许跨域隐式转换. `WindowId` 由 Platform 定义标签. 当前只有一个逻辑槽 index=0, generation 从 1 起, 0 表示无效; 递增前检查溢出, 不创建通用 slot 分配器.

失败描述和 generation 检查不依赖动态分配, 保证工厂遇到 `bad_alloc` 时仍能返回失败. 库构造/冷路径分配的标准异常在所属边界转换; C callback 与析构不允许异常外逸. 未来 RHI 继续使用其上游空 handle/bool/MessageCallback 合同, 不反向依赖 Core 错误类型.

**备选:** 仅返回 null 无法定位阶段; bool+可变 lastError 会引入可被后续操作覆盖的派生状态. 标准库结果组合足够表达本轮的成功/失败.

### 3. 显式约束进程唯一性与线程

调用方必须在进程 `main` 所在线程调用第一次创建; 记录创建线程用于之后的检查, 不把“第一次调用来自哪个线程”当作操作系统主线程发现算法. 所有 GLFW/context 创建、使用、退出均在该线程. 自持快照可以独立传递, 活着的 host 不并发访问.

DeviceHost 是本进程 GLFW 生命周期的唯一 owner; 其它模块/库不能独立 init/terminate 或接管 context. fixture 可以在创建前设置 sentinel callback 和受控 init hints, 但不另行初始化 GLFW. 上游没有公开的“是否已初始化”查询, 不能把重复 glfwInit 返回 true 当成检测并接管外部 owner 的机制.

Platform 内只有一个进程级 GLFW 状态记录: 唯一占用、下一个 generation 和错误捕获目标. 它的存在来自 GLFW/GLAD 的进程全局状态, 不承载任意服务. 一个小互斥量保护占用/目标指针与错误记录拷贝; 持锁期间不调用 GLFW、GL、日志或未知回调. GLAD 加载及所有原生操作仍由唯一 owner 线程执行, 互斥量不用于允许多线程 GL.

工厂在调用任何 GLFW 函数之前取得占用, 同时分配 attempt generation. 第二次创建直接失败, 不能覆盖错误回调、window hints、current context 或 GLAD 全局函数表. 失败的 attempt 也消耗 generation, 已分配身份永不重新发出. 只有持有当前占用的 owner 才能释放它.

显式 `Shutdown` 先检查线程; WrongThread 返回时保留原 owner. owner 线程上的重复关闭是空成功, 不触碰全局状态. 析构在正常主线程走同一幂等路径; 错误线程析构属于调用方违反寿命合同, Release 也终止而不在错误线程调用原生删除. 正常 fixture 只跨线程调用可返回错误的显式操作, 不移动最终 owner 到 worker 析构; 析构违例仅在独立隔离进程中执行负例, 检查终止而非继续使用该进程.

**备选:** GLFW 不提供每个对象独立的初始化引用计数, 不能用多个 Window 析构分别 terminate; 一个全局服务定位器也不能替代这项窄占用协议.

### 4. 创建按依赖推进, Ready 最后发布

| 顺序 | 操作与校验 | 失败时已取得状态 |
|---|---|---|
| 1 | 校验 logical 宽高均大于零且不超过 `int` 上限; title 无内嵌 NUL; 自持必要启动输入 | 无原生状态 |
| 2 | 分配稳定 owner/Impl, 取得唯一占用和 attempt identity, 开始秒计时 | CPU owner / 占用 |
| 3 | 安装 GLFW 错误适配器并保存之前的 callback; 调用 `glfwInit` | 回调; 成功后另有 GLFW 初始化 |
| 4 | `glfwDefaultWindowHints`, 请求 OpenGL API、4.6、Core、双缓冲、sRGB preference、visible 与独立 debug 设置; 创建窗口, 设置稳定 user pointer | GLFW / window+context |
| 5 | `glfwMakeContextCurrent`, 结合本次 GLFW 错误和 `glfwGetCurrentContext` 核对绑定 | current context |
| 6 | `gladLoadGLUserPtr` 使用本次 current context 的 `glfwGetProcAddress` resolver, 以 user pointer 借用当前 Impl; 检查返回版本及本轮必要入口 | 当前代 GLAD 表 |
| 7 | 查询实际版本/profile、默认 framebuffer 与基础 limits, 拷贝字符串并检查 GL error | 自持环境候选 |
| 8 | 在 current context 上请求 `glfwSwapInterval(0/1)`, 封存耗时和环境快照, 返回 Ready owner | 完整可用宿主 |

每一步都先确认其前置成立, 失败即停止推进. 资源所有权由实际取得标记/RAII 保存, 当前阶段只用于错误定位, 不用一个枚举假定所有资源均已存在. `glfwInit` 返回 false 时上游已清理初始化片段, 不把它记录成已初始化; 平台仍负责恢复自己安装的 callback 和占用.

GLAD 返回非零或 `GLAD_GL_VERSION_4_6` 本身不能证明本轮所需函数全部非空. 本轮显式检查 `glGetString/glGetIntegerv/glGetBooleanv/glGetError/glGetFramebufferAttachmentParameteriv` 等实际调用入口; GLAD 自身依赖的扩展枚举入口按固定源码的加载结果处理. 后续 RHI 在首次使用其入口族时补齐检查, 不因此宣称所有 backend 能力已实现.

默认采用已有 debug/release 的正常构建, `debugContext` 为独立启动配置, 本轮默认 false. 可见和隐藏只改变 visibility, 不用 null/OSMesa context 冒充真实 WGL 路径. fixture 对可见窗口的短检查仍在主线程处理必要原生事件, 不形成第二个生产主循环.

**备选:** 创建时只相信 hints 会漏掉实际 context/profile/编码; 启动即实现 RHI 工厂则超出 D01 的前置链.

### 5. 环境观测有明确来源与单位

`HostInfo` 至少自持以下数据; 字段名表达单位, 不含 native enum/pointer:

| 数据 | 查询/来源与验证 |
|---|---|
| WindowId、GLFW 实际版本、平台标识 | 本次 attempt 与 GLFW 版本/平台查询; 当前受支持原生平台为 Win32 |
| GL major/minor、Core profile、context flags | `GL_MAJOR_VERSION/GL_MINOR_VERSION/GL_CONTEXT_PROFILE_MASK/GL_CONTEXT_FLAGS`; 数值版本必须 >=4.6 且包含 Core 位 |
| vendor、renderer、version、GLSL version | 拷贝 `glGetString` 结果; 每项最多 2048 字节, 截断明确标记; 空必要字符串导致查询失败 |
| maxTextureSizePixels、maxRenderbufferSizePixels | 实际 `GL_MAX_TEXTURE_SIZE/GL_MAX_RENDERBUFFER_SIZE` |
| maxUniformBufferBindings、maxStorageBufferBindings、maxCombinedTextureUnits、uniformBufferOffsetAlignmentBytes | 对应 GL limits; 先以 native 有符号类型接收, 检查正值/表示范围后转换 |
| doubleBuffered、defaultFramebufferEncoding | 实际 `GL_DOUBLEBUFFER`; 确认默认 draw FBO 为 0, 查询 `GL_BACK_LEFT` 的 `GL_FRAMEBUFFER_ATTACHMENT_COLOR_ENCODING` |
| requestedSwapInterval、creationSeconds | 调用请求值与单调时钟 elapsed; 不把请求值标为实际 VSync 测量 |

编码接受 `GL_SRGB` 或 `GL_LINEAR`, 映射为项目值枚举. Linear 是有效观测, D04 据此选择明确输出编码; 查询失败或未知值不能填成默认 SRGB. 本轮不启用最终颜色输出, 不声称颜色只编码一次已验证.

GL error 只在这些创建期受控查询作用域检查, 保留原始数值和阶段; 不正常逐帧查询. 不预设 24 个 SSBO 等高于真实需求的门槛, 不导出第二套 DeviceCaps. D02 将从实际设备/资源消费补齐格式和 usage 能力, D02 再落实 clip control, D03 证明 raster/深度语义.

**备选:** `GLFW_SRGB_CAPABLE` 是请求偏好, 用它替代附件查询不能证明实际编码; 从单一版本字符串推断各项已实现功能也没有依据.

### 6. 错误适配器只捕获, 统一边界再报告

GLFW error callback 没有 user pointer 参数, 且可能在产生错误的线程被调用. 适配器在窄进程状态锁内把 code 和描述复制到预先存在的有界捕获记录; 不格式化、不分配、不记录日志、不销毁对象或调用 GLFW. 描述超过上限时标记截断, 回调后不再保存原字符串指针.

每个启动操作开启独立错误捕获作用域: 保留首个错误和额外错误数量, 操作结束后转换到 `HostError`. 已存在的线程 last error 不冒充本次失败; 如需读取 `glfwGetError`, 在调用前明确清理并在之后立即复制, 不用它覆盖已捕获主错误. GL 查询错误经 GL 自身数值记录, 与 GLFW code 区分.

回滚保留创建主错误, 最多附加首个 cleanup 错误与后续错误计数; 不递归构造错误链. 所有动态库/标准异常在工厂边界转换. 正常启动汇总和失败记录在掌握完整上下文的位置各写一次, 不在各 cpp 重复打印.

宿主占用期间保存但不链式调用先前的全局错误 callback, 避免执行未知代码/重复记录; 完成 `glfwTerminate` 后恢复该 callback, 再在保护指针寿命的锁内解除捕获目标. 此时 Diagnostics 和 Impl 仍然存活. 恢复 callback 不是把已经终止的 GLFW 视为已自动清除 callback.

**备选:** 直接在 callback 调用日志可能分配、抛异常或重入; 把描述保存为 string_view 会悬空; 每步覆盖 lastError 会丢掉原始失败.

### 7. 日志容量同时约束条数和消息长度

Diagnostics 显式拥有同步有界诊断后端, 私有复用已固定 spdlog 的 `details::circular_q<DiagnosticRecord>`, 不注册全局 logger. 本轮固定保留 1024 条, 每条完整 payload 最多 2048 字节; 不增加后台日志队列或文件日志轮转系统. 原生环境的多项字符串分条记录, 不把无限拼接的大摘要交给后端.

归档前的逐分配故障检查在 MSVC 19.44 Debug STL 中证明: 原规划的 `logger/ringbuffer_sink_mt` 会在内部 `string/vector/unordered_map` 的 noexcept 构造或移动时分配迭代器代理, 部分 bad_alloc 因而越过不了库自身的 noexcept 边界, 外层 catch 无法转为领域结果. 本轮只需要保留已形成的有界记录, 因此直接复用同版本 circular_q 的容量构造和覆盖算法, 避免修改固定第三方源码、关闭迭代器检查或自建第二个日志框架. 这是本次私有后端的局部取舍, 不推广为其它消费者直接使用第三方内部 API 的惯例; 后续升级必须重新验证该私有依赖. 创建期一次分配完整有界记录空间, 正常写入不动态分配. 快照只拷贝所需记录, 以显式 count 构造和原位复制结果避开 Debug STL 的 noexcept vector 默认构造/移动, 成本仍为有界 O(limit). 验证逐个拒绝 Create、ReadRecent 的各分配点, 并检查写入不分配、覆盖顺序、并发与受控后端失败.

Core 的写入口先将调用方 `string_view` 截入有界 payload, 为截断标记预留空间; 有效 UTF-8 的尾部在完整码点边界截断. 使用已拥有的文本调用私有日志后端, 不公开 fmt/spdlog 类型. 数值标签、身份、源位置和秒时间戳使用固定布局字段或有界前缀, 计入每条总 payload 限制. 不调用会先生成任意大格式化字符串再截断的接口.

维护 retained/evicted/truncated/writeFailed 计数, 累加溢出时饱和. 需要读回时只返回最多 1024 条的自持快照, 不对调用方暴露内部队列. 后端写入异常/错误由本入口捕获并通过写状态和 writeFailed 暴露; 不依赖第三方默认吞掉错误或默认 stderr. 原生 `HostError` 的形成不依赖日志成功.

同一 Diagnostics 的写入和快照由一个内部锁串行; 私有队列不能被外部直接写入. 只有实际成功保留记录后才更新保留/覆盖/截断计数, 失败只更新写失败计数. 日志级别本轮固定允许全部合法级别, 不把后端过滤计为已保留.

容量默认值是本 change 的工程预算, 不宣称性能结果; 压力检查证明高水位、覆盖、UTF-8 截断和失败行为. 不用两套重复的长期日志缓存补齐丢失计数, 不预建 Editor Log 面板.

**备选:** 原始 ringbuffer sink 只限制条数, 单条消息仍可任意增长, 且其 Debug STL 分配失败路径不满足本轮结果合同; 大范围修补第三方构造链收益不足. 异步 logger 会引入当前没有需求的线程、队列和停止协议.

### 8. 回滚与关闭走同一释放逻辑

正常关闭、工厂中途失败与 owner 析构调用同一按取得状态执行的释放过程:

```text
Ready / partial initialization
  --> stop exposing native borrows
  --> clear owned window user pointer
  --> detach owned current context
  --> destroy owned window and context
  --> terminate successfully initialized GLFW
  --> restore previous error callback
  --> detach error capture target
  --> release this owner's process claim
  --> Stopped
```

所有窗口/context 销毁都在 owner 线程且不在 callback 内执行. GLFW 终止仍可能产生错误, 因此捕获器活到 terminate 和 callback 恢复完成. 先保存主错误, 再清理, 最后写一条汇总; 清理不会清除描述/身份. 创建失败的内部 owner 清理后析构不再做第二次 terminate. 已 Stopped 的旧对象不使用全局“当前 host”进行释放.

`gladLoadGLUserPtr` 与 `gladLoadGL` 使用同一固定加载实现, 由 GLFW resolver 提供入口, 不调用 GLAD 自带的动态库 loader, 所以不调用不配对的 `gladLoaderUnloadGL`. user pointer 避免为 resolver 增加一份全局 owner 指针. GLAD 函数指针仅在 Ready 的当前 context 上使用; 下一次创建重新加载, context 失效期间不读取旧表发出 GL 调用. 不维护第二份 loader 状态缓存.

本轮只有环境查询, 没有应用 GPU 资源/提交, 因此无需 glFinish、固定帧延迟或伪造完成票据. 清理若报告错误, 显式 Shutdown 返回 Cleanup 错误, 记录首个失败及数量; 工厂仍保留原始创建错误. 销毁路径继续尝试其余合法释放, 不报告“已验证清理成功”. 若无法解除 current 绑定, 不调用可能通过该绑定重入的销毁步骤; 在具备完整诊断后受控终止进程, 不发布可重试成功. 正常可重试只适用于释放已成功完成的情况.

**备选:** 各个失败分支各写一套 cleanup 容易漏恢复 callback; 只依赖进程退出无法验证下一次创建; D01 提前实现 GPU 等待会混淆两条寿命线.

### 9. 构建和检查沿实际消费落地

- `Engine/CMakeLists.txt` 建立 `PhysaraCore`, spdlog 为 PRIVATE 依赖; `Platform/CMakeLists.txt` 建立 `PhysaraPlatform`, Core 为公开类型所需依赖, GLFW/GLAD 为 PRIVATE. 第一方 target 分别设置 C++20、无编译器扩展、`/W4 /WX /utf-8 /permissive-`; 不向第三方传播警告.
- 根 CMake 仍显式列子目录, 调整为先 Engine 再 Platform 以直接表达当前 Core 前置; 本轮不定义任何 Renderer target. 后续 Engine 内增加下游 target 时仍以无环 target 链表达依赖, 不让 Core 依赖 Platform.
- 公共 include 只满足本模块和明确公开依赖, 不用公开 GLFW/GLAD 头让消费者凑齐编译. Runtime/Main.cpp 保持空文件直到 T04, 不建立无实现 Physara 程序.
- 不建立仓库测试目录、测试框架或长期检查可执行 target. 临时 C++ 检查源和临时 CMake 装配放在 `build/artifacts/establish-native-gl-host/`, 添加正式源码子目录并链接实际 Core/Platform target. 保存实际命令、输出与配置到本 change 的 tasks 证据; 临时文件不进入源树提交.
- 私有故障入口仅用于该临时检查构建, 默认生产构建不编译它; 不在 HostDesc 加入 failNextStep 或用户可见测试选项. 它只改变指定原生调用的输入/resolver 或停止点, 其余工厂、诊断、清理仍使用正式代码. 不建立可替换的全套 GLFW 服务接口.

最小验证矩阵如下. 正常真实宿主是验收前提, 注入/CPU 检查不能替代它.

| 检查 | 方法和可观察结果 |
|---|---|
| Core 身份 | 不同 Tag 不可转换的编译检查; 同槽代际、无效代和临界递增检查 |
| 时间/日志 | 预 GLFW 的有限非负秒数; 1024 条溢出、超长中文文本、独立快照与后端写失败 |
| 原生成功 | Win32 主线程建立隐藏真实 GL 4.6 Core context, 核对 current、必要入口、基础 limits/编码; 可见窗口也走相同路径 |
| 正常退出/复用 | 连续 32 次创建关闭; 保留旧 Stopped 对象时重建并再次关闭旧对象; 每轮核对无遗留本轮原生窗口 |
| 唯一性/线程 | 活跃时再次创建不影响 current/callback/GLAD; worker 显式 Shutdown 返回 WrongThread, 主线程继续关闭 |
| GLFW 失败 | 临时 fixture 选择固定 GLFW 未编译的平台, 让真实 glfwInit 失败并恢复 init hint; 用私有检查入口请求不可能的 context 版本触发真实创建失败 |
| 加载/后阶段失败 | 在已经真实创建的 context 上, resolver 拒绝必要入口; 在 current/查询后受控停止, 检查同一回滚和随后的正常创建 |
| 查询判断 | 正常值与不足版本/错误 profile/缺失入口/linear 编码等纯验证输入; 标明这些分支注入不等于该硬件天然出现过对应条件 |
| callback 寿命 | 预置 sentinel callback, 成功和各失败后核对恢复; 回调描述来源释放后仍能读取返回错误; 全部检查不借用已销毁 GLFWwindow* |
| 构建边界 | 新 binary 目录 Debug/Release 构建; 公共头消费者不需要 GLFW/GLAD; 无新增第一方警告, 无 NVRHI target/include/link |

fixture 自己独占 GLFW init hints, 在所有退出分支恢复其已知基线; 不把注入模式写入生产配置. 准确记录 OS/工具链、GLFW/GLAD 快照、GL vendor/renderer/version、debugContext、visibility 和实际命令. 暂无真实显示/GL 4.6 环境时, 保留未完成检查, 不用 null 后端替代. DPI、输入、最小化、图像/深度/颜色和 GPU 寿命验证分别在后续工作包进行.

## Risks / Trade-offs

- GLFW/GLAD 全局状态限制并行宿主 → 显式进程唯一占用, native fixture 串行, 拒绝第二次创建后才允许触碰原生状态.
- 固定日志容量会覆盖早期记录 → 自持 HostError 独立返回, 覆盖/截断/写失败计数可见; 容量检查随本轮验证.
- 主线程前提不能由便携 C++ 自动发现 → main/fixture 入口承担首次调用保证, 已有 host 的后续操作检查记录线程.
- 原生初始化失败点不一定能在本机自然复现 → 真实调用失败、真实 context 上的受控故障和 CPU 输入分别记证据, 不混称原生行为全覆盖.
- 隐藏窗口仍依赖窗口系统与驱动 → 验证记录具体环境, 不增加 headless 或跨平台支持承诺.

## Migration Plan

1. 实施最小 Core 并立即验证身份、计时和诊断; 然后接入唯一 DeviceHost、GL 启动与回滚, 每步同步完成相应检查.
2. 完成新目录 Debug/Release 与真实宿主矩阵后, 按实际结果更新 README、D01-T01/T02 及本 change tasks; T03/T04 和 D01 总出口保持未完成.
3. 沿原生 verify/sync/archive 流程交付. 本轮不迁移资产/场景; 实施阶段不修改主规格, 验证通过并确认后在归档阶段同步 delta, 不改写既有归档历史. 源码尚未实施时不勾选任务. 回退本增量只移除新增 Core/Platform 实现与对应 target/说明, 保留 D00 和固定第三方快照.

## Sources

本地固定源码是实现 API 核对依据; 官方指南用于确认语义, 不据在线 latest 页面升级依赖. 本轮查阅日期为 2026-10-24.

- [固定依赖与许可](../../../../ThirdParty/README.md): GLFW/GLAD/spdlog 版本和来源.
- [GLFW 公共头](../../../../ThirdParty/glfw/include/GLFW/glfw3.h), [初始化](../../../../ThirdParty/glfw/src/init.c), [窗口释放](../../../../ThirdParty/glfw/src/window.c), [context](../../../../ThirdParty/glfw/src/context.c): callback 生命周期、线程、原生调用与失败清理.
- [GLAD 固定加载实现](../../../../ThirdParty/glad/src/gl.c): 全局函数表、版本检测、resolver 与独立 loader 的区别.
- [spdlog circular queue](../../../../ThirdParty/spdlog/include/spdlog/details/circular_q.h): 固定容量、覆盖顺序与按逻辑下标读取; 记录自持、单条字节上限、同步和饱和计数由 Core 保证.
- [GLFW 初始化/错误与线程指南](https://www.glfw.org/docs/latest/intro_guide.html), [context 指南](https://www.glfw.org/docs/latest/context_guide.html): main thread、callback 描述借用、current 和隐藏窗口限制.
- [Khronos framebuffer attachment 查询](https://registry.khronos.org/OpenGL-Refpages/gl4/html/glGetFramebufferAttachmentParameter.xhtml), [官方参考页源文件](https://github.com/KhronosGroup/OpenGL-Refpages/blob/main/gl4/glGetFramebufferAttachmentParameter.xml): 默认 framebuffer 附件和实际编码查询.
