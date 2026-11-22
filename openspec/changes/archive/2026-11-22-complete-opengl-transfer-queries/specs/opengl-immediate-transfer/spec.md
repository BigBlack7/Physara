# Spec Delta

## Purpose

定义 OpenGL Graphics 即时列表上的有限颜色 mip 生成、颜色 resolve、Buffer 与 Texture staging。调用者可按实际格式、子资源、字节范围和行布局传输数据，并在原录制被接受且真实完成后非阻塞读取对应结果。

## ADDED Requirements

### Requirement: Generate only the selected color mip chain

已交付 profile 中，mip 生成 SHALL 在拥有当前 context 的 Graphics 即时录制内，以已定义的选定基础 mip 为输入，顺序生成同一范围内的下级 mip。每个选中二维层或 cube 面 SHALL 独立过滤。原基础 mip、未选 mip/层和纹理视图解释 SHALL 保持。颜色过滤 SHALL 在线性值上执行，sRGB 存储按其约定解码和编码。有效的一层 mip 范围 SHALL 为无写入的操作。范围、用途、格式或样本不受支持的请求 SHALL 在修改前明确失败。

#### Scenario: Generate from an offset mip on one cube face
- **WHEN** 选中的基础 mip 有可判定的非均匀颜色，前后 mip 或相邻 cube 面含不同哨兵
- **THEN** 目标层的各 mip 具有按前一级过滤得到的值和尺寸，基础 mip 与选择之外的哨兵保持，后继采样可见新 mip

#### Scenario: Interpret encoded color at the filtering boundary
- **WHEN** 在受支持的 sRGB 子资源上生成较低一级 mip
- **THEN** 新 mip 的编码表示线性空间的过滤结果，既有采样继续按 sRGB 解释，普通 copy 和 staging 保留该编码

### Requirement: Resolve matching multisample color subresources

`ResolveTexture` SHALL 只对已交付且实际受支持的同格式颜色组合执行从 Texture2DMS 到 Texture2D 或从 Texture2DMSArray 到 Texture2DArray 的转换。源与目标 SHALL 成对选中等尺寸且数量相同的子资源，源选其唯一 mip，目标可选合法的单个 mip。每个选层的全部颜色样本 SHALL 通过原生 resolve 语义生成单样本颜色，结果保持逻辑行序与编码。目标其余范围和源 SHALL 保持。残留 scissor 或其它无关 Draw 状态 SHALL 不改变覆盖。无效样本、深度/模板、整型、自定义取样规则、缩放及不兼容配对 SHALL 明确拒绝。

#### Scenario: Resolve selected array layers at a matching destination mip
- **WHEN** 在合法样本数的非方形多样本源上用公开 clear 定义两层不同颜色，再配对 resolve 到预置哨兵的单样本数组中的选层或非零 mip
- **THEN** 原生执行逐层 resolve，读回的选层与对应已知颜色及范围一致，未选层/mip 保持其原值，后续 Draw 使用正确区域状态

#### Scenario: Reject unsupported resolve before modifying the destination
- **WHEN** 使用实际样本集合之外的描述，或对分配合法但格式、样本、范围或尺寸组合不支持的对象请求 resolve
- **THEN** 有相应的创建或调用诊断，受影响 resolve 未发出，已有目标数据保持

### Requirement: Transfer exact staging buffer and texture regions

同设备的专用 staging Buffer 和 StagingTexture SHALL 固定为 CPU Read 或 CPU Write，分别支持普通 GPU 资源到 Read staging、Write staging 到普通 GPU 资源的复制。CPU 写入经成功 Unmap 后方可用于 GPU 复制。Buffer SHALL 使用公开的 byte range。Texture SHALL 使用验证过的等格式、单样本一对一矩形和指定 mip/layer，包含已采用的二维、数组及 cube 选面。staging 的缓冲区方向、范围、当前映射及在途占用 SHALL 在发出复制前满足合同。

staging 布局 SHALL 以所选子资源的实际行间距与像素/块宽解释本地偏移。受支持的块格式 SHALL 以压缩块为单位保留原始块数据和已公布的区域限制。普通颜色传输 SHALL 保持分量类型、sRGB 存储编码和逻辑行序。只有目标请求覆盖范围 SHALL 被写入，调用者按其实际定义的范围读取数据。相同容量或相同字节数 SHALL 不赋予格式转换、边界裁切、样本转换或颜色重采样权限。

#### Scenario: Round trip an asymmetric staging region
- **WHEN** CPU 填入带 padding 和哨兵的 Write staging，经 Unmap 将合法非零 byte range 和二维像素矩形复制到普通资源，再复制到 Read staging
- **THEN** 映射观察到匹配的字节、已知整数或颜色值及逻辑行序，所选层/mip 与其它已知区分离，行 padding 未被当作 texel

#### Scenario: Preserve actual compressed block rows
- **WHEN** 对采用的块压缩格式传输含尾部不满一个块的完整二维 mip
- **THEN** 按照实际块行和返回的 row pitch 可重构输入块数据，超出公布格式/范围的复制在修改前失败

