# Spec Delta

## MODIFIED Requirements

### Requirement: Declare exact resource ranges and canonical imports

图 SHALL 持有创建或导入资源的描述，将每次用途解析为明确的 mip、layer、aspect 或缓冲字节区间。显式区间为空、越界、不可表示，或 aspect 与纹理格式不符时 SHALL 报告原因，不得裁成合法区间。导入 SHALL 持有外部 RHI 资源的引用，并明确允许范围、入口与结束状态及输入内容是否有效。同一外部资源的等价导入 SHALL 返回同一导入版本；同一资源重复导入时，范围、状态或内容声明冲突 SHALL 拒绝，不得另建独立身份。

入口和结束状态 SHALL 各自提供公共 RHI 定义的非零状态。遇到 `Unknown` 或未定义状态位，图 SHALL 报告资源及对应字段，并拒绝发布导入记录。

Buffer 导入 SHALL 能表达允许范围中全部有效、全部未定义或一组确切的已定义字节区间。区间 SHALL 经范围校验、归一化后纳入同一导入身份的等价判断。编译及执行期权限 SHALL 以同一份覆盖信息保留已定义区间，直到被对应写版本改变。未定义的间隙 SHALL 保持不可读，直到有实际写入完整覆盖它。

Texture 导入 SHALL 同样表达允许范围中全部有效、全部未定义，或一组确切的已定义 mip、layer、aspect 范围。部分覆盖 SHALL 经校验和归一化后参加规范导入的等价比较。已定义的深度分量 SHALL 不扩大为模板有效性，已定义的 mip 或层 SHALL 不扩大到相邻子资源。允许读写的范围与初始内容已定义的范围各自明确。

先前图的受保留交接重新导入时，调用方 SHALL 以本设备已接受的物理对象及其交接事实建立本图身份，检查所请求的代、许可范围、实际入口状态与有效内容。状态粒度 SHALL 遵守 RHI，纹理按 mip/layer，Buffer 按整个对象。内容覆盖及依赖继续保留精确 aspect 或字节。未获接受、过期、设备或角色不匹配的交接 SHALL 不建立可读导入。

#### Scenario: Reuse only an equivalent import

- **WHEN** 同一外部资源以等价的规范范围和相同进出状态、内容声明重复导入，随后同一资源以冲突条件再导入
- **THEN** 等价请求返回原始导入版本，即使已有写入；冲突请求报告失败，图仍只有一份外部资源身份

#### Scenario: Reject a range that would need clipping

- **WHEN** 纹理访问的 mip、layer 或 aspect 超出声明允许范围，或缓冲访问只超出可用字节区间末端一字节
- **THEN** 合法边界内的范围可声明，越界请求不生成用途、写版本或导出记录

#### Scenario: Import initialized buffer intervals with an unwritten gap

- **WHEN** 一份页 Buffer 有已驻留区间、待上传区间及一个未写空洞，随后图以精确范围写入待上传区间
- **THEN** 两个已定义范围按其导入或生产版本合法消费，跨空洞的读取被拒绝；声明等价初始覆盖继续使用同一图身份，改变覆盖的重复导入被拒绝

#### Scenario: Import one defined texture subset

- **WHEN** owner 导入一张包含多个 mip、层或分量的纹理，仅声明其中两个互不相交的范围已定义，之后以等价覆盖或改变覆盖的方式重复导入
- **THEN** 精确有效的读取可声明，覆盖空洞的读、Load 或导出在写满之前被拒绝。等价覆盖返回同一版本，冲突覆盖不得形成另一个资源身份。物理状态一致不能替代内容覆盖
