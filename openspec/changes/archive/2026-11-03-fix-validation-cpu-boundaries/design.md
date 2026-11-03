# Design

## Context

问题依据见 [proposal](proposal.md) 和 [CR-001/002/003](tasks.md#review-loop-closeout). 当前支持 Windows x64/MSVC/C++20; StaticVector 保留固定来源的容量和元素操作前提, 普通 Validation CPU 容器异常允许传播, tracker 则捕获 bad_alloc/length_error 并锁存 recording 错误. OpenGL 即时上传已发出的前缀即使后续准备失败仍须由物理包保活和退休.

## Goals / Non-Goals

**Goals:** 在真实入口恢复安全拒绝和既定异常边界, 保留合法输入、语义汇总、提交元数据和引用寿命. 同步 B 已核实的注释, 提供最终输入与自检证据供 B 独立复核.

**Non-Goals:** 不修改公开 StaticVector 或 Shader/资源 ABI, 不开启原生 Pipeline、Shader/Draw、完整传输或呈现; 不修改长期架构、第三方和历史归档. 不将受控 OOM 说成自然内存耗尽或 GPU 故障.

## Decisions

### 1. 固定本次收到的输入并保留独立职责

基准 commit 为 1c897364167677b0df283afa187279d48a8f5c78, 无已暂存修改. 62 个源/头/CMake 文件及问题单/tpl/规范/计划/config 的接收快照保存在 build/agent-a-review/input. 当前源清单 SHA256 为 617880326e645c15cfdca260ab7d1f281c8fb5a3afb120e4aa36b0941afe4a7a, 与 B 记录的 c61f3f... 不同, B 的临时清单现不可读取, 不能声称逐文件匹配. A 以现收到的代码重现并修正问题, 交回完整最终 diff 与清单让 B 检查, 保留原记录及注释. 不以 A 自检取代 B 的最终验收.

### 2. CR-001: 上界错误立即返回

ValidatePipelineBindingLayouts 在 descriptor-set registerSpace >= MaxBindingLayouts 时报告原有诊断并返回 false, 不再落到映射写入. 合法值、重复 set、混合解释、push-constant 检查沿用现有算法. 不扩大数组、不删断言、不把 D3D register-space 的不同语义全局限制为 descriptor-set 上界. 通过公共 Pipeline 入口验证 0/7/8/16/UINT32_MAX 和底层委托次数.

### 3. CR-002: 只修正内部工作集

绑定汇总改用 std::array<BindingSummary, MaxBindingLayouts> 的普通构造, 通过 numBindingLayouts 限定消费范围. 原 StaticVector 本来也默认构造全部容量, 无需 resize 再构造/赋值会分配的 unordered_set. 数组构造和后续集合操作的异常可以沿普通 Validation 边界传播. 同一消费链中 Graphics/Meshlet 的 Shader vector 改用 count=0 构造, 避免 MSVC Debug 默认 vector 的 noexcept proxy 分配同样截断传播; Compute 的 initializer-list 构造保留. 不机械改变公开容器的 noexcept 或所有既有消费者.

首次逐分配点自检证实外层数组不足以恢复边界: MSVC 19.44 的 xhash:261 在 noexcept 的 _Hash_vec(allocator) 中分配 Debug proxy, 实际 unordered_set 构造在 failAt=4 仍 terminate (cpu-Debug-stack.log). 因此私有 BindingLocationSet、layout 输入去重及提交预检的唯一列表集合改为 std::set, 集合交/并/差继续按同一逻辑键检查; BindingLocation 用 type/registerSpace/slot/arrayElement 的字典序, 删除不再消费的私有 hash 特化. 这些集合没有需要保留的散列遍历顺序合同, O(log n) 查找换取受支持配置下的可靠失败边界, 不宣称性能改善. 同样证据适用于 tracker 的 unordered_map 内部构造, 将两个私有状态索引改为 std::map, 指针仅以标准比较器作为身份 key, 继续由 tracker 单写和 Reset 释放. 不改公共 hash 或在 STL 内部打补丁. 实际分配点测试覆盖这些共享消费者, 包括原生提交预检 OOM 后的合法重试.

### 4. CR-003: 查询不构造容器, 状态表使用可抛出的初始化

两个纹理容量检查直接计算默认分配器与指针差值可表示的元素上界: min(PTRDIFF_MAX, SIZE_MAX / sizeof(ResourceStates)), 用除法检查 arraySize*mipLevels, 不分配临时 vector. 这是受支持 Windows x64/MSVC/default-allocator 的容器长度上界, 不表示内存预算或设备能力; 验证与实际 count=0 vector.max_size 一致. 两个边界各保留本地检查, 不为两行算术预建新通用框架.

TextureState 的 subresourceStates 显式使用 vector(count=0) 成员初始化, 保留聚合字段和原状态默认值, 使 Debug proxy 分配能从 make_unique 到 tracker 的既有 catch 传播. 展开仍在现有 resize 与 catch 中; HasErrors 后不发布 permanent/initialized 元数据, Reset 丢弃本地状态再允许新 recording. 不放宽 Debug iterator 检查, 不把失败变成成功空状态.

### 5. 失败注入、消费者和交接证据

检查输入位于忽略 build/agent-a-review, 不加入生产 target/框架. CPU recording double 只记录公共入口到达/返回, 直接链接正式 Common/Validation, 不复制检查算法. 全局 new 在已完成夹具准备后只于待测调用期间逐分配点抛 bad_alloc, terminate handler 用独立退出码识别未捕获终止. 比较正常、空/单/多 layout、重复绑定和合法委托; Validation 需可捕获异常且不委托部分操作, tracker 需锁存错误并能 Reset 复用. Debug/Release 分开记录, 原 MSVC C1001 不算通过, 不运行不匹配残留 exe.

对实际 Host/Validation/OpenGL 上传, 先发出一个合法前缀, 再向新纹理 tracker 建表/展开注入 CPU 分配失败; 检查无非法后续写入、外部引用释放后包仍保活、真实完成或受控退出才退休. 正常颜色纹理上传完成后以独立已知字节读回. 辅助读回不新增产品能力. 同时重建受影响生产 Debug/Release、独立通用消费者及 Runtime 冒烟, 检查第一方警告和源/构建输入身份.

## Risks / Trade-offs

- MSVC STL 的 Debug proxy 是支持配置的一部分: 用真实工具链逐失败点验证, 不能只在 GCC 或 Release 推断结果.
- 全局分配注入可能误中夹具/诊断: 所有可分配预备工作放在开关之外, callback 使用固定存储; 区分数据结构各分配点和底层委托.
- B 的旧输入摘要不匹配: 保存实际输入与最终文件清单, 不声称继承旧结果; B 需复核最终实际版本.
- 本机历史 fixture 不在当前 build 中: 本轮提供本地明确检查入口/源码哈希, 不声称原有大矩阵已重跑, 不改变项目测试组织.

## Migration Plan

先建立本修正 change 和边界回归, 再逐项实施与验证, 更新来源差异、审查单 A 栏和开发计划. 将完整最终版本交维护者安排 B Check/Annotation; 正式问题全部由 B 关闭后执行最终原生 verify, 再同步 delta、归档并按规则重置问题单. 本轮 A 不同步主规格、不归档、不提交 Git. 如需回退只撤销本轮明确修正, 保留交入的规范及 B 注释.
