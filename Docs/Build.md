# 构建与运行

需要 Windows x64、Visual Studio 2022/MSVC 的 C++ 桌面开发组件与 Windows SDK, 以及 CMake 3.24+. 第一方代码使用 C++20.

## 构建

在仓库根目录执行:

```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Debug --parallel
cmake --build build --config Release --parallel
```

依赖源码随仓库保存在 ThirdParty, 配置不下载依赖; 版本与许可见 [ThirdParty/README.md](../ThirdParty/README.md). 所有编译产物进入所选 binary 目录, 不依赖个人机器的绝对路径. 需要选择某个已安装 SDK 时可在 configure 命令中指定 `-DCMAKE_SYSTEM_VERSION=<版本>`.

当前第一方目标是 PhysaraCore、PhysaraPlatform、PhysaraRHI 静态库和唯一 Physara 可执行程序. PhysaraRHI 提供模块 include/C++20、六个公共头和 Common/Validation 通用定义, 产生 .lib; 当前可链接能力与后端边界见 [RHI 来源说明](../RHI/README.md). Runtime 仍只链接其实际使用的 Core/Platform. NVRHI 参考源码不加入构建, CMake 不安装或运行 OpenSpec、格式化器或测试框架.

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

项目方案见 [Architecture](Architecture.md), 阶段状态与验收缺口见 [Developments](Developments.md#dev-d01); 每次实现的取舍和实际检查证据保存在对应 OpenSpec change. 开发约定见 [AGENTS.md](../AGENTS.md).
