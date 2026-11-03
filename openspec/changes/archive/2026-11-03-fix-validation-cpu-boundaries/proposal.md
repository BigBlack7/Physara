# Proposal

## Why

首次存量独立审查在已交付 D02-T02 的 Common/Validation 中发现 CR-001/002/003: 非法 registerSpace 在报错后仍越界写, 合法 Pipeline 和纹理路径在受支持 MSVC 的 CPU 分配失败时可能意外 terminate. 本 change 修复这些现有合同缺陷, 作为 D00-D02 存量审查的 A 整改交付, 不改写历史归档.

## What Changes

- 非法 descriptor-set registerSpace 在任何数组访问前拒绝, 保留合法边界和绑定冲突检查.
- Pipeline 的本地 Shader/绑定汇总工作集遵守既定 CPU 异常传播边界, 不从 noexcept 容器元素操作中意外终止.
- 纹理子资源容量检查不再构造临时 vector; 本地纹理状态初始化与展开的分配失败进入已有可捕获/sticky-error 边界.
- Debug/Release 验证公共入口、逐分配点失败、Reset 后复用和真实上传前缀保活; 保留 B 注释, 更新来源差异、当前审查单与开发计划.
- 修正后交维护者安排独立 B Check/Annotation, B 通过后再最终 verify、sync/archive. A 不自勾 B 复核, 不重置问题单.

## Capabilities

### New Capabilities

无.

### Modified Capabilities

- `rhi-common-validation`: 明确 descriptor-set 编号的安全拒绝及普通 CPU 存储异常/跟踪器失败退出行为, 增加回归场景.

## Impact

主要影响 RHI/Validation、RHI/Common 及其已交付 OpenGL 上传消费者. 关联 [D02-T02](../../../../Docs/Developments.md#d02-t02)、[D02-T04](../../../../Docs/Developments.md#d02-t04) 和[首次存量审查](../../../../Docs/Developments.md#baseline-review). 不改变公开 StaticVector 的异常合同、资源/Shader ABI、原生 profile、第三方输入或长期架构. 不启动 D03, 不引入框架、生产检查 target 或第二产品入口. 本轮检查输入和原始日志保存在忽略的 build/agent-a-review, 入口与结果留存于 tasks 和当前问题单.
