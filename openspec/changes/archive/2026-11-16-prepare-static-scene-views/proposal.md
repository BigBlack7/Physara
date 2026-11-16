# Proposal

## Why

[D06-T03](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t03) 已交付可编辑的 Scene、固定资产版本和主 View 请求。[D06-T04](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t04) 要在正式帧边界将这些状态准备为共享的只读 SceneFrame 和每 View 可见工作集，供下一工作包直接构建 Draw 与场景子图。

## What Changes

- 在当前可用的 FrameSlot 提取连续的静态对象、Primitive 和灯描述。记录固定模型版本、源槽、材质引用和必要的实例覆盖。各 View 共享记录，各自持有可见索引。
- 从 Scene 实体或 View 导航位姿准备相机。覆盖有限与无限透视、正交、Reverse-Z、稳定有限裁剪投影、采样投影及固定 aspect 的有效图像矩形。相机位姿与非相机场景内容按各自版本推进。
- 使用世界 bounds 和带 guard band 的稳定视锥，保守筛选 Primitive 和局部灯。无布局、零像素或无效相机返回可解释的每 View 结果，正常帧继续合成可用 UI。
- 正式 Runtime 在编辑事务生效后、声明图之前调用准备入口。现有测试视口消费有效矩形和失败原因，作为诊断合成，继续按提交回执确认输出。几何纹理驻留和真实 Forward 由 [D06-T05](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t05) 接续，编辑与导航交互由 [D06-T06](../../../../Docs/Developments/D06-D07-SceneAndForward.md#d06-t06) 接续。跨槽稀疏更新与多离屏输出的工程化由 [D08-T01](../../../../Docs/Developments/D08-D11-RenderPaths.md#d08-t01) 完成。

## Capabilities

### New Capabilities

- `scene-frame-preparation` — 从一致的 Scene 状态提取一次只读记录，并由安全帧槽保活固定资产和帧内索引。
- `view-preparation` — 解析相机与有效图像、区分稳定和渲染投影、发布保守的每 View 可见工作集。

### Modified Capabilities

- `static-scene` — 同步发布相机和灯使用的世界位姿，按实际影响将相机版本与静态内容版本分别推进。
- `editor-viewport-output` — 按本轮有效图像矩形合成测试图，并将相机或映射错误沿现有占位和输出确认合同报告。
- `runtime-lifecycle` — 在一致的编辑后帧边界准备 SceneFrame 及 View，构图和执行只消费准备值。

## Impact

- 涉及 `PhysaraScene` 的世界状态与只读提取入口、`PhysaraRenderer` 的 Prepare 和 View、Runtime 的帧槽与 Editor 的视口绑定。沿用已发布的 CPU 资产、现有 v1 场景文件、RenderGraph 和原生提交链。
- 使用固定非对称场景核对坐标、裁剪、版本、资产保活和多 View 共享，通过正式产品短运行核对诊断帧的接入。 [坐标约定](../../../../Docs/Architecture.md#conventions) · [相机](../../../../Docs/Architecture.md#camera) · [帧槽](../../../../Docs/Architecture.md#frame-state) · [SceneFrame](../../../../Docs/Architecture.md#prepare) · [View 准备](../../../../Docs/Architecture.md#view-preparation)
