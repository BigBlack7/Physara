# Scene Document Persistence Delta

## MODIFIED Requirements

### Requirement: Persist only the main View's supported requests

主 View SHALL 提供可持久化请求与对应 revision。v1 `mainView` SHALL 保存导航 `cameraPose`、可空 `cameraEntityId` 和 `requested.renderScale`。实体相机引用 SHALL 指向此文档中含相机组件的实体，为空时提供导航请求。持久化请求恢复后的有效输出 SHALL 继续遵循 View 的准备和发布合同。

v1 `mainView.requested.physicalCamera` SHALL 为可省略的物理请求对象。缺少对象时 SHALL 使用与新建主 View 相同的物理缺省值。对象存在时 SHALL 要求完整的有限数值字段 `fNumber`、`shutterSeconds`、`iso`、`exposureCompensationEv`、`sensorHeightMeters`、`navigationFocalLengthMeters` 和 `focusDistanceMeters`，且通过 View 的物理请求校验。保存 SHALL 写入已接受的完整请求。派生 FOV、EV100、预曝光和最后渲染值 SHALL 从其所属运行期状态获得。

v1 `mainView.requested.antiAliasing` SHALL 是可省略的字符串，支持 `none` 和 `fxaa`，缺省为新建主 View 的 None。保存 SHALL 捕获有效请求的值，允许省略缺省的 `none`。显式未知值、`null` 或类型不符 SHALL 定位到该字段并拒绝整个 Open 候选。显示 profile、实际 AA 和渲染及输出像素布局 SHALL 从当前准备与成功接受的帧获得。

v1 `mainView.requested.opaquePath` SHALL 是可省略的字符串，支持 `forward` 和 `forward-plus`，缺省为新建主 View 的 Forward。保存 SHALL 捕获已接受的路径请求，允许省略缺省的 `forward`。显式未知值、`null` 或类型不符 SHALL 定位到该字段并拒绝整个 Open 候选。打开成功时 SHALL 随完整文档恢复请求，实际有效路径由随后匹配的正常受光输出确认。

额外 View、窗口设置、布局、选择、运行期 handle、编辑历史及有效渲染输出 SHALL 保持为各自运行期状态。待选计划、准备诊断、有效路径和历史资格 SHALL 由运行期帧准备与接受合同管理。

#### Scenario: Round-trip a requested value that is still pending
- **WHEN** 主 View 已接收不同于当前有效比率的请求，并选择场景中的相机实体后保存并重开
- **THEN** 主 View 恢复保存的请求比率、导航位姿和可解析的相机实体引用，实际有效值继续按对应输出的接受结果发布

#### Scenario: Reject an invalid main View camera reference
- **WHEN** 文件的 `cameraEntityId` 指向缺失实体或没有相机组件的实体
- **THEN** 候选整体失败，当前活动 Scene、主 View 请求和路径保持有效

#### Scenario: Open a v1 document that predates physical camera settings

- **WHEN** 现有 v1 文件缺少 `physicalCamera`，而 `mainView` 的其余字段合法
- **THEN** 场景和主 View 作为完整候选恢复，保留原相机选择、导航位姿和 render scale，并采用新建主 View 的物理缺省值

#### Scenario: Restore the exact physical camera request after saving

- **WHEN** 主 View 的合法非默认镜头和曝光请求已生效，其新渲染结果尚未确认，用户保存后重开
- **THEN** 文档恢复捕获时的七个请求值及其单位语义，由准备合同重新计算派生值，并由新的匹配场景结果确认渲染状态

#### Scenario: Reject an explicitly invalid physical camera object

- **WHEN** v1 文件的 `physicalCamera` 是 `null`、字段缺失、带未知字段或其物理数值无法接受
- **THEN** 打开失败并给出文件与相关字段原因，原活动 Scene、主 View 请求、路径及编辑状态保持有效

#### Scenario: Restore FXAA from a v1 request before a frame is accepted

- **WHEN** 用户先打开省略 `antiAliasing` 的合法 v1 文件，再选择 FXAA，在新场景帧仍待确认时保存并重开
- **THEN** 首次打开采用 None，重开恢复捕获的 FXAA 请求；实际 AA 由重开后第一次匹配的正常场景结果确认

#### Scenario: Reject an unsupported AA field without publishing the document

- **WHEN** 待打开文件将主 View 的 `antiAliasing` 指定为 `null` 或未支持的值
- **THEN** 文件和该字段的原因可见，原 Scene、主 View 请求、活动路径和已接受输出继续有效

#### Scenario: Restore an opaque path request independently of the accepted path

- **WHEN** 用户打开缺少 `opaquePath` 的 v1 文档，请求 Forward+ 后在有效路径仍为 Forward 时保存并重开
- **THEN** 初次打开的请求为 Forward，重开后恢复保存的 Forward+ 请求并在有效路径尚未确认时显示真实状态。首次有效受光结果使用重开后的当前输入确认

#### Scenario: Reject an invalid v1 opaque path without publishing the document

- **WHEN** v1 文件的 `opaquePath` 是 `null`、非字符串或超出支持值域
- **THEN** Open 诊断包含对应文件及 `mainView.requested.opaquePath`，原活动场景、文档路径、主 View 请求和接受记录依照失败恢复合同继续有效
