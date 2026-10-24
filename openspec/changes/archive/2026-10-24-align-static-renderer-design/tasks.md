# Tasks

## 1. 校正技术依据

- [x] 1.1 复核固定 Filament 与 Khronos 原始资料，在 Filament Reference 明确标准材质映射、静态 TAA 与粗糙 SSR 的数据和近似；逐条检查来源实际支持正文。
- [x] 1.2 在 NVRHI Reference 校正非 coherent 映射、复制 scope、device-lost 和共享 VCB 并发保证；以固定源码路径和 Vulkan 规范核对，不改第三方快照。

## 2. 固定当前项目设计

- [x] 2.1 修订 Architecture 的静态范围、只读模型与单 JSON 场景、实例覆盖、资产重载和串行文件操作；检查打开、编辑、保存、失败及空场景流程闭合。
- [x] 2.2 修订 Architecture 的材质映射、相机重投影、TAA/SSR、GL RAW/WAR/WAW 与 Fatal/退出；检查生产者、消费者、单位、版本及成功提交条件一致。
- [x] 2.3 澄清 AGENTS 通用规则歧义与三层文档职责；检查不加入项目功能细节或格式工具要求。

## 3. 同步推进计划与入口

- [x] 3.1 调整 Developments 的实际前置及静态几何任务，检查阶段/工作包计数、交叉锚点和任务状态，未实现功能保持未勾选。
- [x] 3.2 同步 README、Assets/README 与 OpenSpec context 的静态定位和持久化边界，检查没有相反承诺或新增辅助工程内容。

## 4. 跨文档验收

- [x] 4.1 核对本变更规格场景、当前文档本地链接/锚点、禁用能力残留及任务依赖；运行 OpenSpec strict validate，报告文档证据与未执行的原生验证，不新增验证报告文件。

本轮证据：17 份文档中的 966 个本地文件/锚点链接通过；20 阶段、113 个唯一连续工作包及其显式前置检查通过；开发计划仍只有 D00 原有 12 个清单勾选。OpenSpec `validate --all --strict` 的当前 change 与主规格均通过，`git diff --check` 无内容问题。三份技术文档引用和代码围栏完整，材质/SSR/TAA 完成独立交叉核对；D00 归档和两份 CMakeLists 的哈希未变。本变更只交付设计修订，未运行构建、原生图像、GPU 数值或性能验证。
