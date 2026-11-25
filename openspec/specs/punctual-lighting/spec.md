# punctual-lighting Specification

## Purpose

规定静态场景的方向灯、点光和聚光如何成为每个 View 的完整直接光输入，定义源强度单位、影响范围、当前帧的资源交付以及共享的入射光求值合同。

## Requirements

### Requirement: Assemble the complete visible punctual light set

每个受光 View SHALL 从其冻结场景及保守可见性结果取得标准灯。启用的方向灯 SHALL 独立于局部灯集合。无 range 的点光和聚光 SHALL 保持无距离截断。有限 range 的局部灯 SHALL 按世界影响球保守筛选，保留跨入或接触视锥的候选。工作集 SHALL 保留线性颜色、方向灯 lux、局部灯 candela、有效世界位置或发射方向，以及源 range 和锥角语义。节点缩放影响变换后的姿态，强度、range 和锥角各自沿用源值。Forward SHALL 求值完整集合中的每盏灯一次。

#### Scenario: Preserve light influence outside the camera frustum

- **WHEN** 同一 View 观察到一个受光表面，有中心在视锥外而影响球跨入的有限灯、完全分离的有限灯、无截断的局部灯、方向灯和已隐藏的灯
- **THEN** 候选集合包含相交有限灯、无截断局部灯和方向灯，各灯只出现一次，完全分离及隐藏的灯被排除

### Requirement: Deliver the current light set to the shading pass

受光颜色输出 SHALL 读取与本次相机和场景对应的灯记录及有效数量。生成接口 SHALL 定义 CPU 和 Shader 共用的数据布局，图 SHALL 声明实际初始化与读取的范围及版本。无灯时 SHALL 使用零计数，前置深度保持原有资源及覆盖合同。灯表超出可绑定、可编码或可上传范围时 SHALL 报告原因，视口继续沿已接受输出和现有待确认或拒绝路径工作。只有完整、匹配且被正常图实际采样并接受的结果 SHALL 作为本次受光画面发布。

#### Scenario: Refresh a light set whose count has stayed the same

- **WHEN** 同一个已安全复用的帧槽观察同一 View，灯数量保持相同而强度或发射方向改变
- **THEN** 颜色读取新灯值和对应 View 常量，其他未回收槽仍使用各自的版本

#### Scenario: Retain the accepted output when the complete light set cannot be delivered

- **WHEN** 已有正常受光画面，而当前候选超出灯表容量或其匹配上传尚未接受，随后收到可完整交付的请求
- **THEN** 处理失败或等待期间保有上一接受结果及原因，后续可靠提交才确认完整新灯表产生的画面

### Requirement: Evaluate directional and local illuminance in standard units

方向灯 SHALL 用世界发射轴的反方向作为片元入射方向，从 lux 得到与位置无关的入射量。点光与聚光 SHALL 以片元到灯位的方向及米制距离求值，基础入射量为 `I/r²`。显式 range SHALL 使用 `max(1 − (r/R)^4, 0)`，无 range 时距离窗为 1。聚光 SHALL 在内锥保持全强度，在外锥及以外归零，两锥间按发射轴夹角的余弦差归一化后平方。每盏灯 SHALL 乘一次线性 RGB，局部灯与片元完全重合的无方向样点 SHALL 给出零直接贡献。

#### Scenario: Use directional illuminance independent of distance

- **WHEN** 方向灯的世界发射轴、线性 RGB 和 lux 固定，移动其节点位置，随后改变发射轴朝向
- **THEN** 位移不改变其入射量或方向，轴转向改变入射方向，照射到的片元按线性 RGB 得到各通道贡献

#### Scenario: Fall off by inverse square and respect a finite range

- **WHEN** 点光以相同 candela 分别在两倍距离上照射固定样点，并在另一帧指定有限 range
- **THEN** 无 range 时远样点入射量为近样点的四分之一，有限灯按距离窗衰减并在 range 边界及外侧归零

#### Scenario: Apply spot attenuation on the emitter side

- **WHEN** 聚光的光源和样点距离固定，仅旋转发射轴，使样点分别位于内锥、两端余弦的中点以及外锥外
- **THEN** 角衰减分别为 1、1/4 和 0，样点在灯轴背面时同样为零

### Requirement: Apply visibility to the matching direct light contribution

完整灯表的每盏灯 SHALL 从同一 View、Scene 及带 generation 的灯身份取得其可用阴影输入或中性对应。交付给光照的引用 SHALL 对应本轮有效灯行及已定义且获准查询的组。排序、灯表复用、区域复用和同一源槽的灯替换 SHALL 不把旧灯或另一盏灯的阴影交给当前灯。

共享直接光求值 SHALL 以对应查询的 visibility 对该灯的直接贡献加权，同时沿用其颜色、物理强度、距离、角度和表面响应约定。灯不具备阴影组或样点超出覆盖时 SHALL 以 1 求值该灯。IBL、自发光或其他灯的贡献保持各自的求值权重。

#### Scenario: Preserve independent contribution after a light mapping changes

- **WHEN** 一个接收表面同时由可区分颜色的聚光和点光照明，某盏灯被遮挡，随后改变灯表排列或用带新 generation 的灯复用原槽
- **THEN** 逐灯 HDR 差值对应当前实际受遮挡的那盏灯。获拒绝阴影预算的灯仍保有原本的衰减和颜色贡献，新灯按照当前配对的阴影输入求值。
