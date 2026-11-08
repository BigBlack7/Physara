# Spec Delta

## MODIFIED Requirements

### Requirement: Create immutable binding sets with exact resource ownership and views

绑定集 SHALL 对应同一设备的常规布局, 按布局核对独立资源类型、slot 与范围, 保存不可变描述及预解析视图/range. Buffer range SHALL 满足资源用途、已知记录粒度、UBO/SSBO offset 对齐和实际设备限制; Texture SRV SHALL 保留所请求的兼容二维视图、format、mip/layer 范围和分量映射. 不可用或不兼容的资源/视图 SHALL 拒绝. 绑定集 SHALL 强持有布局与实际资源依赖; trackLiveness SHALL 保持其公开的命令列表保活含义, 不改变集合自身的依赖所有权.

Buffer 绑定的 `byteSize=0` 与全一长度哨兵 SHALL 按公共范围语义解释为从合法 byteOffset 到 Buffer 末尾, 与该剩余长度的显式表示等价; 解析后的范围仍 SHALL 非空并满足既有约束. 显式越界起点或长度 SHALL 保持拒绝, 不因范围解析会裁定到容量而将原始非法请求变成成功. Debug/Release 和 raw/Validation 消费 SHALL 保持同一合法范围语义.

#### Scenario: Retain a valid set after caller releases dependencies
- **WHEN** 绑定集从合法常量、结构化 buffer、纹理视图与独立 sampler 创建, 调用方随后释放这些对象的原始句柄
- **THEN** 集合仍能提供正确描述与解析记录, 资源在集合释放依赖前保持存活, 创建不修改 Texture 的采样状态

#### Scenario: Reject a wrong or foreign binding
- **WHEN** 资源来自另一设备/实现、类型与布局不匹配, 或缺少必须的实际资源
- **THEN** 创建不发布部分集合或视图, 报告相关逻辑位置及原因, 不对该资源执行原生绑定

#### Scenario: Reject an invalid active range
- **WHEN** 请求显式越界/不兼容的纹理视图, 或越界、未对齐、不满足记录粒度或超过实际 block 限制的 buffer range
- **THEN** 创建明确失败, 不通过截断范围、改变 format 或忽略 offset 来伪装成功

#### Scenario: Bind equivalent zero sentinel and explicit remaining ranges
- **WHEN** 合法常量 Buffer 或只读结构化 Buffer 绑定使用零长度、全一长度哨兵或对应显式剩余长度, 起点/记录粒度/设备限制均满足
- **THEN** 三种表示均创建成功并保留相同精确 offset/size, 非零起点不会被清零, 实际消费者能使用该合法绑定

#### Scenario: Reject an explicit length beyond the remaining capacity
- **WHEN** 合法起点的显式长度紧邻越过剩余容量, 或起点本身越出可绑定范围
- **THEN** 不发布绑定集, 给出非法范围诊断, 不 clamp 成一个合法非空范围或改变原先有效集合
