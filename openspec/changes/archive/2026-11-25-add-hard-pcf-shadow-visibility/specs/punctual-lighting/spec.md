# punctual-lighting Spec Delta

## ADDED Requirements

### Requirement: Apply visibility to the matching direct light contribution

完整灯表的每盏灯 SHALL 从同一 View、Scene 及带 generation 的灯身份取得其可用阴影输入或中性对应。交付给光照的引用 SHALL 对应本轮有效灯行及已定义且获准查询的组。排序、灯表复用、区域复用和同一源槽的灯替换 SHALL 不把旧灯或另一盏灯的阴影交给当前灯。

共享直接光求值 SHALL 以对应查询的 visibility 对该灯的直接贡献加权，同时沿用其颜色、物理强度、距离、角度和表面响应约定。灯不具备阴影组或样点超出覆盖时 SHALL 以 1 求值该灯。IBL、自发光或其他灯的贡献保持各自的求值权重。

#### Scenario: Preserve independent contribution after a light mapping changes

- **WHEN** 一个接收表面同时由可区分颜色的聚光和点光照明，某盏灯被遮挡，随后改变灯表排列或用带新 generation 的灯复用原槽
- **THEN** 逐灯 HDR 差值对应当前实际受遮挡的那盏灯。获拒绝阴影预算的灯仍保有原本的衰减和颜色贡献，新灯按照当前配对的阴影输入求值。
