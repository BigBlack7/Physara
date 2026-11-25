# view-preparation Spec Delta

## MODIFIED Requirements

### Requirement: Prepare shadow plans from the current per-view request

View SHALL 持有独立的会话阴影请求，默认关闭。请求 SHALL 由类型化入口校验和提交，支持开启状态、有限正的阴影距离及 1024、512、256 texel 边长档，缺省距离和边长为 50 m 和 1024 texel。过滤 SHALL 支持 Hard 与 PCF，缺省 Hard。每 View 逻辑阴影预算 SHALL 支持 4、8、16、32 MiB，缺省 32 MiB，有效额度同时受现有设备、层与项目预算约束。请求 SHALL 接受 1–4 个方向光级联、有限的透视对数权重 `[0,1]` 及过渡半宽比例 `[0,0.25]`，缺省依次为 4、0.5 和 0.05。过渡比例以相邻两段中较短的名义长度为基准，零比例表示直接切换。请求无效时 SHALL 报告原因并保留原请求。主 View SHALL 可从现有 Renderer 面板修改阴影开关、过滤、分辨率及逻辑预算，离屏 View 使用同一准备合同。

正式 View 准备 SHALL 从其已解析的稳定相机、可见灯和同轮共享 `SceneFrame` 交付匹配的计划或当前原因，并按实际显示分支解析所需阴影组。相机或布局不可准备时，当前 View SHALL 具有空的阴影工作集。计划不可用时，仍可用的相机、主可见集和完整直接光输入 SHALL 继续按正常准备合同交付。只改变一路 View 的阴影设置或相关布局时，独立的另一 View SHALL 保有各自的计划输入和版本。

#### Scenario: Prepare two distinct shadow requests from one frozen scene

- **WHEN** 主 View 和离屏 View 共享同一 Scene，分别使用有效相机及不同的阴影质量、预算、级数或开启状态，随后只修改其中一路请求
- **THEN** 共享场景提取保持一次，各路计划和理由对应其自身输入。另一路的区域、所需版本和请求可独立核对

#### Scenario: Reject a request and recover a usable layout

- **WHEN** 一个 View 已接受有效规划请求，随后分别收到不合法距离、越界级数或非法过滤/预算选项，且尺寸从无有效图像恢复到合法像素范围
- **THEN** 非法请求被整体拒绝且原请求保留，无效布局没有该轮阴影区域，恢复后的计划对应当前相机、尺寸和有效请求

#### Scenario: Reject offscreen attachments before publishing a replacement allocation

- **WHEN** 一个离屏 View 已交付点光六面，场景改用聚光且离屏尺寸超出普通 FXAA 所需的附件预算，并以相同输入再次准备
- **THEN** 无有效级联诊断可用时，首次拒绝按该 View 上次交付的六个区域结算，第二次拒绝的释放数为零且分配代稳定。尺寸恢复后从当前聚光输入重新准入，其余 View 的分配与颜色独立结算

#### Scenario: Report a directional plan failure alongside an attachment limit

- **WHEN** 离屏尺寸只容纳级联诊断，而试算或匹配的已缓存方向光计划报告空接收区间，或者当前规划因 CPU 存储分配失败而不可用
- **THEN** 离屏输出仍按附件超限拒绝且无新区域，阴影摘要同时保留实际方向光原因及规划拒绝计数。一次存储失败后可用原参数重新准备

### Requirement: Resolve the selected local shadow group for the current view

View SHALL 持有独立的会话诊断选择，初始使用普通画面。选择 SHALL 能在普通画面、方向光级联和一个聚光或点光之间切换。类型化请求 SHALL 以当前 View、Scene 与带 generation 的灯身份验收，拒绝外来、失效或类型错误的选择并保留原请求。主 View 和离屏 View SHALL 沿相同的身份与当前输入合同处理。场景文档继续沿 v1 合同读写。

正式准备 SHALL 从当前冻结 Scene 和匹配的 CPU 计划解析所选灯及完整的阴影区域。当该灯被删除、转为其它类型、失去覆盖、预算不足或阴影禁用时，当前诊断结果 SHALL 带可定位原因及空诊断深度组。普通显示仍可用时 SHALL 继续交付有效的相机及完整灯输入，并按普通分支的实际消费解析获准阴影。附件预检及阴影分配结算 SHALL 对应同一次实际显示配置与该 View 上次已交付的分配。

#### Scenario: Change one view selection and reject a stale light identity

- **WHEN** 同一 Scene 的主 View 和离屏 View 分别选择聚光和点光，随后其中一个灯被删除、其槽位由另一个对象复用，并收到旧灯选择请求
- **THEN** 原灯的诊断变为当前不可用，旧身份无法选中新对象。另一 View 的选择、计划及可接受输出继续独立解析

#### Scenario: Settle an offscreen layout with an unavailable local shadow group

- **WHEN** 一个离屏 View 请求所选局部灯的线性诊断，尺寸可容纳该诊断但超出其普通 FXAA 配置的附件预算，随后该灯失去阴影准入
- **THEN** 有完整组的候选按实际诊断配置准备。失去准入后，超限普通候选报告附件及阴影原因，原有规划区域按本 View 的已交付状态结算。重复拒绝不会重复释放，尺寸或请求恢复后按当前输入重新准备
