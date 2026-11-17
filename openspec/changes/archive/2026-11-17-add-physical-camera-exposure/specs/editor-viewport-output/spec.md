# Editor Viewport Output Spec Delta

## ADDED Requirements

### Requirement: Record the pre-exposure of the last accepted scene render

View SHALL 分别提供当前合法请求的曝光计算结果和最后成功渲染的预曝光。首次场景渲染确认前，后者 SHALL 显示为尚未确认。确认时 SHALL 使用本次实际渲染相机快照中的曝光，且场景输出与当前 View 请求、有效布局、场景和预览模式匹配，在完整正常图中被实际采样并获得可靠的 Graphics 提交接受。曝光及相机状态 SHALL 随同一结果确认。

不可准备、待提交、陈旧候选、安全 UI 旧图或测试图 SHALL 保留最后成功渲染的记录。继续显示旧结果时 SHALL 标明它所属的旧有效输出或场景状态。单纯改变曝光 SHALL 与几何投影的变化独立表达。

#### Scenario: Keep the last rendered exposure while a new request is pending

- **WHEN** 场景以上次预曝光成功提交，用户仅调整曝光，先遇到等待或安全 UI，后一正常帧实际采样新场景结果并获接受
- **THEN** 新曝光请求先可见，等待或回退时最后渲染值仍对应旧帧，新的可靠接受才同时确认相机和本次预曝光

#### Scenario: Reject a late scene result from an older camera request

- **WHEN** 一个场景候选已准备，之后 View 接受另一物理相机请求，随后旧候选尝试确认
- **THEN** 旧候选失去当前场景结果资格，仍然由后续匹配且成功接受的结果发布最后渲染值
