# Spec Delta

## MODIFIED Requirements

### Requirement: Reuse prepared storage by effective asset version

静态 GPU 准备 SHALL 消费本帧固定的模型、Primitive 及图像版本。兼容布局的同模型同版本几何 SHALL 共享已就绪存储。不同几何 SHALL 可分配在同一兼容顶点页或索引页的不同区间，Draw SHALL 以对应的 offset、长度和索引解释消费。图像的嵌入或外置身份、内容版本、颜色/数据解释、可访问的 mip 与有效采样参数 SHALL 得到保留。纹理视图及其绑定 SHALL 按有效格式、维度、子资源范围与采样解释复用。有效材质 SHALL 通过表内独立行及预解析的范围绑定交付，同源未覆盖值可共享，实例覆盖的改变只产生该实例所需的更新。只变更相机或预览模式时，已驻留且仍适用的静态记录 SHALL 复用。

已准备且仍有效的静态对象、有效材质和灯数据 SHALL 跨安全槽使用与临时 arena 重置复用其编码。局部字段修改 SHALL 仅使依赖该字段的编码失效。灯行的有效值、可见索引及其组织方式 SHALL 决定各 View 灯表的重用，场景内其它内容的版本变化 SHALL 保持未受影响的灯表有效。View 常量 SHALL 独立对应本轮相机、设置和实际光照输入。新增可见内容、失效布局或资源退休后重新就绪时 SHALL 按实际需要准备对应数据。环境的固定源及烘焙身份保持匹配时，系统 SHALL 使用仍有效的所需环境产品，旋转、强度和曝光的变化只更新查询输入。

#### Scenario: Reuse a mesh across two instances and isolate an override

- **WHEN** 两个实例使用同版模型及兼容几何，随后修改一个实例的材质因子或外置纹理，另一帧只改变相机
- **THEN** 兼容几何继续复用，修改只影响该实例实际取样的值，另一实例及固定源保持原值，仍适用的静态存储无需重新上传

#### Scenario: Sample a shared image under distinct interpretations

- **WHEN** 同一个固定图像在一个槽作为基色，另一个槽作为线性数据，且两槽声明不同 sampler
- **THEN** 采样结果分别遵循颜色解码和线性数据合同，各槽采用自己的寻址和过滤，引用同一源像素不会合并两种解释

#### Scenario: Keep static encodings independent of unrelated changes

- **WHEN** 三个槽已完成暖机，冻结场景跨槽轮转并重置临时 arena，然后在可见集合保持一致的条件下只改一个 View 的曝光，随后只改一项实例材质
- **THEN** 同版静态数据在冻结和曝光轮次的重新编码及静态上传计数为零，新曝光随对应 View 常量生效，材质编辑只更新涉及的静态数据，未受影响的对象、灯和有效材质继续复用

#### Scenario: Draw different primitives from shared pages

- **WHEN** 两个不同的 Primitive 使用兼容顶点布局与索引宽度，在有余量的同组页中占用可区分的区间，后分配的区间起点非零
- **THEN** 图以同一页身份追溯各自范围，绘制从正确的区间取得各自顶点和索引，暖机后两份范围都能直接复用

### Requirement: Record owned uploads and accept only the produced revision

场景上传 SHALL 各自拥有执行所需的真实目标和稳定 CPU 字节，列出实际写范围及后继用途。每次写入 SHALL 进入图，Draw 读取精确的版本、字节范围和纹理子资源。已驻留版本可直接读取，本图待写版本只在其范围有存活上传生产者时可引用。供后续帧复用的已上传身份 SHALL 由实际执行并获完整图接受的同一回执确认。未执行、被裁剪、录制失败或只有前缀提交的候选 SHALL 保持待重试或明确失败。

场景批 SHALL 在共享驻留、各 View 常量和实际 Draw 工作数据追加完成且所有生产者通过准备检查后封存。可合并的有界相邻脏区间 SHALL 在保持写入先后、实际消费活性及确认粒度的前提下合并。合并结果 SHALL 精确覆盖要写的字节，保留每个存储代的生产关系。UI 顶点/索引、字体和其它 UI 上传 SHALL 各自持有载荷并独立声明，与存活的场景写入一起进入最终图的设备准入。封存前的拒绝 SHALL 阻止失败候选进入正常录制。

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

#### Scenario: Merge live adjacent writes without including unwritten bytes

- **WHEN** 同一页有两段相邻、共享存活条件且可合并的脏数据，另有被空洞隔开的数据只服务于裁剪分支
- **THEN** 实际合并写覆盖两段脏数据，空洞和被裁剪区间不占本次设备上传，完整图只确认实际写到的对应存储代

#### Scenario: Reject a late scene upload before recording

- **WHEN** 共享驻留已准备好且最后一个实际 View 或 Draw 数据生产者发现必需范围或容量不可用
- **THEN** 失败候选在封存及录制前停止，首个准备原因仍可见，基础 UI 沿独立候选处理且不确认失败的场景上传

### Requirement: Bound residency and retire generations after real completion

GPU 准备 SHALL 在设备能力及有限条目、驻留字节和上传预算内运行。预算 SHALL 包含已保留的物理页容量、仍在途的旧区段、纹理与环境产品的实际分配存储，以及材质/绑定、纹理视图、sampler 和环境产品的适用条目。共享存储 SHALL 按物理容量计一次。有界增长、可淘汰目录与空闲页回收 SHALL 随同资源版本失效运行。暂时没有可用安全容量时返回 Pending，确定超限或资源/绑定不可用时返回可定位的拒绝或失败。当前可绘集合所需的资源均有效后才可交付正常候选。

替换版本 SHALL 使用安全的新存储并保持旧引用有效，只有覆盖旧访问的实际 GPU 完成或合法退出收拢才可复用或销毁仍被引用的存储。准备、关闭、CPU 目录淘汰、前缀提交及没有可靠 fence 的失败 SHALL 按各自真实的拥有关系保留仍会被访问的区间，不能把一个提交票据直接解释为完成。

#### Scenario: Replace a revision referenced by an in-flight draw

- **WHEN** 一个旧模型或材质行已被接受且仍被在途绘制使用，新固定版本或实例覆盖进入准备
- **THEN** 新候选使用自己的匹配数据，旧提交保有其原版本，旧存储与绑定到实际完成后才具备退休资格

#### Scenario: Expose the reason a required resource cannot be prepared

- **WHEN** 当前候选的资源大小或绑定超出设备/profile 能力，或者本轮在途资源占用了所需预算
- **THEN** 受影响的 View 报告对应硬限制或暂时等待，基础 UI 可继续，候选不把未就绪句柄交给绘制

#### Scenario: Replace a resident environment under a bounded budget

- **WHEN** 视口只调整已接受环境的朝向、强度或曝光，随后换入实际源或烘焙身份不同的候选，旧代仍在使用
- **THEN** 查询调整复用原烘焙和驻留资源，换源按预算取得完整匹配产品，旧产品在真实访问结束前保有，候选超限时保留旧已接受输出并报告原因
