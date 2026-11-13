# 构建与运行

需要 Windows x64、Visual Studio 2022/MSVC 的 C++ 桌面开发组件与 Windows SDK, 以及 CMake 3.24+. 第一方代码使用 C++20; Shader 编译依赖以精简源码和固定表快照直接编译, 普通配置/构建不需要 Python 或运行上游生成器. 获取/升级这些快照时才使用 Python 3, 复现说明见 [第三方来源](../ThirdParty/README.md#shader-固定生成源码).

## 构建

在仓库根目录执行:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel
cmake --build build --config Release --parallel
```

依赖源码随仓库保存在 ThirdParty, 配置不下载依赖; 版本与许可见 [ThirdParty/README.md](../ThirdParty/README.md). 所有编译产物进入所选 binary 目录, 不依赖个人机器的绝对路径. 需要选择某个已安装 SDK 时可在 configure 命令中指定 `-DCMAKE_SYSTEM_VERSION=<版本>`.

当前第一方目标是 PhysaraCore、PhysaraRenderGraph、PhysaraRenderer、PhysaraShader、PhysaraPlatform、PhysaraRHI、PhysaraRHIOpenGL、PhysaraEditor 静态库及产品程序 Physara. PhysaraRenderGraph 提供 CPU 图声明和 Pass 访问, 依赖 Core 与公共 RHI. Shader 模块内的构建入口见下节. PhysaraRHI 提供模块 include/C++20、六个公共头和 Common/Validation 通用定义. PhysaraRHIOpenGL 单独封闭 loader 和原生实现依赖. Runtime 链接实际使用的 Core、Renderer、Platform 和 Editor, ImGui 是 Editor 的私有依赖. NVRHI 源码仅供参考. [RHI 来源说明](../RHI/README.md)

Platform PRIVATE 链接 PhysaraRHIOpenGL, 在 Ready 前装配 raw Device 与 Validation, 并在释放 context 前收拢. 普通消费者通过 Host 的借用入口访问资源; 直接原生消费者链接 PhysaraRHIOpenGL 并包含 OpenGL.hpp. 只使用通用合同的独立工程仍可单独 add_subdirectory(RHI), 不需要 loader/Platform 或 SDK. 原生 target 包含基础资源、基线图形/传输即时列表、持久映射存储、Event/Timer 查询、状态跟踪及真实提交/完成退休. Platform 用闭合包的末端复制入口消费窗口颜色, 在同一包 Execute 后检查 Win32 交换结果, 源资源沿真实完成保活. [RHI 支持范围](../RHI/README.md#opengl-原生消费边界)

## Shader 构建

PhysaraShader 是 CPU 编译/包消费库, PUBLIC 使用 Core 的有界错误值和 RHI 句柄合同, 编译库/JSON/CNG 为私有实现. PhysaraShaderBuild 是 Engine/Shader 内部薄构建驱动. 默认构建为 PhysaraEditor 生成 EditorUi 接口及 `Shaders/<Debug|Release>/EditorUi.pshader`. Runtime 共享该包的唯一生成目标, CMake 按配置生成从产品目录到包的相对路径, 运行时从实际 exe 位置解析. 从其他工作目录启动也使用同一配置的包. 缺失或不匹配的包使启动返回 1. [声明说明](../Engine/Shader/Interface/README.md)

运行期消费使用 [RhiProgram.hpp](../Engine/Shader/RhiProgram.hpp) 的 `LoadRhiProgram(device, packagePath, expectedAbiHash)`, 通常从生成 Data.hpp 的 `AbiHash` 传入期望值. 成功结果拥有 VS/PS RHI 句柄和包身份, 包原始 CPU 存储可释放; 返回前任一阶段失败不交付部分集合. 此入口只链接通用 RHI, 不反向依赖 native backend; 所用 Device 的 owner/context 前提由实际消费者满足. 原生 GL 的绑定展平、GLSL 460 编译/link 在 `CreateGraphicsPipeline` 中发生, 随后通过公共集合/Framebuffer、SetGraphicsState 和直接 Draw/DrawIndexed 消费. 当前普通单样本执行范围见 [RHI](../RHI/README.md#opengl-原生消费边界), Dispatch、间接命令仍待后续工作包.

模块提供 `physara_add_shader_interface(name schema)` 和 `physara_add_shader_program(name interfaceName program sourceRoot)`. 调用方显式提供真实输入路径, 在同一 CMake 作用域先声明接口再声明程序. 后者返回 `<name>Package` 数据路径与 `<name>PackageTarget` 唯一生成 target. 每个消费者通过 `add_dependencies(consumer ${MainPackageTarget})` (以 name=Main 为例) 或 target DEPENDS 共享此前置, 再读取包路径; 不能只把包文件分别挂到多个独立生成消费者. 生成头消费者依赖 `PhysaraShaderInterface<name>`、包含生成头根, 使用绑定头时链接 PhysaraRHI. 编译核心不 include 自己的生成物.

规则将接口写入 `<binary>/Generated/Shader/<Debug|Release>/<name>`, 包写入 `<binary>/Shaders/<Debug|Release>/<name>.pshader`. 全部必要输出、工具/请求依赖和源 DEPFILE 显式声明, 配置间没有共同写入者. 删除必要输出会重新生成, 不变输入不执行 Shader 编译.

驱动提供下列命令, 路径参数由实际消费者指定:

```text
PhysaraShaderBuild generate <schema> <output-directory> <stamp>
PhysaraShaderBuild build <program> <source-root> <interface-root> <package> <depfile> <stamp>
PhysaraShaderBuild check <package> <expected-abi>
```

直接 build 始终执行编译/最终核对. 构建输入路径不允许目录联接、符号链接等 reparse point, 首次解析就拒绝, 不把联接切换留给增量时间戳猜测. 非 main/超 profile 输入、混合代、损坏包和不可写输出均非零退出. depfile/stamp 先原子写入, 在发布前就把辅助文件时间设为上一成功包的时间; 包最后发布, CMake 首输出与 depfile 左侧均为包. 辅助时间不会提前表示构建完成, stamp 即使被占用而无法清理, 解除失败条件后的 CMake 构建仍须重跑. 首次构建缺少包时始终未完成. 已成功核对的包若字节相同则只刷新求值时间, 不替换内容, 避免仅输入 mtime 改变后持续重编译; 普通无改动构建不运行驱动. 调用者只在命令成功后使用辅助输出. 可传播的异常返回有界 Failure, noexcept 内的致命 OOM 由构建进程有界报告后失败退出, 不承诺同进程恢复.

开发测试代码、脚本、Shader/JSON 样例和临时 CMake 工程只放在 build 下归类目录, 不注册到正式工程或默认构建. 删除 build 后按上节命令重新配置/构建即可, 不需要也不自动恢复本地测试材料. 具体检查入口、实际运行/复用结果与限制保存在对应 change tasks; 验证预算遵循 [验证技能](../.agents/skills/physara-verification/SKILL.md).

D03-T05 的固定原生图案消费者、实际命令及最终验收见 [图像门槛归档](../openspec/changes/archive/2026-11-08-establish-opengl-native-draw-gate/tasks.md#final-verification). 它复用正式 Shader/RHI 与非零提交后的 Event 完成链, 仅在 build 冷路径读回; 不提供公共 staging/map/异步读回接口, 本地材料不是 Runtime 的绘制入口或普通构建前提.

## 运行

```powershell
./build/bin/Debug/Physara.exe
./build/bin/Debug/Physara.exe --help
./build/bin/Debug/Physara.exe --hidden --poll-limit=3
```

Release 程序位于 `build/bin/Release/Physara.exe`. 可见和隐藏模式装配同一真实窗口/GL 宿主和 RHI 编辑器会话, 都需要窗口系统和支持 OpenGL 4.6 Core 的驱动.

| 选项 | 含义 |
|---|---|
| `--help` | 显示用法后退出, 不创建 Diagnostics 或初始化原生库; 其余参数仍须通过语法检查 |
| `--hidden` | 创建隐藏窗口 |
| `--width=N` / `--height=N` | 十进制无符号 32 位逻辑尺寸, 默认 1280x720; 零值或超过原生 int 范围由宿主配置校验拒绝 |
| `--poll-limit=N` | 成功消费指定次数的 poll 后退出; N 为正的无符号 64 位整数 |

选项不可重复; 未知选项、位置参数、符号、空值、尾随字符、溢出和零 poll limit 属于参数错误.

| 返回码 | 含义 |
|---|---|
| 0 | 帮助、正常关闭或达到 poll limit, 且关闭成功 |
| 1 | Diagnostics、宿主或 Editor 创建失败, 或运行及关闭失败, 通过有界 stderr 文本报告 |
| 2 | 参数语法错误, 在原生初始化前退出 |

可渲染轮次生成主窗口停靠区和面板. Runtime 按索引取得三个帧槽之一, 由同槽的 Event 完成观察决定 CPU 存储和 PresentColor 的复用. 视口有可采样的离屏测试图时显示其颜色, 场景状态和诊断占位在面板中可辨认. 基础 UI 程序或首个字体 atlas 初始化失败返回 1. 列表关闭后由宿主复制有效颜色, 接受提交后交换窗口缓冲. PresentWait 来自实际交换调用.

槽全忙时继续处理输入和有限事件等待. 最小化或 framebuffer 为零时清理交互并暂停新帧. 关闭请求和达到计数上限的 poll 在生成下一份 UI 数据前进入退出, 最后一次 GPU 使用按设备的完成与关闭协议收拢. [阶段计划](Developments/D03-D05-FramePipeline.md#d04-t04)

- 在 `Inspector` 的 `Main View` 中拖动 `Render scale` (0.50x 至 1.00x). `Requested` 显示请求比例, `Effective` 显示最后确认的比例与离屏像素尺寸. 若 `Viewport` 在同一停靠组的后台标签中, 切回该标签后可准备新的诊断图. 需要同时看比例和图像时可将两个面板停靠在不同区域. `Pending` 和失败状态会显示原因.
- 通过 `View > Viewport` 或面板关闭按钮编辑主视口显隐. 在 `Edit` 菜单用 `Undo`、`Redo` 重放显隐和比例编辑, 也可用 `Ctrl+Z`、`Ctrl+Y`. 一次拖拽只生成一条记录, 按 Esc 或失焦会取消尚未结束的预览.
- 在 `Window` 菜单切换 `VSync`, 用 `Effective interval` 查看宿主确认的交换区间. 在 `Title` 中输入标题后按 Enter 提交, 用标题栏和 `Requested title`、`Effective title` 核对结果. 字库缺失的字符在 `Draft title` 和标题回执中显示为 Unicode 码点, 如 `\u{4e2d}`. 两项窗口设置各有自己的确认结果和错误原因. [编辑与窗口设置计划](Developments/D03-D05-FramePipeline.md#d04-t05)

## 接口与开发资料

宿主、输入值和 CPU 编辑器会话的合同见公开头. [DeviceHost.hpp](../Platform/DeviceHost.hpp) · [Input.hpp](../Platform/Input.hpp) · [EditorSession.hpp](../Editor/Session/EditorSession.hpp)

项目方案见 [Architecture](Architecture.md), 当前阶段与工作包见 [Developments](Developments/Developments.md#current-stage); 每次实现的取舍和实际检查证据保存在对应 OpenSpec change. 开发约定见 [AGENTS.md](../AGENTS.md).

## 历史本地检查

D02 的实际命令、源码身份与资源/上传读回结果保存在 [资源一致性 change](../openspec/changes/archive/2026-10-29-establish-opengl-resource-conformance/tasks.md). 当时的本地 fixture 及其私有 accessor 是开发材料, 不属于当前工程入口. 2026-11-04 清理后, 正式 Core/Platform 不再保留测试故障宏, RHI 不再保留 ResourceChecks/UploadChecks 的测试 friend; 不能直接用旧 fixture 证明当前输入通过, 也不自动恢复它们.

需要复现历史结论时按归档中的源码/fixture 身份取得实际输入. 当前变更优先使用受影响的正式构建与已有消费者, 按验证技能选择最小检查; 本地材料可随 build 清理.
