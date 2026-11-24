# shadow-rendering Specification

## Purpose

规定静态场景如何从当前 View 的阴影计划生成有明确层地址、覆盖及深度约定的阴影纹理，并按成对的投影查询可见性。深度结果和跨帧复用必须对应实际图访问、当前场景内容及已接受的提交。

## Requirements

### Requirement: Produce authorized directional shadow depths from static casters

请求方向光级联诊断且具备实际消费的 View SHALL 从同一冻结场景和对应阴影计划生成全部已准入级联的 D32 深度。每级 SHALL 对应一个可追溯到计划区域的层，图 SHALL 声明精确的深度写入及后继采样范围。每级 SHALL 使用独立的常规 `[0,1]` 深度，近端为 0，以 1 清除，并以 `Less` 写入。Caster bias SHALL 使用与该投影及 texel 尺度相容的方向和单位。已分配且 caster 集为空的层 SHALL 交付已清除内容。

支持的静态 OPAQUE 和 MASK caster SHALL 使用与正常静态几何一致的实例变换、有效基色 alpha、采样和覆盖阈值。负缩放 SHALL 保持有效绕序，双面材质 SHALL 从两侧投射。当前 View 的准备 SHALL 使画外 caster 需要的几何、实例和 MASK 数据在写入通道消费前可用。投射物准备发生失败时 SHALL 以既有可定位的拒绝或等待状态处理受影响的候选。

#### Scenario: Render a masked offscreen caster into the matching layer

- **WHEN** 冻结场景有遮挡屏内接收点的画外 caster，并有共享静态网格的 MASK、镜像和双面实例，当前图请求实际阴影诊断
- **THEN** 图先生产被实际读取的数据，再向对应的 D32 层写深度。实心部位留下常规深度，MASK 孔保留后方几何或清除值，实例按各自的变换、绕序和双面属性覆盖。主视图仍可读到匹配的 Reverse-Z 场景深度

#### Scenario: Clear the layer after its last caster leaves

- **WHEN** 一个已分配级联的最后一个 caster 被移除或关闭投射，接收区间仍有效
- **THEN** 匹配该级新版本的深度层只含清除值，原先被该物遮挡的接收点查询为全可见

### Requirement: Resolve directional cascade visibility by stable view depth

方向光查询 SHALL 以稳定相机的线性视深和同次计划的名义分割点确定级联。内部非过渡区 SHALL 使用左闭右开区间，最终有效区间包含有限远端。非空过渡带 SHALL 使用相邻两级各自的有效层和投影，按视深在带内线性混合两次比较所得的可见性。

查询 SHALL 从同级光空间投影生成 UV 和常规比较深度，使用统一纹理原点。单点比较 SHALL 在接收深度小于或等于保存深度时返回全可见。视深超出有效区间时 SHALL 返回中性的 `visibility=1`。每个级联查询 SHALL 只访问投影坐标有效、内容已定义且有采样权限的层，否则该级返回全可见。

#### Scenario: Query a cascade boundary and the ends of shadow coverage

- **WHEN** 一个固定场景的接收样点分布在近端、远端和某个非零过渡带两侧，带内相邻两级在选定点给出可区分的比较值
- **THEN** 过渡带内的可见性按两次比较结果及线性视深权重变化，带的两端分别接续相邻级联的结果，覆盖区间外为全可见。仅改变主相机实际 jitter 时，固定世界样点的选级和权重保持稳定

### Requirement: Reuse directional depth only from a matching accepted graph output

仍被当前诊断消费且生产依赖完全匹配的深度组 SHALL 从同设备的已接受结果复用，静态后继帧保持零深度重绘。复用 SHALL 核对 View、Scene、有效存储规格、逻辑区域及内容版本，以及影响写入的深度程序和偏移规则。深度内容只有在对应生产者实际执行、该 View 的消费分支存活且完整正常提交被接受后 SHALL 成为可供后继帧复用的版本。

取消、裁剪、录制失败或仅有前缀票据的候选 SHALL 保持待产出资格。阴影禁用、布局或有效输入改变时 SHALL 结束不匹配内容的当前可用状态。替换的 GPU 存储 SHALL 沿既有图池和提交完成协议保活及退休。

#### Scenario: Retry an unaccepted replacement without reading a stale layer

- **WHEN** 静态 View 复用了一组已接受深度，随后一个影响阴影覆盖的输入改变，继任候选的场景分支被裁剪或提交未被完整接受
- **THEN** 未接受版本无法用于后继帧采样，恢复后为当前输入生产并接受匹配组才可复用。原来的在途访问仍按其真实完成条件退休
