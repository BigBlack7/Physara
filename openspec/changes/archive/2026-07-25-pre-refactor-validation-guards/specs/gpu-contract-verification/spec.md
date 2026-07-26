## ADDED Requirements

### Requirement: CPU 结构布局编译期锁定

系统 SHALL 对每个 GPU 契约结构(`CameraData`、`ObjectData`、`MaterialGPUData`、`LightData`、`ShadowData`、`IBLData`、`ClusterGridData`、`FrameUniforms`)提供编译期布局断言(`sizeof` 与逐字段 `offsetof`),使字段顺序、类型或大小的任何意外改动导致**编译失败**而非静默漂移。

#### Scenario: 字段重排或改型触发编译失败
- **WHEN** 某 GPU 结构的字段被重排、增删或改变类型/大小
- **THEN** 编译期 `static_assert` 失败,并给出违例的结构与字段位置

#### Scenario: 有意布局变更需同步更新断言
- **WHEN** 开发者有意重设计某结构布局(如模块 4.2)
- **THEN** 必须同步更新对应 `offsetof`/`sizeof` 断言,断言通过方可编译

### Requirement: CPU↔GLSL 结构一致性校验

`Tools/verify_gpu_contracts.py` SHALL 解析 GLSL 镜像结构(`Common.glsl`、`FrameUniforms.glsl`、`Material.glsl`)的字段序列与类型,并与 C++ 侧结构逐字段比对;不一致时 MUST 以非零退出码失败并报告差异。

#### Scenario: GLSL 与 CPU 字段分歧被拦截
- **WHEN** 某 GLSL 结构的字段序列/类型与其 C++ 对应结构不一致
- **THEN** 校验脚本失败,输出结构名、分歧字段与两侧类型

#### Scenario: 解析失败不得静默放过
- **WHEN** 脚本无法解析某个受检结构
- **THEN** 校验以失败告终(fail-loud),而非跳过该结构

### Requirement: binding 号一致性校验

校验 SHALL 确认 GLSL `PHYSARA_BINDING_*` 定义值与 C++ `GPUBufferBinding`/`GPUTextureBinding` 枚举值一一对应;并 SHALL 报告未被任何 GLSL 引用的死枚举与被多义重载的 binding 号。

#### Scenario: binding 号错位被拦截
- **WHEN** 某 `PHYSARA_BINDING_*` 值与对应 C++ 枚举值不等
- **THEN** 校验失败并指出该 binding 的两侧取值

#### Scenario: 报告死枚举与重载
- **WHEN** 存在未被 GLSL 引用的 binding 枚举(如 `Shadow=6`/`IBL=7`)或同值多义(如 `binding=4`)
- **THEN** 校验输出告警清单供清理

### Requirement: GPU 结构布局纪律不变量

校验 SHALL 断言所有 GPU 契约结构仅由 `vec4`/`mat4`/`uvec4`(及等价 16 字节对齐成员)构成;出现 `float`/`vec2`/`vec3`/标量数组等 std140 高危成员时 MUST 告警。

#### Scenario: 引入 std140 高危成员被告警
- **WHEN** 某 GPU 结构新增了非 16 字节对齐友好的成员(如 `vec3` 或标量数组)
- **THEN** 校验输出该成员为 std140/std430 对齐风险点

### Requirement: 契约校验作为布局变更前置门

涉及 GPU 数据布局或 binding 变更的任务(尤其模块 4.2)MUST 在校验通过后方可推进;校验为该类变更的强制 gate。

#### Scenario: 布局变更未过校验则阻断
- **WHEN** 一次改动修改了 GPU 结构布局或 binding 但契约校验未通过
- **THEN** 该改动被视为未完成,必须先修复至校验通过
