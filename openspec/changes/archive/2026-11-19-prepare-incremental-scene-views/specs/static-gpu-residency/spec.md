# Spec Delta

## MODIFIED Requirements

### Requirement: Reuse prepared storage by effective asset version

静态 GPU 准备 SHALL 消费本帧固定的模型、Primitive 及图像版本。兼容布局的同模型同版本几何 SHALL 共享已就绪存储。图像的嵌入或外置身份、内容版本、颜色/数据解释、可访问的 mip 与有效采样参数 SHALL 得到保留。有效材质 SHALL 通过独立行及预解析绑定交付，同源未覆盖值可共享，实例覆盖的改变只产生该实例所需的更新。只变更相机或预览模式时，已驻留且仍适用的静态记录 SHALL 复用。

已准备且仍有效的静态对象、有效材质和灯数据 SHALL 跨安全槽使用与临时 arena 重置复用其编码。局部字段修改 SHALL 仅使依赖该字段的编码失效。灯行的有效值、可见索引及其组织方式 SHALL 决定各 View 灯表的重用，场景内其它内容的版本变化 SHALL 保持未受影响的灯表有效。View 常量 SHALL 独立对应本轮相机、设置和实际光照输入。新增可见内容、失效布局或资源退休后重新就绪时 SHALL 按实际需要准备对应数据。

#### Scenario: Reuse a mesh across two instances and isolate an override

- **WHEN** 两个实例使用同版模型及兼容几何，随后修改一个实例的材质因子或外置纹理，另一帧只改变相机
- **THEN** 兼容几何继续复用，修改只影响该实例实际取样的值，另一实例及固定源保持原值，仍适用的静态存储无需重新上传

#### Scenario: Sample a shared image under distinct interpretations

- **WHEN** 同一个固定图像在一个槽作为基色，另一个槽作为线性数据，且两槽声明不同 sampler
- **THEN** 采样结果分别遵循颜色解码和线性数据合同，各槽采用自己的寻址和过滤，引用同一源像素不会合并两种解释

#### Scenario: Keep static encodings independent of unrelated changes

- **WHEN** 三个槽已完成暖机，冻结场景跨槽轮转并重置临时 arena，然后在可见集合保持一致的条件下只改一个 View 的曝光，随后只改一项实例材质
- **THEN** 同版静态数据在冻结和曝光轮次的重新编码及静态上传计数为零，新曝光随对应 View 常量生效，材质编辑只更新涉及的静态数据，未受影响的对象、灯和有效材质继续复用

### Requirement: Record owned uploads and accept only the produced revision

场景上传 SHALL 各自拥有执行所需的真实目标和稳定 CPU 字节，列出实际写范围及后继用途。每次写入 SHALL 进入图，Draw 读取精确的版本、字节范围和纹理子资源。已驻留版本可直接读取，本图待写版本只在其范围有存活上传生产者时可引用。供后续帧复用的已上传身份 SHALL 由实际执行并获完整图接受的同一回执确认。未执行、被裁剪、录制失败或只有前缀提交的候选 SHALL 保持待重试或明确失败。

同一正常图的多个 View SHALL 对同一兼容存储代及范围共用同一图资源身份及生产版本。待写代 SHALL 只有在本图拥有完整生产者时才能共享。同一生产者 SHALL 只执行一次，保留至少一个存活消费者时继续有效。上传确认 SHALL 对应实际完成的存储代及其所需写入，未执行的 View 私有写入仍待重试。

#### Scenario: Draw after new geometry and texture ranges are uploaded

- **WHEN** 首次使用一份有效固定模型版本，当前图计划消费其新建顶点/索引、材质行及多个纹理层级
- **THEN** 图的实际传输范围先于匹配的 Draw 读取，成功回执后相同数据可作为已初始化内容导入后续帧

#### Scenario: Retry a candidate whose uploads were culled or followed by failure

- **WHEN** 新上传在最终计划中失去消费者，或已发写入后图失败且只获得合法前缀票据
- **THEN** 该候选不具备已上传资格，后续需要这些范围时重新准备，真实前缀工作所用的存储和上传区段由完成机制保活

#### Scenario: Use one upload for resources shared by two offscreen consumers

- **WHEN** 冷帧的两个 View 消费同一待上传几何、纹理或显示 LUT，其中一个 View 的私有分支随后在构图前被拒绝或在图内被裁剪
- **THEN** 仍存活的消费者只读取一份完整生产后的资源，实际上传确认随成功回执推进，共享版本保持可用，未执行的私有记录未获确认
