# render-graph-contract Spec Delta

## ADDED Requirements

### Requirement: Declare and authorize exact compute storage effects

Compute Pass SHALL 声明每个实际访问的类型化资源版本、storage 用途、方向及非空范围。Buffer 的范围以字节计，Texture storage 的范围以颜色 mip/layer 计。只读 SHALL 消费已定义版本，完整覆写 SHALL 对所声明范围产出新版本，读改写 SHALL 同时消耗已定义输入版本并在同一范围产出新版本。写入声明的范围 SHALL 由 Shader 调度定义或保留其所有内容，未覆盖内容 SHALL 不伪装为已定义。PassResources SHALL 核对请求版本、角色、方向及范围。录制 SHALL 核对实际绑定是否属于该计划为当前 Pass 解析出的物理身份、用途及范围。普通采样/ShaderRead 与 storage 授权 SHALL 保持独立。Compute 写生成的兼容版本 SHALL 能用于后续已支持的采样、indirect 或传输声明。

#### Scenario: Bind a storage read modify write to its exact input and output

- **WHEN** 一个 Compute Pass 对确定版本的 Buffer 区段或 image 子资源执行读改写，随后另一 Pass 从新版本读取，旁有两条未被该写入覆盖的范围
- **THEN** 只修改获批的范围和新版本，仍存活的旧内容依各自版本与范围解析。缺失可读取输入的读改写不被当作完整覆写

#### Scenario: Deny a role direction version or range outside the declaration

- **WHEN** 在包含合法 storage 访问的 Pass 中另外请求采样权限、将只读声明用作 store、使用另一版本或使绑定翻过许可边界
- **THEN** 当前 Pass 的查询或实际 Dispatch 授权明确拒绝相应请求。原生 Dispatch 只能使用满足声明的实际活动范围
