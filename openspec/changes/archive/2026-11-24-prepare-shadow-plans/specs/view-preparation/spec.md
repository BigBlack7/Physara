# View Preparation Spec Delta

## ADDED Requirements

### Requirement: Prepare shadow plans from the current per-view request

View SHALL 持有独立的会话阴影请求，默认关闭。请求 SHALL 由类型化入口校验和提交，支持开启状态、有限正的阴影距离及 1024、512、256 texel 边长档，缺省距离和边长为 50 m 和 1024 texel。请求无效时 SHALL 报告原因并保留原请求。主 View SHALL 可从现有 Renderer 面板开启或关闭 CPU 阴影规划，离屏 View 使用同一准备合同。

正式 View 准备 SHALL 从其已解析的稳定相机、可见灯和同轮共享 `SceneFrame` 交付匹配的计划或当前原因。相机或布局不可准备时，当前 View SHALL 具有空的阴影工作集。计划不可用时，仍可用的相机、主可见集和完整直接光输入 SHALL 继续按正常准备合同交付。只改变一路 View 的阴影设置或相关布局时，独立的另一 View SHALL 保有各自的计划输入和版本。

#### Scenario: Prepare two distinct shadow requests from one frozen scene

- **WHEN** 主 View 和离屏 View 共享同一 Scene，分别使用有效相机及不同的阴影质量或开启状态，随后只修改其中一路请求
- **THEN** 共享场景提取保持一次，各路计划和理由对应其自身输入；另一路的区域、所需版本和请求可独立核对

#### Scenario: Reject a request and recover a usable layout

- **WHEN** 一个 View 已接受有效规划请求，随后收到不合法距离，且尺寸从无有效图像恢复到合法像素范围
- **THEN** 非法距离被拒绝且原请求保留，无效布局没有该轮阴影区域，恢复后的计划对应当前相机、尺寸和有效请求
