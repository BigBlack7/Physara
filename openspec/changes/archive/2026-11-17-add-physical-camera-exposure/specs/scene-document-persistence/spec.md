# Scene Document Persistence Spec Delta

## MODIFIED Requirements

### Requirement: Persist only the main View's supported requests

主 View SHALL 提供可持久化请求与对应 revision。v1 `mainView` SHALL 保存导航 `cameraPose`、可空 `cameraEntityId` 和 `requested.renderScale`。实体相机引用 SHALL 指向此文档中含相机组件的实体，为空时提供导航请求。持久化请求恢复后的有效输出 SHALL 继续遵循 View 的准备和发布合同。

v1 `mainView.requested.physicalCamera` SHALL 为可省略的物理请求对象。缺少对象时 SHALL 使用与新建主 View 相同的物理缺省值。对象存在时 SHALL 要求完整的有限数值字段 `fNumber`、`shutterSeconds`、`iso`、`exposureCompensationEv`、`sensorHeightMeters`、`navigationFocalLengthMeters` 和 `focusDistanceMeters`，且通过 View 的物理请求校验。保存 SHALL 写入已接受的完整请求。派生 FOV、EV100、预曝光和最后渲染值 SHALL 从其所属运行期状态获得。

额外 View、窗口设置、布局、选择、运行期 handle、编辑历史及有效渲染输出 SHALL 保持为各自运行期状态。

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
