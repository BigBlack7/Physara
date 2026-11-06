# 第三方依赖

采用精简源码直接入库，只保留当前消费者需要的实现、传递头和许可/NOTICE，由本目录及 ShaderToolchain 的 CMakeLists.txt 显式组织。关闭上游 target 不能代替源码裁剪；不为加载上游工程保留未用实现、工具、测试、示例、CI、安装/打包或仓库维护文件。固定生成源码须有可复现来源，不能包含机器路径、时间戳或浮动版本。头文件库使用 INTERFACE target，需要编译的库随当前 Debug/Release 配置生成到 build 目录。配置和构建不联网，不使用 submodule，也不要求手动准备预编译库。当前支持 Windows x64/MSVC。

本表集中记录固定版本、完整源码 commit、保留范围和许可；不另外建立依赖版本配置文件或逐文件来源清单。既有依赖于 2026-10-21 获取, Shader 工具链于 2026-11-04 按对应 change 的固定 commit 获取, 构建不跟随浮动分支。

| 目录 | 固定版本 / 源码 commit | 许可 | 保留范围与用途 |
|---|---|---|---|
| entt | v4.0.0 / [85c6bba014049b5de8fad49d25424df2f1f6a8c1](https://github.com/skypjack/entt/tree/85c6bba014049b5de8fad49d25424df2f1f6a8c1) | MIT，见 LICENSE | single_include 头文件 |
| glad | v2.0.8 / [73db193f853e2ee079bf3ca8a64aa2eaf6459043](https://github.com/Dav1dde/glad/tree/73db193f853e2ee079bf3ca8a64aa2eaf6459043) | 生成器 MIT；生成代码见头部 SPDX 与 LICENSE | GL 4.6 Core loader 的 include/src，无可选扩展 |
| glfw | 3.5.1 / [d9d6f0f1f967807ffade6598ea9a631ebaf37a56](https://github.com/glfw/glfw/tree/d9d6f0f1f967807ffade6598ea9a631ebaf37a56) | Zlib，见 LICENSE.md | 公共头、Windows 后端及其公共/null 实现 |
| glslang | 16.6.0 / [e1b562a8bed273a02f30b59b66a5d499793cede5](https://github.com/KhronosGroup/glslang/tree/e1b562a8bed273a02f30b59b66a5d499793cede5) | 多许可, 见 LICENSE.txt、LICENSES 及各源文件声明 | GLSL/SPIR-V 编译、Windows 适配、默认资源限制及其传递头, 固定 build_info.h; 无 C 包装、StandAlone、上游工程、HLSL、测试或外部依赖副本 |
| glm | 1.0.3 / [8d1fd52e5ab5590e2c81768ace50c72bae28f2ed](https://github.com/g-truc/glm/tree/8d1fd52e5ab5590e2c81768ace50c72bae28f2ed) | MIT 或 Happy Bunny，见 copying.txt | glm 头文件树 |
| imgui | v1.93.0-docking / [b916186e66aa3c02b72c6ca17455516512ed8049](https://github.com/ocornut/imgui/tree/b916186e66aa3c02b72c6ca17455516512ed8049) | MIT，见 LICENSE.txt | 核心和官方 GLFW 平台适配，无 demo 或原生图形绘制适配 |
| imguizmo | 固定源码快照 / [105e4fde45e4e74aae8973d98750820ef7aca7e2](https://github.com/CedricGuillemet/ImGuizmo/tree/105e4fde45e4e74aae8973d98750820ef7aca7e2) | MIT，见 LICENSE | ImGuizmo.cpp/.h，不带 GraphEditor、Sequencer 等附属工具 |
| json | v3.12.0 / [55f93686c01528224f448c19128836e7df245f72](https://github.com/nlohmann/json/tree/55f93686c01528224f448c19128836e7df245f72) | MIT，见 LICENSE.MIT | nlohmann 单头及 forward 声明，用于编辑器场景 JSON |
| mikktspace | 固定源码快照 / [3e895b49d05ea07e4c2133156cfa94369e19e409](https://github.com/mmikk/MikkTSpace/tree/3e895b49d05ea07e4c2133156cfa94369e19e409) | Zlib，见源码头 | .c/.h；资产导入需要生成切线时再接入构建 |
| nvrhi | 参考快照 / [6b96fb03e07539f08327aea76c56d55f1de9d906](https://github.com/NVIDIA-RTX/NVRHI/tree/6b96fb03e07539f08327aea76c56d55f1de9d906) | MIT，见 LICENSE.txt、ThirdPartyLicenses.txt | include/src/doc、README 和许可；只供源码参考，不加入 CMake |
| spdlog | v1.17.0 / [79524ddd08a4ec981b7fea76afd08ee05f83755d](https://github.com/gabime/spdlog/tree/79524ddd08a4ec981b7fea76afd08ee05f83755d) | MIT，含配套 fmt 许可 | include 头文件及内置 fmt |
| spirv-cross | vulkan-sdk-1.4.363.0 / [f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7](https://github.com/KhronosGroup/SPIRV-Cross/tree/f11ba9f0b21ba8fc15153d50a2a1ae31ab1cf8f7) | Apache-2.0 或 MIT 及所附第三方许可, 见 LICENSE、LICENSES | core 反射的 4 个实现、GLSL lowering 的 spirv_glsl.cpp/.hpp 及实际传递头/许可, 无上游工程、版本脚本、其他语言后端、CLI 或测试语料 |
| spirv-headers | 固定源码快照 / [496543121ce6419f23d6fa5d7194ba66c36212d2](https://github.com/KhronosGroup/SPIRV-Headers/tree/496543121ce6419f23d6fa5d7194ba66c36212d2) | MIT 及 LICENSE 列出的例外, 保留 REUSE 归属声明 | validator 实际包含的 unified1 头及许可; 语法 JSON 只在更新固定表快照时从同 commit 的完整归档取得 |
| spirv-tools | 固定源码快照 / [ef96ed763b43b59b33b31b362f09a02b729fa1c9](https://github.com/KhronosGroup/SPIRV-Tools/tree/ef96ed763b43b59b33b31b362f09a02b729fa1c9) | Apache-2.0, 见 LICENSE | 基础解析/validator、必要 util/val 头与六份固定表/版本快照, 无 optimizer/fuzz/reduce/link/lint/diff、未用 option 实现、上游工程、脚本、测试或示例 |
| stb | 固定源码快照 / [a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d](https://github.com/nothings/stb/tree/a7d4c2175630e1ea5b6f8dd00ab96fed0b97b96d) | MIT 或公有领域，见 LICENSE | stb_image.h、stb_image_write.h |
| tinyexr | v3.2.0 / [6f470c9ab24bf3992bc512ce07e8ecb00d9bf105](https://github.com/syoyo/tinyexr/tree/6f470c9ab24bf3992bc512ce07e8ecb00d9bf105) | BSD-3-Clause；miniz 为 MIT，见 LICENSE、NOTICE、源码头 | tinyexr.cc/.h、内部读取头、配套 miniz |
| tinygltf | v3.0.1 / [0d9ce1a9561688c6c6e7471430d15a50a0b15f1a](https://github.com/syoyo/tinygltf/tree/0d9ce1a9561688c6c6e7471430d15a50a0b15f1a) | MIT，见 LICENSE | 官方 C11 实现和配套 JSON C 头，用于 glTF/GLB 模型 |

## 使用约定

2026-11-06 为 D03-T02 从同一固定 commit 的 raw 源补入原样 `spirv_glsl.cpp` (SHA256 `7f211747e8e879700119baa7bdd05dc2103e70b5aec297bf751ecc5a3ba3e1c5`) 与 `spirv_glsl.hpp` (`c3580710895edfabda19e4e70efa818b42a71460451ca024b747ebfd335ca43b`). 其 Windows include 闭包完全使用已保留的 core、GLSL 枚举和标准库头; 无新的生成步骤或本地上游源码改动. GL 4.6 builtin 规则在第一方适配中实现.

- stb 仅提供头文件路径。首次实现图像解码时，在实际承担该职责的一个 cpp 中定义 STB_IMAGE_IMPLEMENTATION；需要图像写出时同样只定义一次 STB_IMAGE_WRITE_IMPLEMENTATION。实现宏不放公开头，不传播到所有消费者。
- TinyGLTF 3 使用 C API。模型解析与编辑器场景 JSON 是两条独立的数据入口，不通过 TinyGLTF 保存编辑器场景。
- ImGuizmo 的头文件要求先包含 imgui.h；其公开依赖由 CMake 的 PUBLIC 链接关系传递。
- NVRHI 只供后续 RHI 开发参考，没有 target、include 路径或链接关系。移植时在第一方 RHI 中记录来源与差异。
- MikkTSpace 暂不编译。后续遇到缺少切线且使用法线贴图的模型时，再由资产导入接入。
- Shader 工具链的 Tools/Headers 固定输入取自上述 glslang commit 的 known_good.json, 当前由 ShaderToolchain/CMakeLists.txt 直接声明五个必要静态目标: glslang、默认资源限制、SPIRV-Tools-static、spirv-cross-core 和 spirv-cross-glsl, 不加载上游 CMake 或调用获取脚本. MSVC 对这些目标及 Shader 核心使用 /EHs, 允许 C ABI 桥接中的 C++ 分配异常正确展开, 保持 RTTI 与 Debug 迭代器检查; Tools 的 context 检查仅在 Debug 启用, PCH 为目标私有. SPIRV-Cross 编译 core 与当前 GL 准备消费的 GLSL backend, 不导出其它语言后端.
- 保留的上游实现与头文件不修改算法, build_info.h 与 Tools 六份生成源码的来源见下节. 普通配置/构建不需要 Python、语法 JSON、生成脚本、Git 或 Vulkan SDK. 上次上游 source/CMakeLists.txt 与 update_build_version.py 的构建适配已随未使用的上游构建流程移除.
- glslang 有两项 Physara 私有协议适配: Public/ShaderLang.h 声明 IsReservedIdentifier 并为 Includer 增加默认空操作 RecordInclude; MachineIndependent/Scan.cpp 只读查询原 KeywordMap/ReservedSet, preprocessor/Pp.cpp 在成功 include 处报告原文件/行. 第一方据此提前拒绝词法器关键字及准确记录展开边. 三处源码保留原版权, 不改变语法、搜索、条件处理或 IR 算法. 第一方将源文件的 CRLF/CR 统一为 LF 编译视图以保持行号, 原始字节仍参与身份; 具体合同见 [Interface 说明](../Engine/Shader/Interface/README.md). 该私有头/库须一起重建, 指纹包含实际适配字节.

除源码选择与 CMake 组织外，不修改上游算法。后续升级时从固定 tag/commit 获取源码，保留许可，更新本表及归档校验值，并检查实际消费者的构建。

## GLAD 生成来源

当前保存的是 GLAD 2.0.8 在上述固定 commit 下生成的源码快照，生成环境使用 Jinja2 3.1.6、MarkupSafe 3.0.3：

```powershell
python -m glad --api gl:core=4.6 --extensions "" --out-path build/generated-glad --reproducible c --loader
```

生成头部的 SPDX 为 `(WTFPL OR CC0-1.0) AND Apache-2.0`。普通工程构建不重新运行 GLAD 生成器.

## Shader 固定生成源码

glslang/glslang/build_info.h 来自固定 glslang 的 build_info.h.tmpl 与 CHANGES.md. spirv-tools/source/generated 中保留 core_tables_body.inc、core_tables_header.inc、DebugInfo.h、OpenCLDebugInfo100.h、generators.inc 和 build-version.inc, 分别来自固定 Tools 的表/语言头/registry/版本生成步骤及固定 Headers 的完整语法输入. 生成源码统一为 LF, 保留原版权说明; 版本描述固定为 Tools 的完整 commit, 不采用外层 Git、机器路径或生成日期. 语法表使用固定版本的完整规则, 不按个别测试用例裁剪枚举或改写算法.

只有获取/升级源码时需要 Python 3. 将上表固定归档完整解包到 build/dependency-update 下同名目录, 可使用该版本的上游生成步骤复现快照; 该临时完整源码不回填正式 ThirdParty:

```powershell
$dependencyRoot = (Resolve-Path build/dependency-update).Path
cmake -S "$dependencyRoot/spirv-tools" -B "$dependencyRoot/tools-out" -G "Visual Studio 17 2022" -A x64 "-DSPIRV-Headers_SOURCE_DIR=$dependencyRoot/spirv-headers" -DSPIRV_SKIP_TESTS=ON -DSPIRV_SKIP_EXECUTABLES=ON -DSPIRV_TOOLS_BUILD_STATIC=ON -DSKIP_SPIRV_TOOLS_INSTALL=ON
cmake -E env FORCED_BUILD_VERSION_DESCRIPTION=ef96ed763b43b59b33b31b362f09a02b729fa1c9 cmake --build "$dependencyRoot/tools-out" --config Release --target SPIRV-Tools-static
cmake -S "$dependencyRoot/glslang" -B "$dependencyRoot/glslang-out" -G "Visual Studio 17 2022" -A x64 -DBUILD_EXTERNAL=OFF -DENABLE_OPT=OFF -DENABLE_HLSL=OFF -DENABLE_GLSLANG_BINARIES=OFF -DGLSLANG_TESTS=OFF -DGLSLANG_ENABLE_INSTALL=OFF
```

从 tools-out 取上述六份文件, 从 glslang-out/include/glslang 取 build_info.h, 将 CRLF 规范为 LF 后与正式快照比较. 更新时同步固定来源和裁剪范围, 用实际 Shader 消费者构建并核对一个生成/编译/反射/包读取流程; 不恢复上游测试/工具目录或新增依赖维护框架.

## 源码归档校验值

以下 SHA256 对应 `https://codeload.github.com/<上表仓库>/zip/<固定 commit>` 的原始源码归档，用于核对获取来源，不参与每次 CMake 配置。

| 目录 | 源码归档 SHA256 |
|---|---|
| entt | `6f6518a2eefcc6264dfcbf5bbba5542411cc9bc3b1c3c03882a733a29e229ac8` |
| glad | `c6fe11176ef2dca44f0da370fabcd986e1d06320831950adc622d317d8a60e0a` |
| glfw | `0adf70be965af472c8b8d146d9c795c05510b6c1ef043df3451ac3e2656902e7` |
| glslang | `99c8dc8bafeeec24bc4629e92cf39c667208d0a01c336603759c4e042fbcbe3d` |
| glm | `3685779c341ed1c09a57ac038d119a735a63afb14f2365a24fbc3ffd31afe0db` |
| imgui | `bce023ef1cdd7af5217c76da5a54c9a1f30ea309f06a0cd113e1cefcb05587fa` |
| imguizmo | `754fca4e8c66c5a255f953239b5181385228923f646d1a39bdca5ba18acc7cfd` |
| json | `0746352e4e9532e7aeabbcbaa79079b6bc6008e9261e4d459c9486a9b48a172e` |
| mikktspace | `0fd3d51f10dd521cfb72195c6fc8112fe935a48f7ed86c511f3b2b9ffebea45d` |
| nvrhi | `375d7df03d40e3a458bb2c68ae26a27028f0944f4689b16c5bcea124d4605ed4` |
| spdlog | `2594162a28f06d12bf38ab4388c002156db9f8d5f7bcb0e1b4e60067dbad6c76` |
| spirv-cross | `a60bc3c8166d5c48f667af9a9f35ba9a332f2a33a115ed82ceee6a7398de45cf` |
| spirv-headers | `b33782a6ccd124e6e02634f6382456b0aba2242c6ec5b68d07be20ee77a5b6bc` |
| spirv-tools | `2a7666cfaec2818516019fccb8101a6e57c589417bef83a3c61d19ed63de1c1f` |
| stb | `fc6e7c840a0e025ee238fc09e136452614a4b765afa6ff4177d2e9e1b3493675` |
| tinyexr | `41017d74a7eb1848d6d3a1a031882773582050b2b50c367c353eaa0ed673e790` |
| tinygltf | `fe454348b521e0fdc9bce6e7c24d6a42572ecadcba3a507a217106b3b0cbef01` |

