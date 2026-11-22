# Spec Delta

## MODIFIED Requirements

### Requirement: Plan compatible physical storage within a transient budget

编译 SHALL 依据存活 Pass 的有序访问和图尾导出计算每项使用中的资源的首末使用。图内复用 SHALL 仅发生于存储描述和用法兼容、使用区间不重叠的瞬态资源之间。导入使用原对象，导入的外部存储不进入瞬态复用计划。复用后的逻辑资源仍须自行建立有效内容，物理对象的实际状态沿执行交接跟踪。

瞬态预算 SHALL 按计划中的不同物理槽累计对象数和逻辑存储字节。缓冲按字节容量，纹理按全存储的格式块、mip、层或深度与样本计量。不能表示的估算以及超出调用方字节或对象上限的计划 SHALL 返回含定位的预算错误。该预算约束计划用量，原生分配与执行的实际结果由执行器报告。

精确子资源及字节范围 SHALL 决定内容根、依赖及被裁剪的写入。物理槽的完整对象寿命 SHALL 由其实际保留的用途及图尾交接合并得到。被裁剪分支的用途 SHALL 不延长物理寿命、增添槽或把未定义内容变为有效。同一存储存在相交的实际使用区间时 SHALL 仍保留独立逻辑对象所需的槽。

#### Scenario: Reuse a compatible slot only after the last use

- **WHEN** 两个同描述的瞬态资源在有序计划中先后使用，另有一个同描述资源已在后者首用时处于使用中，或在此之前已写入且仍须保留到图尾
- **THEN** 先后使用的资源可共享物理槽，同时使用的资源分开分配，预算按各槽计一次

#### Scenario: Reject a plan that cannot fit the transient budget

- **WHEN** 存活资源需要的物理槽总规划字节超过提供的预算，或所需字节不可表示
- **THEN** 编译返回计划容量错误，不交付分配计划。字节数可表示且其它条件满足时，同一封存图以恰好足够的预算可重新编译

#### Scenario: Lower physical demand when only selected subresources survive

- **WHEN** 一个 mip/layer 区域被 ShaderRead 或 Readback 出口消费，另一不相交区域仅有无根写入，另有一项存活资源跨两个 Pass 保持使用
- **THEN** 只保留实际生产者与消费者，按完整槽各计一次字节和对象数。若仍需要的物理槽数比提供的对象上限多一个，编译失败且未触发设备创建
