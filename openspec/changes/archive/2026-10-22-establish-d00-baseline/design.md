# Design

## Context

当前业务模块尚未实现。支持环境为 Windows x64、MSVC、C++20；工程面向个人维护，初始化以清晰、直接的构建组织和必要说明为目标。

## Goals / Non-Goals

**Goals:** 普通构建离线可用、第三方版本与许可可追溯、Debug/Release 从源码构建、模型与场景职责明确、文档和目录一致。

**Non-Goals:** 不实现 D01 及之后的业务功能，不预建测试框架、工具程序、开发环境安装或自动检查设施，不声称具备运行时或 GPU 验证能力。

## Decisions

- 构建仅使用根目录与模块内的 CMakeLists.txt。第三方 target 直接声明源码、头路径和依赖；业务 target 随真实实现出现，不创建空库和示例入口。第一方编译要求在实际 target 上声明，不污染第三方。
- 第三方采用精简 vendored 源码。固定版本或 commit、上游地址、许可及必要本地差异集中记录于 ThirdParty/README.md，不引入额外依赖配置或逐文件校验清单。普通配置不下载内容，也不要求预编译库或 submodule 初始化。
- NVRHI 仅保存源码参考；不声明其 target、include 或链接依赖。单头库直接提供 include 路径；STB 实现宏在未来真实资产解码 cpp 中定义一次，不增加当前没有消费者的包装 cpp。
- Assets/Models 保存标准 glTF/GLB 模型。Assets/Scenes 保存编辑器 JSON 场景，记录实体身份、层级、变换、模型引用与场景设置。具体模型引用、实例覆盖、版本校验、加载失败及安全保存合同以架构 3.4 为准；场景 JSON 不复制网格、纹理或 GPU 对象。
- Asset 负责解码和资源发布；Scene 负责实例状态、可保存快照及候选场景校验；Editor/Session 组合 JSON 文件读写及 Scene/View 的恢复；Renderer 消费准备后的场景并提供可保存的 View requested 配置。JSON 类型停留在文件边界，Scene 不反向依赖 Renderer。Shader 编译、资产校验和性能采样在其实际消费模块内组织，不建立独立 Tools 模块。
- 格式化使用维护者现有 VS Code 设置。当前不加入仓库格式配置、CI、CTest 或测试样本；必要检查根据本次实际修改执行，测试组织留待维护者统一确定。
- OpenSpec 采用官方 1.13.2 与 bundled spec-driven schema，独立于 C++ 构建使用。工程不安装 Node.js/npm 或 OpenSpec；当前 change 与开发计划记录实际完成状态，不新增验证报告文件。

## Risks / Trade-offs

- 精简源码可能遗漏构建输入：以新目录的 Debug/Release 构建验证，不用全局屏蔽诊断掩盖问题。
- 当前尚无业务消费者：构建通过仅证明已编译第三方的源码可用，头文件库和业务集成在首次接入时验证，不扩大完成结论。
- 当前资源存在来源或许可未知项：在 Assets/README.md 如实登记，不推测授权或导入能力。
- JSON 场景方案当前只交付设计：实际解析、编辑与读写行为分别在 D06、D17 实现和验证。

## Implementation Order

先对齐格式和模块职责，再清理目录与构建入口，核对第三方记录，最后从干净目录验证现有构建并更新任务状态。
