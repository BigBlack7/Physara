# Design

## Context

动机和 D01-T03/T04 范围见 [proposal](proposal.md), 行为要求见 [window-input](specs/window-input/spec.md) 与 [runtime-lifecycle](specs/runtime-lifecycle/spec.md). 起点为提交 `8e9e3e0`: Core/Platform 已有实际实现, `DeviceHost` 独占 GLFW、稳定持有私有 State, 只安装进程错误回调; `Runtime/Main.cpp` 与 Runtime CMakeLists 仍为空. 原生依赖、窗口 user pointer、线程和回滚的权威实现继续复用, 不另建 Window 或 Application owner.

约束依据为 Architecture 的 [主循环](../../../Docs/Architecture.md#frame-flow)、[Runtime](../../../Docs/Architecture.md#runtime)、[输入](../../../Docs/Architecture.md#input)、[UI chaining](../../../Docs/Architecture.md#editor-input)、[退出](../../../Docs/Architecture.md#shutdown), 以及 Developments 的 [D01-T03/T04](../../../Docs/Developments.md#d01-t03). 这些章节描述完整产品; 本次只实现开发计划明确要求的无渲染器阶段, 不照抄尚不存在的对象或方法.

## Goals / Non-Goals

**Goals:** 输入只有一个写 owner 和一个发布边界; 消费者持有值快照及有期限的事件借用; 所有热回调有界、不分配、不调用未知代码; 失败不会交付半批成功; Runtime 的可执行入口能实际退出并供自动进程检查复现.

**Non-Goals:** 增加全局 Input、IWindow/IInput、多线程事件服务、可替换 GLFW 服务、空请求生产者、通用命令/配置框架或无实际消费者的帧时间/作业设施. 不把固定硬件上受控事件注入当成跨屏、物理输入设备或完整 UI 渲染的证明.

## Decisions

### 1. 输入算法与原生适配有具体归属

| 位置 | 本轮职责 | 依赖/所有权 |
|---|---|---|
| `Platform/Input.hpp` | WindowState、InputSnapshot、事件、请求和单位明确的值类型 | 仅标准库; 不含 GLFW/GLAD/RHI 或可变全局 |
| `Platform/InputState.hpp/.cpp` | 私有 InputState, 持有双缓冲、held/edges/deltas、窗口事实、序列和首个输入故障 | 作为稳定 DeviceHostState 的值成员; 只依赖标准库/Core 值类型, 不调用 GLFW 或日志 |
| `Platform/GlfwInput.cpp` | 原生码映射、12 类回调安装/解绑、窗口事实查询、poll/wait 和请求执行 | 只借用当前 State, 把原生结果送到 InputState |
| 既有 `GlfwWindow.cpp` | GLFW 生命周期和窗口/context 建立, 配合输入释放 | 保持现有进程唯一性和错误回调 owner |
| `DeviceHost.hpp/.cpp`、私有状态头 | 公开操作、线程/重入检查、阶段及失败传播, 启动/关闭装配 | 仍是唯一公开有状态平台入口 |
| `Runtime/Main.cpp` | 参数解析、Diagnostics/Host 装配、循环及退出 | 不 include 原生头, 不持有另一个平台服务 |

私有 InputState 是一个实际算法 owner: 两个事件 bank 的寿命与发布、输入转换和故障恢复属于同一不变量. GlfwInput 是原生入口适配, GlfwWindow 继续管理原生资源寿命; 拆分依据是状态合同和依赖, 不是机械按行数拆文件. 不复制第二个 native context 或 InputSnapshot 服务. 实施时在 Architecture 的源文件职责表补记最终落点, 长期依赖和 owner 关系不变.

职责复审: 实施后的 Input.hpp 超过 300 行, 其中 120 个逻辑键枚举和具名值事件属于同一个公开输入协议; GlfwInput.cpp 超过 600 行, 含对应机械码表、12 类对称安装/解绑及同一窗口的有序请求算法. 按 AGENTS 的评审触发条件检查后保留这些职责边界, 不新增 owner 或只按行数切分的转发文件. 饱和清理错误合并由 DeviceHost.cpp 的既有非平凡算法统一供窗口/输入释放使用; 键盘与按钮共享显式 modifier 转换.

**备选:** 全放进 DeviceHost 会把窗口寿命、CPU 输入算法和原生事件泵混合; 再建公共 IInput 会引入没有替换消费者的虚层级. 两者均不采用.

### 2. 窄公开合同不泄漏原生类型

拟新增的入口如下, 是待实施的声明方案:

```cpp
using PollResult = std::variant<HostFrameInput, HostError>;
[[nodiscard]] PollResult PollEvents() noexcept;
[[nodiscard]] std::optional<HostError> WaitForEvents(double timeoutSeconds) noexcept;
[[nodiscard]] std::optional<HostError> ApplyDeferredRequests(std::span<const HostRequest> requests) noexcept;
```

三者仅限 owner/main 线程, 不可重入; `ApplyDeferredRequests` 名称表示由调用方选定延后边界, 请求在该次调用内顺序执行, 不暗藏长期队列或保留传入 span. 空 batch 是合法的零请求, 但 Runtime 不为它新造空生产者. 既有 WindowId 仍在 DeviceHost 头中, 不重复定义身份或原生窗口名.

- `WindowState`: `WindowExtent clientSizeLogical/framebufferSizePixels` 用受检 uint32 宽高, `ContentScale` 用有限正 float x/y, 另有 focused/iconified/closeRequested 与 uint64 metricsRevision. Architecture 的 Float2 示例表达单位; GLFW 的整数尺寸在本轮按整数保存, 不引入新的数学抽象. `Renderable()` 只判断非 iconified 且 framebuffer 两维均大于零.
- `InputSnapshot`: 已知 Key/MouseButton 的固定 ButtonState 数组 (down/pressed/released), double 逻辑光标位置、逻辑运动 delta、double 滚轮 offset, cursor-inside 与实际 CursorMode. 不另存一份从 held keys 推导的可变 modifiers; modifiers 由有序按键/按钮事件携带.
- `Key` 覆盖固定 GLFW 头定义的逻辑键, 映射表在适配 cpp 私有; `Unknown` 与 `Count` 不作为有效下标. scan code 仅是 int32 不透明事实, 不作为逻辑数组下标. MouseButton 覆盖 8 个按钮. KeyAction 区分 Press/Release/Repeat, modifier 位显式映射 Shift/Control/Alt/Super/CapsLock/NumLock.
- `InputEvent`: Key、Char、MouseButton、CursorPosition/Enter、Scroll、ClientSize、FramebufferSize、ContentScale、Focus、Iconify、CloseRequested 等小型具名值的 variant. 文本为 char32_t Unicode scalar, 事件无 string/vector、原生指针或独立堆 payload. 请求为 `SetCursorMode{Normal|Relative}` 与 `SetCloseRequested{bool}` 的 variant.
- `HostFrameInput`: uint64 sequence、自持 WindowState/InputSnapshot 与 `std::span<const InputEvent> events`. 值成员可以跨关闭保留; span 只到下一次通过前置检查的 owner PollEvents 开始或 owner Shutdown 开始. Wait/Apply 只写 pending, 包括其失败处理也不覆盖已发布 bank. 跨帧/线程保存事件必须由消费者自持复制.

这些结果只包含固定值和 span, 返回不移动动态容器; 不重现前一 change 发现的 Debug STL noexcept vector 分配问题. `HostInfo` 仍是创建期观测, 动态窗口事实只由 WindowState 发布, 不逐帧同步修改 HostInfo 的初始设置.

另提供纯值函数 `Core::BoundedText DescribeHostError(const HostError&) noexcept`, 从既有 ReportError 中抽取 stage/identity/native code/description/cleanup 的非平凡有界格式化并纳入 requestIndex. 内部诊断写入和 Runtime stderr 共用这一来源, 不再从最后一条日志猜测主错误. 该函数不访问宿主或原生 API, 可在任意持有自持 HostError 的线程调用; 不是新增错误框架或有状态服务.

**备选:** 返回内部可变 vector 会破坏发布寿命; 每帧复制整批事件无必要. 把 GLFW key/enum 直接作为公开索引会使 Unknown 和版本范围越界.

### 3. Ready 前建立输入, 同一回滚撤销

State 的输入存储在取得进程占用前随 State 一次创建, 保持 heap 中稳定地址. 既有 GLFW → window → current → GLAD → environment → swap interval 成功后增加 `InputSetup`, 全部输入准备成功后才设置 Ready:

1. 用现有 BeginNativeOperation/TakeNativeError 包围输入准备; 从真实窗口查询尺寸、scale、focus、iconify、close flag、cursor position/mode, 受检后种入初始事实, metricsRevision 从 1 开始. held 初值为全释放, 不推测创建前的物理按键.
2. 确认稳定 user pointer 仍为 State; 安装 key/char/mouse button/cursor position/cursor enter/scroll/focus/iconify/close/window size/framebuffer size/content scale 共 12 类回调. 在首个安装动作前登记需要解绑, 覆盖部分安装失败.
3. 以受检调用开启 `GLFW_LOCK_KEY_MODS`, 取得事件中的锁定修饰键事实. 不开启 sticky keys/buttons 或 raw mouse; Relative 映射 `GLFW_CURSOR_DISABLED`, 不是 GLFW 的可见 confined `GLFW_CURSOR_CAPTURED`.
4. 安装后再查询并协调窗口事实, 捕获安装期间的变化; 不清空已经到达的 pending 事件. 回调安装/观察任一步失败均通过原工厂回滚, 不返回半就绪 owner.

回调只经 `glfwGetWindowUserPointer` 解析稳定目标, 做码值转换和有界累积. 不分配、格式化日志、查询 GL、创建资源、调用业务或重入 poll/wait/请求. 原生码先转合法项目值; int 尺寸先检查非负再转 uint32, scale/位置/scroll 检查有限性, scale 要大于零. 零尺寸是有效观测, 不是 1×1 或初始化错误.

**备选:** 靠 HostDesc 填初始状态会遗漏 DPI/焦点事实; 回调安装前后清 pending 会丢失同步到达的事件. 产品首次 poll 之后再安装会形成未验证的 Ready.

### 4. 双 bank 发布保证顺序和寿命

每个 host 固定两个 `std::array<InputEvent, 4096>` bank, 容量通过 Input.hpp 的命名常量声明. `sizeof(InputEvent) <= 64` 用 static_assert 检查, 两 bank 事件空间上限 512 KiB, 加小型状态/错误; 不运行时增长、不逐帧申请或清零整块. State 在 heap 内原位构造, 不用大临时对象或跨帧 arena.

```text
GLFW callbacks / Wait / request side effects
                -> pending events + authoritative current facts
owner Poll      -> native poll -> reconcile facts / release capture -> check -> bank swap
                -> owned value header + const event span -> Runtime consumer
                -> clear interval edges/deltas; retain held and current window facts
```

Poll 前先检查 owner、重入、故障和 Ready; 被拒绝的调用不结束上次借用. 接受后借用按合同到期, 但不能在 glfwPollEvents 前清 pending. 原生 poll 后以受检 getter 协调最新窗口事实; 与已知值相同不重复发布变化. 完成安全的焦点/光标释放, 检查原生和输入故障后才发布. sequence 从 1 单调递增; 空 poll 也发布新 sequence, Wait/Apply 不递增.

发布交换 bank 索引, 只复制小型值头; 旧 published bank 在此成为下一个 pending bank, 只把其有效数量置零. 清当前快照的 pressed/released、cursor delta、scroll offset, 不清 held/当前位置/窗口状态. 每个到达的事实只进入 pending 一次; 本轮不合并事件记录, 窗口状态本身始终合并为最新值.

metricsRevision 只在逻辑尺寸、像素尺寸或 scale 真正改变时递增, 不因重复 query、焦点变化或空 poll 增加. 快速变化后回到相同值也保留已经发生的 revision 递增. 两类 uint64 递增均检查最大值, 不回绕. 不把多个顺序 getter 的观测描述为操作系统提供的原子快照.

**备选:** 单 bank 会被 Wait/请求同步回调改写; 动态增长无法保证回调有界. 本轮保留全序比位置事件的就地合并更容易验收, 容量超限按第 7 节处理.

### 5. 输入规约与失焦清理

| 输入 | 唯一更新规则 |
|---|---|
| 已知 key/button Press | down=true, pressed=true; 追加事件 |
| Release | down=false, released=true; 保留同区间已有 pressed |
| Key Repeat | 只追加 Repeat, 不伪装新的 pressed |
| Unknown key | 保留 Key::Unknown、scan code/action/modifier 事件, 不访问 keys 数组 |
| Char | 检查 <=0x10FFFF 且不在 surrogate 区, 按序保存 char32_t; 不从 key 推文本 |
| CursorPosition | 保留 double 逻辑坐标; 仅有焦点且基线有效时累加差值, 检查差值/累加结果有限 |
| Scroll | 保留 signed double offset, 检查累加有限; 不乘 DPI, 不当 framebuffer 像素 |
| Focus loss | 对原 held 项置 released, 清 down 和运动 delta, 使基线无效并锁存释放 Relative 的要求 |

失焦先清状态, 但不自己追加逐键物理 Release; 固定 GLFW 源码会在 focus callback 之后生成已按下 key/button 的 Release 回调, 这些事实仍各保留一次. 不清掉本区间已出现的 press/text/scroll 事实. 焦点恢复不自动重新捕获; 恢复后的首个位置样本只建基线, delta=0. 模式实际改变前清运动/基线, 原生调用可能同步发位置回调, 改变后再次使下一样本建零基线, 防止虚拟坐标与恢复位置混算.

InputState 的焦点回调只锁存安全要求, 由外层原生调用返回后执行 Normal 请求; 焦点丢失优先于同次捕获请求, 不在 C callback 内调用 glfwSetInputMode. 只改变模式的真实转换会重置基线, 同模式的幂等请求不丢掉本区间运动.

**备选:** 只读 glfwGetKey 会遗漏同一 poll 的 press+release; 只依赖原生 release 才清理会使 UI 先观察到 held/capture 的旧状态. 累加光标从旧坐标开始会造成恢复跳变.

### 6. 请求与等待具有明确边界

`ApplyDeferredRequests` 在调用方消费发布结果之后、进入下一等待或下一帧工作前调用. 当前 Runtime 没有 Editor 请求生产者, 不制造占位队列; 临时原生消费者实际验证该入口, 后续 Editor 使用相同边界.

- 完整预检 batch 的枚举/值和初始焦点; 无效值或已失焦的 Relative 请求在任何 native 状态修改前整体拒绝. 输入 span 只借到返回, 顺序处理不做内部缓存/排序.
- Normal/Relative 对应受检 glfwSetInputMode 和实际 mode 查询; `SetCloseRequested` 对应 glfwSetWindowShouldClose 与实际 flag 查询. close callback 仅发布意图, 不清标志、不销毁窗口. 清标志由上层取消请求执行.
- glfwGetInputMode 读回的是库中的模式状态, 不是物理捕获或 raw motion 证明; 结合实际 focus 确定项目 Relative 状态, 原生检查另观察 Win32 光标约束/释放和位置行为. 不把一次 setter/getter 相等扩大成全部平台的捕获能力.
- 同步回调进入 pending bank. 已发布帧保持原样, 新实际模式/close 状态由下一 poll 反映. 不将一个请求值缓存成另一份权威设置.
- 原生失败时停止 suffix, HostError 的可空 `requestIndex` 指明 0-based 失败请求. 成功 prefix 可能已经生效, 不伪称事务回滚. 捕获过程中失焦时在安全边界回到 Normal, 返回该 index 的 RequestRejected, 已明确恢复后可继续处理事件; 不声称请求一直有效.
- `WaitForEvents(timeoutSeconds)` 只接受有限且 >0 的秒数, 包围 glfwWaitEventsTimeout 和必要的事实协调/光标安全处理, 不发布、不换 bank. 超时也不说明有回调发生. 固定 GLFW 头声明 positive finite, 当前 window.c 实现额外接受 0; 项目采用头文件的正式前置, 不依赖这一实现宽容行为.

0.05 秒是本轮 Runtime 的空闲唤醒请求, 不是实时截止保证; GLFW 明示原生拖动/菜单等可能阻塞事件处理. 没有作业系统消费者, 本轮不增加跨线程 PostEmptyEvent/watcher 入口.

**备选:** 在回调执行请求违反重入边界; 静默最后一个请求覆盖全部 prefix 会隐藏副作用. 把 Wait 作为另一个发布入口会缩短现有借用并重复/丢失输入.

### 7. 有界故障不交付半批成功

沿用 `HostError`/ErrorContext/Diagnostics. 增加 InputSetup、EventPolling、EventWait、RequestApplication 阶段及对应输入/请求失败原因, 一并更新既有阶段名称诊断映射. requestIndex 只表示请求位置, 不挪用 additionalNativeErrors 统计输入数量. 新操作的 elapsedSeconds 用本次操作的 steady_clock 起点, 不把宿主总生存期冒充操作耗时.

- 错误线程、重入、Stopped、非法超时、无效 batch 和有界 RequestRejected 属于前置/可恢复拒绝, 在拒绝点不修改已发布数据、不锁存终止故障. 回调内的重入拒绝只返回自持错误, 不在回调内记录日志.
- 队列满、非法数值/scalar、序列/revision 耗尽锁存首个 InputFault. 回调仅保存小型原因/来源及饱和拒绝计数, 清 held/运动和基线; 后续事件不再覆盖首错或补出伪完整批次. 在原生操作外形成 HostError 和有界说明.
- 输入故障或 native poll/wait/query/request 失败进入终止输入态: Ready=false、不再允许普通输入操作/新原生借用, State/window/进程占用仍归原 owner, Shutdown 仍可用. 尝试在安全边界解除 Relative; 不能确认解除时由最终窗口关闭完成资源释放并保留诊断, 不继续发布.
- 先把操作错误复制成自持主错误, 再开启光标释放等清理的独立错误作用域. cleanup 只附加首项及有界计数, 不能覆盖主错误. 同一次原生操作兼有 native error 与 InputFault 时以捕获的 native error 为主, 在有界说明追加输入故障事实; 两者均触发停止发布.
- 终止输入态重复调用返回保存的首错, 不重复日志. 需 owner Shutdown 后新建宿主, 不提供难以证明完整性的原地输入恢复. 新宿主以新 WindowId 和全释放输入开始.

原生请求返回后先核对 native error 和回调 InputFault, 再解释可恢复的失焦捕获拒绝; 两类检查统一保留 requestIndex. 光标安全释放也是可能触发同步回调的原生边界, 返回后再次检查输入故障, 防止容量耗尽等终止故障被可恢复拒绝遮蔽或以成功返回.

所有正常回调与发布均无动态分配; 异常与日志错误不能穿越 C callback 或把原始错误变成成功. 私有故障入口沿用现有检查宏, 只在 build 树 fixture 的 target 生效, 不在 HostDesc 加测试选项.

**备选:** 丢最旧按键/文本后继续成功会留下卡键与错误业务动作; 动态扩容或普通输入日志逐条输出破坏预算. 自动重建输入而不通知消费者无法补回丢失顺序.

### 8. 防重入和退出覆盖真实窗口寿命

State 增加 owner-thread 的 operation-active 阶段标记, 用作用域管理公有 poll/wait/apply/shutdown; 输入入口依次检查 owner、嵌套操作、Stopped、终止输入故障和 Ready. 不用锁允许跨线程原生调用. Shutdown 在 owner/重入检查后直接走既有幂等释放, 不因 Ready=false 或输入故障拒绝关闭. GetInfo 的创建期值不变; 已发布输入值可独立保留, 活宿主仍不允许并发访问.

显式 Shutdown 重入返回 ReentrantCall, 不触碰窗口. 在回调里销毁 owner 无法返回失败, 析构必须检测活动操作并走现有 TerminateForContract, 不能忽略 Shutdown 的重入拒绝后释放仍被原生调用访问的 State. 原有错误线程析构和无法安全 detach current 的致命协议保持.

正常关闭、输入故障和初始化失败都按实际取得资源执行同一释放顺序:

```text
stop publishing / expire borrows
  -> caller's UI backend shutdown (本轮只在集成 fixture 中存在)
  -> safely leave Relative while window/context remain owned
  -> disconnect all platform input callbacks
  -> clear window user pointer
  -> existing detach -> destroy window/context -> terminate GLFW
  -> restore previous process error callback -> release process claim
  -> destroy Host -> destroy Diagnostics
```

后装 UI 必须先 Shutdown 恢复平台回调, 不在其接管期间重新覆盖 callback/user pointer. 原生 fixture 使用固定的实际 ImGui GLFW backend 验证这条链, 只做平台适配初始化/事件/关闭, 不声称已实现 ImGui renderer 或完整 UI 光标策略. 私有输入故障检查在 owner 内观察仍拥有的 native state, 不从失效 GlfwAccess 借用指针继续调用 GLFW.

**备选:** 先清 user pointer 再指望回调自行工作有悬空风险; 回调里直接删除窗口违反 GLFW 非重入约束. 每个路径复制一份释放逻辑易遗漏部分安装目标.

### 9. Runtime 是最小实际组合入口

`Runtime/Main.cpp` 使用常规 `int main(int argc, char** argv)` 与控制台子系统以保留错误输出, 不增加 Application 类或第二产品入口. 参数用 string_view/from_chars 和固定值解析, 没有 JSON/配置文件/环境变量服务.

| 选项 | 语义与边界 |
|---|---|
| 无参数 | 使用现有 HostDesc 的默认逻辑尺寸, visible=true, 无 poll 上限 |
| `--help` | 全部参数先做语法检查, 然后输出用法并返回 0; 不创建 Diagnostics/GLFW |
| `--hidden` | 仅改变 HostDesc.visible; 仍需真实 Win32/GL context |
| `--width=N` / `--height=N` | 十进制无符号 uint32, 拒绝符号、尾随字符、空值及溢出; 0 和超过 int 的值交既有 Host 工厂验证, 不复制其配置规则 |
| `--poll-limit=N` | 十进制正 uint64; 0/溢出为参数错误, 默认无上限; 用输入 sequence 判断已消费成功 poll 数, 不再维护一份帧计数 |

未知/重复选项、位置参数或格式错误在 native 初始化前返回 2; 带 help 的参数也要先通过语法检查. `--help --width=0` 语法成立, help 分支无需验证尚未创建的宿主. 选项在 Docs/Build.md 列清, 不增加隐藏的第二套运行模式/入口.

进入次序为解析 → Diagnostics::Create → DeviceHost::Create → 循环. 每次成功 Poll 后先处理 closeRequested 与 poll-limit; 再用 Renderable 判断暂停/恢复事实, 只在状态转换时记录一条有界诊断. 本轮没有图形工作, 正常和暂停分支最终都请求 0.05 秒事件等待, 不制造空渲染分支、零尺寸目标或 GPU wait. 后续真正接入 Editor/Renderer 时在消费之后、等待/下一帧之前插入已有请求边界; 本轮不产生虚构 UI 请求.

循环只在实际存在的请求到来时应用请求, 然后 Wait; Wait 到达的回调保留到下一 Poll. 没有帧 dt 消费者, 不预建闲置计时基准; 恢复首个 cursor delta 的规则已在输入层实现. 不调用 GL clear/draw/swap, 因而窗口内容不作为渲染验收.

### 10. 进程结果与有序退出

返回码固定: 正常 close/poll-limit/help 为 0, 参数语法错误为 2, Diagnostics/Host 初始化、事件操作或显式关闭失败为 1. 解析后持有 Diagnostics, 随后持有 Host, 保证逆序析构; 循环退出走同一显式 Shutdown, 再退出局部 owner. 操作失败后不从循环直接绕过显式关闭.

底层在原生操作边界记录一次失败, Runtime 不再次写一份相同错误到 Diagnostics. 进程结束前直接将返回的 HostError 经 DescribeHostError 形成的有界文本 fwrite 到 stderr, 不读可能无关的最后一条日志, 不新增快照分配/失败路径. 无 Diagnostics owner 的创建失败、参数错误使用各自自持 ErrorContext/有界文本和无分配输出. 诊断写入或输出失败不能替换原结果; 原操作与关闭同时失败时保留原操作为主, 补充关闭事实, 返回码保持 1.

**备选:** 让 Runtime 复制另一套 HostError 格式化会造成两份阶段映射; 读回最后日志不能证明与当前返回错误对应, 且引入额外分配. 共享纯值描述函数满足两个实际消费者.

循环外围捕获未预期标准异常并走同一拥有者关闭路径, 不允许异常越过 main 或 native callback; 不为后续 GPU/job owner 创建空清理动作. Startup 工厂失败继续使用原回滚; 没有成功 host 就不调用假 Shutdown. 后续每引入实际 owner 再扩展同一逆依赖退出链.

### 11. 构建与证据跟随实际消费者

- Platform CMake 加入 Input.hpp、私有 InputState 和 GlfwInput 源; Core 不增加 Platform 依赖. Runtime CMake 建立唯一 `Physara` 可执行 target, PRIVATE 链接实际使用的 PhysaraPlatform/PhysaraCore, target 级 C++20、现有警告和 utf-8, 不公开原生头或全局改第三方选项.
- 临时检查放 `build/artifacts/establish-window-input-runtime/`, 添加正式 source targets, 不把 fixture 变成仓库测试框架. CPU 检查消费私有输入算法, 原生检查和产品都消费同一 DeviceHost; 不能以 CPU 替身证明 native 行为.
- 私有检查宏可扩展现有 DeviceHostChecks: 输入准备中途停止、计数临界、受控非法回调和 native 错误; 生产 target 不启用. 单独检查生产符号、公开消费者 include 和实际链接图.
- 新 binary 目录离线 Debug/Release 构建, 产品 `--hidden --poll-limit=3`、help、非法参数/尺寸、正常 OS close 与重复进程; 回归 Core 和上一 change 的真实创建/32 次关闭/错误回调恢复. 源码、参数与原生环境都记录实际输出, 不套用上一 change 的成功结论.

原生矩阵覆盖 window size/content scale/framebuffer、focus/cursor、poll 外同步回调、wait、关闭取消、callback chaining、部分输入安装失败和在途退出. Win32 最小化可能保留非零 framebuffer, 按实际值和 iconified 判定; 零 extent 算法检查与自然出现零 framebuffer 分别记证据. 有不同 DPI 显示器时实际跨屏并记录显示器/scale; 不具备环境则相应验收未完成, 不把注入值当成跨屏实测. UI 平台适配检查不包含 UI Draw、字体上传或渲染画面.

**备选:** 为验证建立另一套 Host/Main 会绕开真实入口; 预建 GPU/Editor 占位对象或测试框架不满足当前需求. 当前没有性能优化结论, 不增加性能报告门槛.

## Risks / Trade-offs

- 固定 4096 事件预算可能在极端消息洪峰中超限 → 显式失败、全释放输入、安全解除捕获并关闭重建; 两 bank 字节预算和临界场景可检查.
- 固定数组增加单个 State 的一次性内存 → 限定事件大小/容量、heap 原位构造, 不跨帧复制或扩容; 不宣称比其他实现更快.
- span 不能自行阻止过期访问 → 公开寿命合同与双 bank, 只在有效期内做一致性检查, 跨帧消费者必须复制.
- 窗口系统可同步回调且 poll/wait 可能被 OS 交互阻塞 → 不在 poll 前清 pending, owner/reentrancy guard, 正的有限等待仅是请求.
- UI backend 自身也会改光标状态 → 本轮只验证官方 callback chaining, 后续 Editor 的 cursor 控制需在其私有适配按相同权威状态合同接入, 不提前假称完整兼容.
- 显示器、焦点权限、IME 配置影响真实输入检查 → 分别记录实际 OS 事件、原生受控消息、直接回调/纯值注入; 未运行分支不声明覆盖.

## Migration Plan

1. 按 tasks 先落地公开数据与私有累积算法并即时做 CPU 边界验证, 再接入原生输入准备/回调、poll/wait/请求和同一释放链.
2. 确认原生公共合同与上一宿主回归后建立 Runtime target 和真实入口, 验证参数、循环/暂停与进程退出; 同步更新 Docs/Build.md 与实际源文件职责说明, README 保持项目名片和文档导航.
3. 全部必要证据通过后更新 Developments 的 T03/T04 与 D01 出口, 执行原生 verify/strict validation; 主规格同步和归档另沿原生流程, 不在实施前标记完成.
4. 回退只撤销本轮新增输入/Runtime 和相应源列表/说明, 保留已归档 native-window-host 和固定第三方源; 不中途重写上一 change 的历史.

<a id="closeout"></a>
## 阶段收尾与后续前置

维护者已明确接受本次保留 4.1 跨屏环境缺口, 在整理代码和文档后自行提交, 后续另开会话推进新任务. 此为环境验收延后的限定决定, 不删除行为要求, 不把 4.1 或 D01 出口标为完成; 当前 change 保持活动状态, 本次不执行规格同步或归档.

- 原因: 当前仅一个活动显示器, 实际不同缩放跨屏无法执行; 已实现的逻辑坐标、framebuffer 像素和 content scale 合同保持独立, CPU/单屏原生/进程证据在 tasks 中记录.
- 适用范围: 允许新 change 先推进 D02-T01 的公共 RHI 合同移植、来源映射和公共消费者编译. 这些工作不依赖跨屏尺寸或 UI 缩放; 不因此放宽其他任务的前置, 也不提前声明窗口呈现或跨屏 UI 已验收.
- 验证与退出: 下一 change 记录此项前置例外及其独立编译/接口核对方法. 获得不同缩放的双屏环境后, 回到本 change 的 4.1, 实测同一正式宿主/Runtime 的 logical/framebuffer/scale/revision 与响应、关闭行为, 再核对 D01 出口. 未来窗口呈现和 UI 缩放仍须按实际集成回归.

README 依维护者定位仅作项目介绍、展示及文档导航; 构建运行说明集中到 Docs/Build.md, 接口合同以公开头为准, 当前阶段状态与验收证据仍分别归 Developments 和本 change 的 tasks, 后续功能不再逐次向 README 累加实施说明.

## Sources

核对日期为 2026-10-25. 固定源码决定实际可用 API/行为, 在线官方指南用于核对语义, 不据 latest 页升级依赖.

- [GLFW 固定头](../../../ThirdParty/glfw/include/GLFW/glfw3.h)、[window.c](../../../ThirdParty/glfw/src/window.c)、[input.c](../../../ThirdParty/glfw/src/input.c)、[Win32 window](../../../ThirdParty/glfw/src/win32_window.c): 线程/重入、focus 后 release、输入模式、timeout、尺寸/回调及失败.
- [官方输入指南](https://www.glfw.org/docs/latest/input.html): poll/wait 都处理事件、其他原生调用也可能同步回调、逻辑光标与字符输入. [官方窗口指南](https://www.glfw.org/docs/latest/window.html): framebuffer 与 logical size 独立, content scale 不等于两者比例. [官方线程/重入指南](https://www.glfw.org/docs/latest/intro.html): 原生事件处理边界.
- [固定 ImGui GLFW backend](../../../ThirdParty/imgui/backends/imgui_impl_glfw.cpp): InstallCallbacks/RestoreCallbacks、保存/链式调用平台回调与 IO 更新; [第三方构建](../../../ThirdParty/CMakeLists.txt) 已有 imgui 核心/GLFW adapter target.
- [现有 DeviceHost](../../../Platform/DeviceHost.cpp)、[私有状态](../../../Platform/DeviceHostInternal.hpp)、[GlfwWindow](../../../Platform/GlfwWindow.cpp)、[上轮设计](../archive/2026-10-25-establish-native-gl-host/design.md): owner、错误捕获、回滚、稳定地址和 Debug 分配教训.
