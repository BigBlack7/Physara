# Spec Delta

## Purpose

定义封存的编辑器 UI 帧如何经公共 RHI 产生离屏颜色输出，以及调用者可依赖的几何单位、纹理身份、内容确认和 GPU 在途寿命。使后续窗口呈现和图执行能够消费同一份 UI 绘制结果。

## ADDED Requirements

### Requirement: Compose a sealed UI frame on a supplied RHI target

已接入 renderer 的编辑器 SHALL 从本会话封存的 UI 帧准备自持绘制与上传数据，并通过公共 RHI 录制到调用者提供的目标。基线目标为已初始化的非零单色、单样本二维 sRGB 附件。调用者提供相同帧对应的实际像素范围并拥有提交和目标用途。初始化 SHALL 校验 UI Shader 接口，缺失资源、超出预算或不兼容目标 SHALL 有界报告。无新帧或有效像素范围为零时 SHALL 返回无工作。

#### Scenario: Record a sealed editor frame into an offscreen target
- **WHEN** 已接入 renderer 的会话封存一帧，调用者提供兼容的离屏目标和开放的图形命令列表
- **THEN** 录制使用本帧几何和纹理依赖，在已有底色上合成。调用者提交有效工作后可观察目标中的 UI 像素

#### Scenario: Reject incompatible rendering inputs
- **WHEN** UI 程序接口不匹配，或准备使用不兼容的非零目标或超预算输入
- **THEN** 返回指明受影响阶段的有界失败，不发布可提交的 UI 绘制结果

#### Scenario: Cancel a partial preparation on CPU allocation failure
- **WHEN** 批次建立或临时纹理身份已安装后的准备阶段发生 CPU 分配失败
- **THEN** 返回有界失败，撤回本次创建的临时身份，保留待处理请求和像素供后续重试

#### Scenario: Stop after rejection of a draw's graphics state
- **WHEN** 同一批中先有成功绘制，后续命令设置图形状态时收到同步 RHI 错误
- **THEN** 后续命令不发出该次索引绘制，保留已发前缀及其提交或收拢责任

#### Scenario: Leave a paused iteration without UI work
- **WHEN** 窗口暂停而没有新 CPU UI 帧，或本轮有效像素范围为零
- **THEN** UI renderer 不发出本轮绘制或上传，不分配替代目标。待处理纹理请求可由后续有效帧继续处理

### Requirement: Preserve UI coordinate and index semantics

UI 布局 SHALL 在 `DisplayPos` 和 `DisplaySize` 定义的逻辑空间投影。裁剪 SHALL 以该帧的 `FramebufferScale` 转到实际像素范围并夹取，空裁剪 SHALL 跳过。索引命令 SHALL 结合每列表基址、局部索引和顶点偏移及实际 16 位或 32 位索引宽度，受影响的范围与数值转换 SHALL 在录制前校验。

#### Scenario: Clip using nonzero logical origin and framebuffer scale
- **WHEN** 非零 `DisplayPos` 与非等比 framebuffer 比例映射带有部分越界和空裁剪的绘制命令
- **THEN** 实际目标在对应像素内出现 UI，目标范围以外被夹取，空裁剪不改变底色，行序与逻辑左上原点一致

#### Scenario: Draw multiple lists with nonzero local offsets
- **WHEN** 多个 draw list 含非零索引偏移和 base vertex，并分别以一致的 16 位或 32 位索引配置执行
- **THEN** 两种宽度都从指定范围获取顶点与索引并得到相同的预期图案，干扰前后缀不参与绘制

### Requirement: Dispatch reset-state commands separately from draws

Renderer SHALL 将已声明的 reset-state 回调解释为图形状态重建命令。此命令 SHALL 结束当前命令的处理，纹理解析和索引绘制只属于普通 draw。超出已声明能力的回调 SHALL 在准备时报告并拒绝该批。

#### Scenario: Reset between ordinary draw commands
- **WHEN** 两条有效 draw 之间出现没有可采样纹理的 reset-state 命令
- **THEN** reset 命令不取该命令的纹理或发出三角形，后一条 draw 在重建的 UI 状态下产生正确像素

