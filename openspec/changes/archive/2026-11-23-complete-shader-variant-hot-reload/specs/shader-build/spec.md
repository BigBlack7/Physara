# Spec Delta

## ADDED Requirements

### Requirement: Resolve product Shader variants from an explicit catalog

产品 Shader 程序 SHALL 从有版本的有限目录解析 `VariantKey`。目录 SHALL 明确程序用途、阶段与入口、canonical profile、结构性字段、字段取值和实际允许的完整组合。相同语义请求 SHALL 得到相同键，未知程序或字段、重复字段和非法组合 SHALL 在进入编译队列前报告具体原因。存在歧义的程序或组合声明 SHALL 拒绝发布。材质数值、灯光强度与数量、曝光和普通运行时预览参数 SHALL 继续通过已有数据接口传递。

运行时构建 SHALL 使用与离线生成相同的变体值、生成接口、canonical 编译、验证和最终反射合同。现有合法的默认包及独立编译入口 SHALL 可由其原消费者使用。

#### Scenario: Resolve only complete approved combinations

- **WHEN** 同一受支持的 Forward 程序以不同字段顺序请求相同变体，另一个请求选择分别合法却未获准组合的字段值
- **THEN** 等价请求得到同一键。组合非法的请求指出冲突，在 CPU 编译及原生准备前拒绝

#### Scenario: Change numeric scene inputs independently of variants

- **WHEN** 渲染角色和结构字段相同，用户仅改变粗糙度、灯光强度或曝光
- **THEN** 上传和绘制仍消费新数值，变体键与所引用的编译包保持稳定

### Requirement: Reuse CPU Shader packages only for verified build inputs

运行时 CPU 包复用 SHALL 以规范化程序与变体、完整传递源及生成接口身份、schema、入口、编译选项、profile 和工具指纹为依据，并在命中前核实实际输入。相同的完整键 SHALL 复用已验证包，变化的编译输入 SHALL 使相关结果失效。相同 ABI 的不同角色 SHALL 可凭程序和阶段身份区分。包缓存 SHALL 按条目数和逻辑字节约束占用，按明确的淘汰或压力规则处理容量不足。失败、取消与过期构建 SHALL 保持此前完整成功结果及其身份。

#### Scenario: Invalidate a package through a shared transitive include

- **WHEN** 两个已缓存程序中仅一个依赖某个传递 include，而该文件发生内容变化
- **THEN** 依赖它的程序取得新编译身份和可追溯依赖，另一程序仍可按核实的同一完整键命中。在无内容变化的再次请求中可复用前次成功包

#### Scenario: Bound package reuse under pressure

- **WHEN** 兼容包复用已达到条目或逻辑字节上限，新的合法请求超出可用额度
- **THEN** 只有可淘汰的条目可释放额度。无法容纳时报告受影响请求和压力原因，已有可消费包的内容与身份仍可验证
