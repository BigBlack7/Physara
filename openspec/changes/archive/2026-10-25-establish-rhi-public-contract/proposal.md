# Proposal

## Why

[D02-T01](../../../../Docs/Developments.md#d02-t01) 需要为后续 Common/Validation 和 OpenGL 实现提供完整、可独立消费且能追溯到固定 NVRHI 的公共合同, 当前 `RHI/CMakeLists.txt` 为空, 尚无第一方 RHI 声明. 本次沿 [D01 收尾决定](../../establish-window-input-runtime/design.md#closeout) 的限定前置例外推进这一工作包, 将已确认的移植规则、必要局部修正和编译验收落到同一 change.

## What Changes

- 从 NVRHI commit `6b96fb03e07539f08327aea76c56d55f1de9d906` 移植完整公共主头、资源引用、固定容量容器、公共工具声明及主头依赖的共享 GPU 类型, 应用 `Physara::RHI` 和架构规定的受控命名映射.
- 保留 descriptor 字段/默认值、既有枚举数值、虚函数及 RT、Meshlet、CoopVec、bindless、sparse 等扩展声明; 追加 `GraphicsAPI::OpenGL`, 公开原始头版本和来源, 不把声明存在作为能力支持证据.
- 保留 intrusive CPU 引用和容器合同, 补齐公开头的直接标准库依赖. 为标准容器协议名称、上游不初始化的暂存记录及大头文件保留明确、有限的移植说明.
- 经维护者确认, 为 StaticVector 的 initializer-list 构造补 Base() 初始化, 避免合法整数组 swap 读取剩余未初始化的标量/指针槽; 保持逻辑内容、容量和布局, 单独记录这一来源差异并验证部分填充构造/swap.
- 对绑定键进行局部正确性修正: `BindingSetItem` 的相等/哈希纳入 `arrayElement`, 按资源类型访问有效范围字段; `BindingSetDesc` 的相等/哈希纳入影响资源保活的 `trackLiveness`. 不改变公开字段、布局或绑定含义, 差异与验证逐项登记, 实施前在 Architecture 7.1 补清这一局部例外.
- 建立只传播当前公共头和 C++20 要求的 `PhysaraRHI` 消费入口, 完成独立头/消费者编译、来源和声明差异核对、GPU 数据布局与纯 CPU 引用/绑定键检查.
- 同步模块来源说明、构建能力说明与工作包状态; 实际实现/检查后再更新 D02-T01 完成标记, 保留 D01 跨屏环境验收和旧 change 的活动状态.

本 change 交付公共合同与头内 CPU 行为. Common/Validation、外部工具函数定义归 D02-T02, GL 工厂/资源归 D02-T03 起, Shader/绘制/提交归 D03. 这些实现不以占位或成功空操作提前提供.

## Capabilities

### New Capabilities

- `rhi-public-contract`: 完整重命名公共合同的独立消费、数值/布局、CPU 引用与容器行为、绑定键语义、来源可追溯性及声明/实现边界.

### Modified Capabilities

无. 已完整核对 `project-baseline` 与 `native-window-host`: 本次新增第一方公共合同, 不修改既有工程基线或宿主创建/输入/退出要求. `ThirdParty/nvrhi` 继续仅作参考, 不进入正式 targets、include 路径或链接依赖.

## Impact

实现落点为 `RHI/RHI.hpp`、`Resource.hpp`、`Containers.hpp`、`ShaderTypes.hpp`、`Utils.hpp` 及 `RHI/CMakeLists.txt`, 来源与受控差异集中到 `RHI/README.md`. `ShaderTypes.hpp` 承接 `nvrhiHLSL.h` 的共享声明, 不建立业务 Shader ABI 或编译器依赖. 当前没有 RHI 消费者需要修改, Core/Platform/Runtime 的源和依赖方向保持现有边界.

实施时更新 Docs/Architecture.md 的来源映射和局部例外、Docs/Build.md 的实际构建范围、Docs/Developments.md 的过期总览与 D02-T01 关联. 根 README 保持项目介绍和导航. 不修改第三方参考、OpenSpec 配置、旧 change/归档或两个主规格, 不同步/归档本 change.

验证使用本模块正式头和 `build/artifacts/establish-rhi-public-contract/` 下的临时消费者及核对输出, 不新增测试框架、CI、独立工具模块、SDK 或依赖下载. 本 change 完成仍不表示 D01/D02 阶段出口通过、GL Device 已创建或任何 GPU 能力可用.