#### Scenario: Reject an undeclared user callback
- **WHEN** UI 帧包含未声明资源和状态影响的任意回调
- **THEN** 准备返回可诊断的失败，不调用该回调或录制该 UI 批次

### Requirement: Composite straight-alpha UI in the linear display domain

顶点 RGB SHALL 按 sRGB 创作值解码，颜色图标 SHALL 按 sRGB 纹理语义采样，display-linear 输入以线性颜色参与运算。字体 alpha SHALL 表示覆盖率。UI 片元 SHALL 以直通 alpha 合成，RGB 使用 `SrcAlpha/OneMinusSrcAlpha`，alpha 使用 `One/OneMinusSrcAlpha`。sRGB 附件 SHALL 在存储时编码一次。

#### Scenario: Blend a tinted color icon on a known background
- **WHEN** 一块半透明、含 sRGB 创作顶点色和 sRGB 纹理颜色的 UI 覆盖已知底色
- **THEN** 像素的存储 RGB 和 alpha 符合独立推导的线性合成及单次输出编码，未覆盖区域保留底色

#### Scenario: Preserve font coverage at glyph edges
- **WHEN** 实际字体 atlas 被 UI 文本采样，字形边缘含部分覆盖 texel
- **THEN** 边缘的混合符合原覆盖率，透明处保留底色，覆盖率不会被颜色解码改变

### Requirement: Confirm texture requests only after their upload is accepted

Renderer SHALL 独立持有本次 create/update 所需的像素及范围。临时 `TexID` 可以供该次准备解析，内容有效状态 SHALL 在上传实际执行、执行区间成功且同批获有效提交后确认。取消、执行或提交失败 SHALL 保留请求和 CPU 数据，清理未发布的临时身份。已发的失败前缀沿实际提交退休或受控退出收拢。

#### Scenario: Publish a managed texture after its first accepted upload
- **WHEN** `WantCreate` 准备出一张供同帧 draw 采样的纹理
- **THEN** 同帧先上传再采样，只有该批获有效提交后才设置 `OK`。提交前的 `TexID` 只供这次准备解析

#### Scenario: Apply an independently owned texture update
- **WHEN** `WantUpdates` 提供一个已验证的更新区域，准备完成后源像素借用结束
- **THEN** 实际命令消费本次拥有的区域字节，受影响 texel 按新内容采样，区域外像素保留，更新前后使用同一有效纹理身份

#### Scenario: Stop recording at a rejected upload
- **WHEN** RHI 同步拒绝本批所需的暂存纹理或几何缓冲写入
- **THEN** 录制在拒绝处停止，依赖该写入的复制或绘制不发出。暂存写入被拒绝时对应驻留区域保留原内容，请求保持待处理，已发前缀沿真实提交或受控退出退休

#### Scenario: Keep a request pending after cancelled or failed work
- **WHEN** 上传被取消、录制或执行区间报告失败，或该批未获有效提交
- **THEN** 请求保持待处理，临时创建身份失效。即使已发前缀取得票据也只用于保护依赖，下一次安全尝试仍可处理请求

### Requirement: Resolve and retire versioned UI texture references

UI 纹理 SHALL 使用可校验会话和代的稳定逻辑身份，并解析为当前工作使用的纹理和采样绑定。过期的非基础图像引用 SHALL 产生可辨认的占位和诊断。删除请求 SHALL 结束新解析，之前已发工作引用的纹理及绑定 SHALL 保留至实际最后使用的完成或受控退出，正常帧使用有界资源并避免隐式全设备等待。

#### Scenario: Reject a stale texture token after slot reuse
- **WHEN** 已删除的纹理槽位被新代复用，后续 UI 帧仍引用旧 token
- **THEN** 旧 token 只能得到有诊断的占位，新代正常采样自己的像素

#### Scenario: Retire a texture referenced by an earlier submission
- **WHEN** 先前的有效提交已引用一张纹理，随后收到删除或 atlas 重建请求并释放 CPU 句柄
- **THEN** 新帧不再解析旧代，已发工作的引用保持到实际退休，后续有效代可继续绘制，关闭会话沿同一设备寿命收拢
