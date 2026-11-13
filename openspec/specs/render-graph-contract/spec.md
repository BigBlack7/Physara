# render-graph-contract Specification

## Purpose

定义帧内渲染资源如何声明身份、版本、内容和访问权限，使 Renderer 能在不触碰原生设备的前提下，把上传、视口采样、UI 合成和外部输出交给图编译器。

## Requirements

### Requirement: Keep typed resource versions local to their graph

图 SHALL 分别为纹理与缓冲提供不可互换的版本身份，包含资源索引、图代与逻辑版本。图 SHALL 在使用身份前检查其所属图、类型、索引及版本是否有效。默认值、其它图的身份或未发布的版本 SHALL 在 Debug 与 Release 中被拒绝，不能落到同一索引的另一资源上。图身份仅供当前图使用；跨帧数据 SHALL 通过外部资源边界重新导入。

#### Scenario: Reject a handle from a different graph generation

- **WHEN** 两张图拥有同类型、同索引和同版本号的资源，调用者将第一张图的身份交给第二张图，或传入默认身份
- **THEN** 第二张图拒绝本次声明或查询，原有资源不会被误读、改写或导出

### Requirement: Declare exact resource ranges and canonical imports

图 SHALL 持有创建或导入资源的描述，将每次用途解析为明确的 mip、layer、aspect 或缓冲字节区间。显式区间为空、越界、不可表示，或 aspect 与纹理格式不符时 SHALL 报告原因，不得裁成合法区间。导入 SHALL 持有外部 RHI 资源的引用，并明确允许范围、入口与结束状态及输入内容是否有效。同一外部资源的等价导入 SHALL 返回同一导入版本；同一资源重复导入时，范围、状态或内容声明冲突 SHALL 拒绝，不得另建独立身份。

入口和结束状态 SHALL 各自提供公共 RHI 定义的非零状态。遇到 `Unknown` 或未定义状态位，图 SHALL 报告资源及对应字段，并拒绝发布导入记录。

#### Scenario: Reuse only an equivalent import

- **WHEN** 同一外部资源以等价的规范范围和相同进出状态、内容声明重复导入，随后同一资源以冲突条件再导入
- **THEN** 等价请求返回原始导入版本，即使已有写入；冲突请求报告失败，图仍只有一份外部资源身份

#### Scenario: Reject a range that would need clipping

- **WHEN** 纹理访问的 mip、layer 或 aspect 超出声明允许范围，或缓冲访问只超出可用字节区间末端一字节
- **THEN** 合法边界内的范围可声明，越界请求不生成用途、写版本或导出记录

### Requirement: Record version producers and content effects

一次成功的写声明 SHALL 从该资源当前写版本产生一个新版本，记录唯一生产者、写入范围和前一版本，不隐式复制物理存储。读取 SHALL 指定已有版本，较新的声明不能将旧版读取重定向到新版。图 SHALL 记录部分写入与未受影响范围的区别。导入状态 SHALL NOT 被用作内容有效性证明。

附件 `Load` SHALL 表达对同一范围旧内容的读取；`Clear` 或明确完整覆盖 SHALL 定义所声明的范围，不要求旧内容；`Discard` 放弃旧内容，且只有后续明确完整写入才能定义该范围。局部保留式更新 SHALL 表达对原有效内容的依赖。保留结果的 Store SHALL NOT 将未写内容变为有效；`StoreDontCare` SHALL 使所声明范围的结果无效。声明时能够直接确定所需内容未定义的读取、Load 或导出 SHALL 被拒绝；涉及跨范围依赖和物理覆盖顺序的判断由后续编译消费这些记录。

后续局部写入 SHALL 只恢复其覆盖的范围。首发配置中的 `Clear` SHALL 要求纹理具有 `isRenderTarget`，包括以 `CopyDestination` 声明的清除。无法清除的目标 SHALL 在发布 Pass、写版本和导出记录前被拒绝。

#### Scenario: Record clear and load as distinct writes

- **WHEN** 未初始化的颜色范围被清除并保留为版本 1，合成 Pass 再以 Load 读取版本 1 并写入版本 2
- **THEN** 两个版本各有唯一生产者，只有合成记录前一内容读取；版本 1 仍可被显式引用，两次写入不创建另一份物理资源

#### Scenario: Preserve the initialized remainder of a partial update

- **WHEN** 字体 atlas 的已初始化 mip 收到局部矩形更新，同时其它 mip 未初始化
- **THEN** 更新记录该 mip 对旧有效内容的依赖；本次写入不能证明其它 mip 已初始化，且相同的局部更新不能让一张未初始化的 atlas 整 mip 可读

#### Scenario: Reject a read of known discarded or unwritten content

- **WHEN** 调用者读取或导出明确标为 StoreDontCare 的写入范围，或试图 Load 已声明为内容未定义的初始范围
- **THEN** 声明失败并指明资源、版本与原因；合法的清除或完整覆盖仍可作为该范围的首次内容生产者

### Requirement: Own deferred Pass data after immediate setup

图 SHALL 在声明 Pass 时立即完成 setup，且不得在声明中调用 execute。成功声明 SHALL 保持 execute 所需的值和载荷在该 Pass 的消费期间有效，不依赖 setup 局部变量或之后可被覆盖的源数据。图或其帧内 owner SHALL 在释放或复用承载存储前销毁这些拥有型对象。setup 或存储失败 SHALL 报告原因，且不得发布失败 Pass 的可用回调、写版本或导出记录。

#### Scenario: Consume owned values after setup returns

- **WHEN** setup 从局部输入提交拥有型 Pass 数据，setup 返回后原输入被修改或结束寿命
- **THEN** 声明时未调用 execute，随后受控消费得到原提交的值；放弃未消费的图也会释放其拥有型对象

#### Scenario: Reject an incomplete Pass without publishing a version

- **WHEN** Pass 的 setup 已暂存写入声明但后续用途不合法，或候选 Pass 数据无法纳入存储预算
- **THEN** 返回失败，不能消费该 Pass 或使用它尝试发布的版本，并回收未发布的拥有型数据

### Requirement: Restrict resource access to the current Pass declaration

Pass 的资源查询 SHALL 只解析其声明允许的资源类型、精确版本、用途和范围。请求范围 SHALL 完全落在当前 Pass 的一条同版本、同用途声明内。查询失败 SHALL 在解析物理资源之前返回可诊断结果；另一 Pass 的声明或同一 Pass 对其它用途、范围或版本的声明不能授权本次访问。

#### Scenario: Deny access outside the current Pass permissions

- **WHEN** 一个 Pass 仅声明采样纹理某一版本的一个子资源，并尝试访问该子资源、以同一身份取得写附件、扩大范围或读取未声明版本
- **THEN** 声明范围内的采样可解析，其余请求都被拒绝，解析器不收到被拒绝的请求

### Requirement: Describe editor frame dependencies without device work

图 SHALL 能用同一套声明表达纹理上传后的采样、UI 顶点及索引和常量范围的读写、窗口颜色的初始化与合成，以及一个指定版本、范围和外部用途的导出。声明过程 SHALL 只记录内容和用途，不进行原生资源创建、命令录制、提交或窗口操作。

#### Scenario: Declare the baseline upload and UI composition chain

- **WHEN** CPU 消费者声明诊断纹理和字体上传、UI 缓冲写入、窗口目标清除、随后采样和合成，并标记窗口输出
- **THEN** 记录显示各读取引用对应生产版本，合成保留明确的目标内容依赖，外部输出指向合成后的版本；无原生资源创建、命令或窗口操作
