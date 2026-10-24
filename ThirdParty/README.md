# 第三方依赖

保留实际需要的上游源码、头文件和许可证，由本目录的 CMakeLists.txt 直接组织。头文件库使用 INTERFACE target，需要编译的库随当前 Debug/Release 配置生成到 build 目录。配置和构建不联网，不使用 submodule，也不要求手动准备预编译库。当前支持 Windows x64/MSVC。

本表集中记录固定版本、完整源码 commit、保留范围和许可；不另外建立依赖配置文件或逐文件哈希扫描。版本记录来自 2026-10-21 核实并下载的官方源码，构建不跟随浮动分支。

| 目录 | 固定版本 / 源码 commit | 许可 | 保留范围与用途 |
|---|---|---|---|
| entt | v4.0.0 / [85c6bba014049b5de8fad49d25424df2f1f6a8c1](https://github.com/skypjack/entt/tree/85c6bba014049b5de8fad49d25424df2f1f6a8c1) | MIT，见 LICENSE | single_include 头文件 |
| glad | v2.0.8 / [73db193f853e2ee079bf3ca8a64aa2eaf6459043](https://github.com/Dav1dde/glad/tree/73db193f853e2ee079bf3ca8a64aa2eaf6459043) | 生成器 MIT；生成代码见头部 SPDX 与 LICENSE | GL 4.6 Core loader 的 include/src，无可选扩展 |
| glfw | 3.5.1 / [d9d6f0f1f967807ffade6598ea9a631ebaf37a56](https://github.com/glfw/glfw/tree/d9d6f0f1f967807ffade6598ea9a631ebaf37a56) | Zlib，见 LICENSE.md | 公共头、Windows 后端及其公共/null 实现 |
| glm | 1.0.3 / [8d1fd52e5ab5590e2c81768ace50c72bae28f2ed](https://github.com/g-truc/glm/tree/8d1fd52e5ab5590e2c81768ace50c72bae28f2ed) | MIT 或 Happy Bunny，见 copying.txt | glm 头文件树 |
| imgui | v1.93.0-docking / [b916186e66aa3c02b72c6ca17455516512ed8049](https://github.com/ocornut/imgui/tree/b916186e66aa3c02b72c6ca17455516512ed8049) | MIT，见 LICENSE.txt | 核心和官方 GLFW 平台适配，无 demo 或原生图形绘制适配 |
| imguizmo | 固定源码快照 / [105e4fde45e4e74aae8973d98750820ef7aca7e2](https://github.com/CedricGuillemet/ImGuizmo/tree/105e4fde45e4e74aae8973d98750820ef7aca7e2) | MIT，见 LICENSE | ImGuizmo.cpp/.h，不带 GraphEditor、Sequencer 等附属工具 |
| json | v3.12.0 / [55f93686c01528224f448c19128836e7df245f72](https://github.com/nlohmann/json/tree/55f93686c01528224f448c19128836e7df245f72) | MIT，见 LICENSE.MIT | nlohmann 单头及 forward 声明，用于编辑器场景 JSON |
| mikktspace | 固定源码快照 / [3e895b49d05ea07e4c2133156cfa94369e19e409](https://github.com/mmikk/MikkTSpace/tree/3e895b49d05ea07e4c2133156cfa94369e19e409) | Zlib，见源码头 | .c/.h；资产导入需要生成切线时再接入构建 |
| nvrhi | 参考快照 / [6b96fb03e07539f08327aea76c56d55f1de9d906](https://github.com/NVIDIA-RTX/NVRHI/tree/6b96fb03e07539f08327aea76c56d55f1de9d906) | MIT，见 LICENSE.txt、ThirdPartyLicenses.txt | include/src/doc、README 和许可；只供源码参考，不加入 CMake |
| spdlog | v1.17.0 / [79524ddd08a4ec981b7fea76afd08ee05f83755d](https://github.com/gabime/spdlog/tree/79524ddd08a4ec981b7fea76afd08ee05f83755d) | MIT，含配套 fmt 许可 | include 头文件及内置 fmt |
| stb | 固定源码快照 / [a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d](https://github.com/nothings/stb/tree/a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d) | MIT 或公有领域，见 LICENSE | stb_image.h、stb_image_write.h |
| tinyexr | v3.2.0 / [6f470c9ab24bf3992bc512ce07e8ecb00d9bf105](https://github.com/syoyo/tinyexr/tree/6f470c9ab24bf3992bc512ce07e8ecb00d9bf105) | BSD-3-Clause；miniz 为 MIT，见 LICENSE、NOTICE、源码头 | tinyexr.cc/.h、内部读取头、配套 miniz |
| tinygltf | v3.0.1 / [0d9ce1a9561688c6c6e7471430d15a50a0b15f1a](https://github.com/syoyo/tinygltf/tree/0d9ce1a9561688c6c6e7471430d15a50a0b15f1a) | MIT，见 LICENSE | 官方 C11 实现和配套 JSON C 头，用于 glTF/GLB 模型 |

## 使用约定

- stb 仅提供头文件路径。首次实现图像解码时，在实际承担该职责的一个 cpp 中定义 STB_IMAGE_IMPLEMENTATION；需要图像写出时同样只定义一次 STB_IMAGE_WRITE_IMPLEMENTATION。实现宏不放公开头，不传播到所有消费者。
- TinyGLTF 3 使用 C API。模型解析与编辑器场景 JSON 是两条独立的数据入口，不通过 TinyGLTF 保存编辑器场景。
- ImGuizmo 的头文件要求先包含 imgui.h；其公开依赖由 CMake 的 PUBLIC 链接关系传递。
- NVRHI 只供后续 RHI 开发参考，没有 target、include 路径或链接关系。移植时在第一方 RHI 中记录来源与差异。
- MikkTSpace 暂不编译。后续遇到缺少切线且使用法线贴图的模型时，再由资产导入接入。

除源码选择与 CMake 组织外，不修改上游算法。后续升级时从固定 tag/commit 获取源码，保留许可，更新本表及归档校验值，并检查实际消费者的构建。

## GLAD 生成来源

当前保存的是 GLAD 2.0.8 在上述固定 commit 下生成的源码快照，生成环境使用 Jinja2 3.1.6、MarkupSafe 3.0.3：

```powershell
python -m glad --api gl:core=4.6 --extensions "" --out-path build/generated-glad --reproducible c --loader
```

生成头部的 SPDX 为 `(WTFPL OR CC0-1.0) AND Apache-2.0`。普通工程构建不运行 Python 或生成器。

## 源码归档校验值

以下 SHA256 对应 `https://codeload.github.com/<上表仓库>/zip/<固定 commit>` 的原始源码归档，用于核对获取来源，不参与每次 CMake 配置。

| 目录 | 源码归档 SHA256 |
|---|---|
| entt | `6f6518a2eefcc6264dfcbf5bbba5542411cc9bc3b1c3c03882a733a29e229ac8` |
| glad | `c6fe11176ef2dca44f0da370fabcd986e1d06320831950adc622d317d8a60e0a` |
| glfw | `0adf70be965af472c8b8d146d9c795c05510b6c1ef043df3451ac3e2656902e7` |
| glm | `3685779c341ed1c09a57ac038d119a735a63afb14f2365a24fbc3ffd31afe0db` |
| imgui | `bce023ef1cdd7af5217c76da5a54c9a1f30ea309f06a0cd113e1cefcb05587fa` |
| imguizmo | `754fca4e8c66c5a255f953239b5181385228923f646d1a39bdca5ba18acc7cfd` |
| json | `0746352e4e9532e7aeabbcbaa79079b6bc6008e9261e4d459c9486a9b48a172e` |
| mikktspace | `0fd3d51f10dd521cfb72195c6fc8112fe935a48f7ed86c511f3b2b9ffebea45d` |
| nvrhi | `375d7df03d40e3a458bb2c68ae26a27028f0944f4689b16c5bcea124d4605ed4` |
| spdlog | `2594162a28f06d12bf38ab4388c002156db9f8d5f7bcb0e1b4e60067dbad6c76` |
| stb | `fc6e7c840a0e025ee238fc09e136452614a4b765afa6ff4177d2e9e1b3493675` |
| tinyexr | `41017d74a7eb1848d6d3a1a031882773582050b2b50c367c353eaa0ed673e790` |
| tinygltf | `fe454348b521e0fdc9bce6e7c24d6a42572ecadcba3a507a217106b3b0cbef01` |

