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

当前第一方目标是 PhysaraCore、PhysaraShader、PhysaraPlatform、PhysaraRHI、PhysaraRHIOpenGL 静态库及产品程序 Physara. Shader 模块内的构建入口见下节. PhysaraRHI 提供模块 include/C++20、六个公共头和 Common/Validation 通用定义; PhysaraRHIOpenGL 单独封闭 loader/原生实现依赖. 当前可链接能力与后端边界见 [RHI 来源说明](../RHI/README.md). Runtime 仍只直接链接其实际使用的 Core/Platform. NVRHI 参考源码不加入构建, CMake 不安装或运行 OpenSpec、格式化器或测试框架.

Platform PRIVATE 链接 PhysaraRHIOpenGL, 在 Ready 前装配 raw Device 与 Validation, 并在释放 context 前收拢. 普通消费者通过 Host 的借用入口访问资源; 直接原生消费者链接 PhysaraRHIOpenGL 并包含 OpenGL.hpp. 只使用通用合同的独立工程仍可单独 add_subdirectory(RHI), 不需要 loader/Platform 或 SDK. 原生 target 包含基础资源、基线图形/传输即时列表、持久映射存储、Event/Timer 查询、状态跟踪及真实提交/完成退休; 直接关闭还可使用 OpenGL.hpp 的预检/drain 入口. 查询不增加第三方或默认测试目标, 公共 staging/map/异步读回仍待后续工作包. 实际支持和 CPU 源寿命见 RHI 说明与头合同, 当前产品事件循环尚不消费它们绘制.

## Shader 构建

PhysaraShader 是 CPU 编译/包消费库, PUBLIC 使用 Core 的有界错误值和 RHI 句柄合同, 编译库/JSON/CNG 为私有实现. PhysaraShaderBuild 是 Engine/Shader 内部薄构建驱动, 随正式默认构建产生, 不增加产品入口或 Runtime 图形依赖. 当前尚无业务 Shader 消费者, 默认构建不生成开发样例包. 声明格式和包合同见 [声明说明](../Engine/Shader/Interface/README.md).

运行期消费使用 [RhiProgram.hpp](../Engine/Shader/RhiProgram.hpp) 的 `LoadRhiProgram(device, packagePath, expectedAbiHash)`, 通常从生成 Data.hpp 的 `AbiHash` 传入期望值. 成功结果拥有 VS/PS RHI 句柄和包身份, 包原始 CPU 存储可释放; 返回前任一阶段失败不交付部分集合. 此入口只链接通用 RHI, 不反向依赖 native backend; 所用 Device 的 owner/context 前提由实际消费者满足. 原生 GL 的绑定展平、GLSL 460 编译/link 在 `CreateGraphicsPipeline` 中发生, 随后通过公共集合/Framebuffer、SetGraphicsState 和直接 Draw/DrawIndexed 消费. 当前普通单样本执行范围见 [RHI](../RHI/README.md#opengl-原生消费边界), Dispatch、间接命令及呈现未交付.

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

## 运行

```powershell
./build/bin/Debug/Physara.exe
./build/bin/Debug/Physara.exe --help
./build/bin/Debug/Physara.exe --hidden --poll-limit=3
```

Release 程序位于 `build/bin/Release/Physara.exe`. 可见和隐藏模式使用同一真实窗口/GL 宿主, 都需要窗口系统和支持 OpenGL 4.6 Core 的驱动.

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
| 1 | Diagnostics/宿主创建、运行或关闭失败; 通过有界 stderr 文本报告 |
| 2 | 参数语法错误, 在原生初始化前退出 |

当前程序运行窗口事件循环, 尚无 Renderer、Editor、绘制或交换缓冲. 因此客户区不输出确定画面, 拖动尺寸后出现的空白或黑白区域不作为渲染结果验收. 最小化和零 framebuffer 状态仍处理事件与关闭, 空闲时使用有限事件等待.

## 接口与开发资料

宿主公开合同见 [DeviceHost.hpp](../Platform/DeviceHost.hpp), 输入单位与事件借用寿命见 [Input.hpp](../Platform/Input.hpp). 以公开头为准, 不在此复制实现或内部 API 使用规则.

项目方案见 [Architecture](Architecture.md), 当前阶段与工作包见 [Developments](Developments/Developments.md#current-stage); 每次实现的取舍和实际检查证据保存在对应 OpenSpec change. 开发约定见 [AGENTS.md](../AGENTS.md).

## 历史本地检查

D02 的实际命令、源码身份与资源/上传读回结果保存在 [资源一致性 change](../openspec/changes/archive/2026-10-29-establish-opengl-resource-conformance/tasks.md). 当时的本地 fixture 及其私有 accessor 是开发材料, 不属于当前工程入口. 2026-11-04 清理后, 正式 Core/Platform 不再保留测试故障宏, RHI 不再保留 ResourceChecks/UploadChecks 的测试 friend; 不能直接用旧 fixture 证明当前输入通过, 也不自动恢复它们.

需要复现历史结论时按归档中的源码/fixture 身份取得实际输入. 当前变更优先使用受影响的正式构建与已有消费者, 按验证技能选择最小检查; 本地材料可随 build 清理.
