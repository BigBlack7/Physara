# Spec Delta

## ADDED Requirements

### Requirement: Report incremental preparation and arena use

正常接受帧的只读统计 SHALL 带 Scene、结构及内容版本、槽、本次被接受的 View 身份和提交身份。统计 SHALL 分别报告一次共享场景准备的耗时、复用或重建事实、对象、Primitive 与灯的实际更新数量、静态编码次数及各 View 的裁剪耗时与候选规模。统计 SHALL 区分 CPU 编码、实际静态 GPU 写入、View 动态写入和总原生上传。每槽的临时载荷与图 arena SHALL 提供容量、高水位及实际扩容次数，重置游标不计扩容。

事实 SHALL 由操作发生的 owner 记录。共享工作 SHALL 按帧计一次，裁剪按 View 计，失败、取消或安全 UI SHALL 保留最后有效的正常场景快照。展示 SHALL 消费已有只读统计通道，开启统计面板不改变事实计数口径或查询等待行为。

#### Scenario: Distinguish a warm frame from a local edit across slot rotation

- **WHEN** 同一场景经过三个槽暖机，两路 View 继续使用冻结输入，随后提交一次仅影响一个源槽的材质编辑
- **THEN** 快照可从实际更新与编码计数辨认零改写的暖机后帧和局部变化，共享提取只有一项，View 裁剪各自可见，arena 的高水位和扩容事实与各槽实际使用匹配
