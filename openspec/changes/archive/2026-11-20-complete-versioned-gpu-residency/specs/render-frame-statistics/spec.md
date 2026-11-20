# Spec Delta

## ADDED Requirements

### Requirement: Report bounded GPU residency and upload reuse

已接受场景的有界只读统计 SHALL 报告几何页的分配容量、已用、空闲和待退休字节，以及能辨别页内碎片的连续空闲范围信息。材质行和绑定 SHALL 报告条目、请求、命中与资源代，纹理/环境 SHALL 报告物理驻留容量和适用条目。RHI SHALL 从实际录制及退休记录发布映射区段容量、占用与高水位、复用或原生分配，以及真实上传次数和字节。脏范围合并后的原生写入 SHALL 按实际命令计数，Renderer 的准备需求 SHALL 保持单独口径。共享物理存储的字节 SHALL 在同一口径只计一次。

暂时容量不足、确定超限及资源准备失败 SHALL 通过当前诊断报告相应操作和可用预算。失败、取消或只接受 UI 的帧 SHALL 保留最后正常场景统计身份。Editor SHALL 通过既有只读通道展示这些计数，展示开关 SHALL 不改变资源回收、上传或 GPU 等待行为。

#### Scenario: Observe reused storage and distinguish budget pressure

- **WHEN** 场景在正常帧中首次分配页及上传区段，之后复用已完成范围，并在一轮申请中先后遇到暂时占用和确定超限
- **THEN** 接受帧的快照可区分实际新分配、范围复用和原生上传，容量及高水位可解释，当前诊断区分两种拒绝原因并保留最近有效场景快照
