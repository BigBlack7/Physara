# Spec Delta

## ADDED Requirements

### Requirement: Confirm the effective environment with the accepted scene render

请求的环境 SHALL 与最近一次成功渲染实际使用的环境分开呈现。状态 SHALL 可区分场景未配置环境、当前未消费所声明的用途、等待准备或提交、拒绝以及已接受的有效用途。已接受结果 SHALL 对应当前帧实际使用的资源代、标定、用途和查询参数，并与该帧的场景、View、模式及曝光一并确认。缺少可用候选时 SHALL 沿现有安全 UI 选择并标明上次有效图像或带理由占位。安全 UI、过期请求及匹配失败的候选 SHALL 不推进已接受环境记录。

#### Scenario: Show which environment produced the last accepted image
- **WHEN** 环境 A 的 Physical Lighting 已接受，随后打开带 B 的场景，B 先等待预计算或上传，后由正常图实际使用并获得可靠提交
- **THEN** 等待期间界面显示 B 的请求及原因，旧图像若被使用则仍标明 A，B 完整确认后界面和图像表示同一新代