### Requirement: Map only a completed readback generation

CPU 读回 SHALL 将实际复制与接受该录制的有效提交关联。读回调用者 SHALL 在接受提交后捕获该提交的 Event 快照，在后续非阻塞 Poll 真正完成后消费相应 staging 数据。未捕获工作的零快照、无关提交、Timer Ready 或其他包的退休 SHALL 不代替该关联。底层 Map SHALL 独立拒绝尚未接受、未完成、已失效或与本次方向/子资源不符的 Read 数据，保持地址与 pitch 的输出有效性。等待当前工作 SHALL 由明确的提交观察协议承担。

成功 Map SHALL 建立到匹配 Unmap 的单一 CPU 借用，返回整个 Buffer 或选定单个 mip/layer 起始处，以及纹理相邻像素行或块行的真实字节步长。调用者 SHALL 持有 owner 并仅消费其本次已定义的字节或 texel。设备 SHALL 拒绝重复 Map、错误 Unmap 及与借用冲突的 GPU 复制。失败 Map SHALL 返回空并保留调用方原 `outRowPitch`。仍可用且未完成的读回 SHALL 能在稍后按同一身份重试。

#### Scenario: Leave a new generation unmappable until it completes
- **WHEN** staging 已有历史成功结果，随后为其选定范围发出一次新复制，并在正常提交前、或该提交还未完成时尝试 Read Map
- **THEN** 无法借到未完成或历史数据的指针，pitch 输出保持，未发生隐式等待；观察到本次完成后 Map 得到新值与实际 pitch

#### Scenario: Map only the caller's live staging use
- **WHEN** 调用者持有成功映射，并请求重复 Map、错误方向或复用同一 staging 的冲突复制
- **THEN** 受影响请求失败，已有借用和有效数据不会因此被替换或提前复用；合法 Unmap 后可按下一次使用条件继续

### Requirement: Retire staging and cancelled work by the real completion authority

已发传输 SHALL 从录制开始保留实际资源和存储，按原工作包的接受提交与真实退休或受控退出解除。调用者取消需求或复位观察者 SHALL 只撤销该需求的结果消费。放弃录制、无有效提交、实际失败或 context 失效时 SHALL 无可发布的本次读回结果，已发前缀仍 SHALL 保持到安全退休。正常工作和退出 SHALL 区分真正的外部 owner、CPU 映射和后端内部保活。

staging 分配及仍未归还的原生占用 SHALL 有有限字节和对象预算。耗尽 SHALL 在新分配前诊断，保持前次使用及其有效性。最后一个外部句柄释放 SHALL 不替代 GPU 退休。原生清理 SHALL 遵守 owner thread/current context，确认原生环境不可用时只沿既有 CPU 收拢合同退出。

#### Scenario: Cancel a pending result and later reuse the staging capacity
- **WHEN** 复制已发出但结果被放弃，调用者释放观察者和资源句柄，设备继续收集或按规定退出
- **THEN** 不交付该次应用结果，内部保活延续到同一工作证明可退休，预算只在实际归还后释放，未取得有效提交的前缀不发表正常完成身份

#### Scenario: Exhaust staging capacity with retained work
- **WHEN** 分配及尚待退休存储达到声明预算，再请求一份超限 staging 后完成已有工作并释放旧 owner
- **THEN** 超限请求明确失败且没有隐式等待或破坏旧结果，安全收拢后可在释放的预算内重新分配

### Requirement: Synchronize transfer with the real producer and consumer domains

普通资源的 clear、copy、mip、resolve 和 staging 复制 SHALL 在其实际范围使用对应读写用途，并把依赖交给同一 RHI 发射权威。Automatic 推导用途，Explicit 和允许的 GraphManaged 复制 SHALL 消费各自已准备的精确权限。transfer 消费 Shader 写入 SHALL 满足其实际域的可见性。后继重叠 Shader 写 SHALL 满足先前传输读写的执行次序。已同步过的采样或其它域、相同抽象状态及原生缓存命中 SHALL 不替代该依赖。偏移部分写入 SHALL 保留未覆写区间的访问事实。

传输使用的 pack/unpack、附件、scissor、颜色编码等原生状态 SHALL 与后续 Draw、Dispatch 和上传所依赖的状态一致。合法即时命令在 recording 发出一次，Execute 接受提交并跟踪完成。

#### Scenario: Observe real mixed consumers of the same shader write
- **WHEN** 一次 Compute 产生已知 image 与 Buffer，Draw 先对选中纹理层采样，后续 transfer 继续复制源结果及 Buffer 区间，再对重叠区间发出 Shader 写
- **THEN** 采样、传输和后继结果按声明顺序观察对应版本，实际依赖按消费域和范围发出，读取发生在其应观察的覆写前

#### Scenario: Resume ordinary draw and upload after transfer
- **WHEN** 一次录制把受限制区域的 Draw、staging pack/unpack 或 mip/resolve 与后续合法 Draw 和上传交错
- **THEN** 后续使用获得正确状态、数据和范围，传输选择与图形缓存的前次命中互不改变实发结果
