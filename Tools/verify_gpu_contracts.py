from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
CPP_CONTRACT = ROOT / "Engine" / "Renderer" / "GPUContracts.hpp"
GLSL_CONTRACT = ROOT / "Assets" / "Shaders" / "Includes" / "Common.glsl"
GLSL_MATERIAL = ROOT / "Assets" / "Shaders" / "Includes" / "Material.glsl"
SHADER_DIR = ROOT / "Assets" / "Shaders"

# 软告警(信息性:死枚举/重载/布局纪律),不导致失败;硬失败通过抛异常触发 exit(1)。
WARNINGS = []


def evaluate(expr):
    cleaned = expr.strip().rstrip(",;")
    cleaned = re.sub(r"(?<=\d)[uUlL]+", "", cleaned)
    if not re.fullmatch(r"[0-9xXa-fA-F\s<>()|&+\-*/]+", cleaned):
        raise ValueError(f"unsupported expression: {expr}")
    return int(eval(cleaned, {"__builtins__": {}}, {}))


def parse_glsl_defines(text):
    defines = {}
    for match in re.finditer(r"^#define\s+(PHYSARA_[A-Z0-9_]+)\s+((?:0x[0-9a-fA-F]+|[0-9]+)[uUlL]*)\s*$", text, re.MULTILINE):
        defines[match.group(1)] = evaluate(match.group(2))
    return defines


def parse_enum(text, enum_name):
    match = re.search(rf"enum class\s+{enum_name}\s*:\s*std::uint32_t\s*\{{(?P<body>.*?)\n\s*\}};", text, re.DOTALL)
    if match is None:
        raise ValueError(f"missing enum {enum_name}")

    values = {}
    for item in re.finditer(r"^\s*([A-Za-z0-9_]+)\s*=\s*([^,\n]+)", match.group("body"), re.MULTILINE):
        values[item.group(1)] = evaluate(item.group(2))
    return values


def parse_constant(text, name):
    match = re.search(rf"constexpr\s+std::uint32_t\s+{name}\s*=\s*([^;]+);", text)
    if match is None:
        raise ValueError(f"missing constant {name}")
    return evaluate(match.group(1))


def parse_object_flags(text):
    match = re.search(r"namespace\s+ObjectFlags\s*\{(?P<body>.*?)\n\s*\}", text, re.DOTALL)
    if match is None:
        raise ValueError("missing namespace ObjectFlags")

    flags = {}
    for item in re.finditer(r"constexpr\s+std::uint32_t\s+([A-Za-z0-9_]+)\s*=\s*([^;]+);", match.group("body")):
        flags[item.group(1)] = evaluate(item.group(2))
    return flags


def require_equal(label, cpp_value, glsl_value):
    if cpp_value != glsl_value:
        raise AssertionError(f"{label}: C++={cpp_value}, GLSL={glsl_value}")


# ── 结构体布局比对(P.4)─────────────────────────────────────────────
# 按 std140/std430 语义对当前受检类型集(mat4/vecN/uvec4/标量)计算字节尺寸,
# 逐字段名不比对(因 C++ 4×uint32 与 GLSL uvec4 是有意等价),改比对字节布局尺寸。

BASE_SIZE_ALIGN = {
    "mat4": (64, 16), "mat3": (48, 16),
    "vec4": (16, 16), "uvec4": (16, 16), "ivec4": (16, 16),
    "vec3": (12, 16), "vec2": (8, 8),
    "float": (4, 4), "int": (4, 4), "uint": (4, 4), "uint8": (1, 1),
}

CPP_TYPE_MAP = {
    "glm::mat4": "mat4", "glm::mat3": "mat3",
    "glm::vec4": "vec4", "glm::vec3": "vec3", "glm::vec2": "vec2",
    "glm::uvec4": "uvec4", "glm::ivec4": "ivec4",
    "std::uint32_t": "uint", "std::int32_t": "int", "float": "float",
    "std::uint8_t": "uint8",
}

STRUCT_TYPES = ("CameraData", "ObjectData", "MaterialGPUData", "MaterialData",
                "LightData", "ShadowData", "IBLData", "ClusterGridData", "FrameUniforms")


def align_up(value, alignment):
    return (value + alignment - 1) // alignment * alignment


def extract_braced_block(text, header_regex):
    match = re.search(header_regex, text)
    if match is None:
        return None
    open_index = text.index("{", match.start())
    depth = 0
    for index in range(open_index, len(text)):
        if text[index] == "{":
            depth += 1
        elif text[index] == "}":
            depth -= 1
            if depth == 0:
                return text[open_index + 1:index]
    return None


def strip_initializers(body):
    previous = None
    while previous != body:
        previous = body
        body = re.sub(r"\{[^{}]*\}", "", body)
    return body


def parse_cpp_struct(text, name):
    body = extract_braced_block(text, rf"struct\s+(?:alignas\(\d+\)\s+)?{name}\s*\{{")
    if body is None:
        raise ValueError(f"missing C++ struct {name}")
    body = strip_initializers(body)
    members = []
    pattern = (r"(glm::mat4|glm::mat3|glm::vec4|glm::vec3|glm::vec2|glm::uvec4|glm::ivec4|"
               r"std::uint32_t|std::int32_t|std::uint8_t|float|" + "|".join(STRUCT_TYPES) +
               r")\s+([A-Za-z0-9_]+)\s*(?:\[([^\]]+)\])?\s*;")
    for match in re.finditer(pattern, body):
        raw_type = match.group(1)
        members.append((CPP_TYPE_MAP.get(raw_type, raw_type), match.group(3)))
    if not members:
        raise ValueError(f"C++ struct {name} parsed with no members")
    return members


def parse_glsl_struct(text, name):
    body = extract_braced_block(text, rf"struct\s+{name}\s*\{{")
    if body is None:
        raise ValueError(f"missing GLSL struct {name}")
    members = []
    pattern = (r"\b(mat4|mat3|vec4|vec3|vec2|uvec4|ivec4|uint|int|float|" + "|".join(STRUCT_TYPES) +
               r")\s+([A-Za-z0-9_]+)\s*(?:\[([^\]]+)\])?\s*;")
    for match in re.finditer(pattern, body):
        members.append((match.group(1), match.group(3)))
    if not members:
        raise ValueError(f"GLSL struct {name} parsed with no members")
    return members


def resolve_dim(token, consts):
    token = token.strip()
    if token.isdigit():
        return int(token)
    if token in consts:
        return consts[token]
    raise ValueError(f"unresolved array dimension: {token}")


def struct_size(name, structs, consts, cache):
    if name in cache:
        return cache[name]
    offset = 0
    for member_type, dim in structs[name]:
        count = 1 if dim is None else resolve_dim(dim, consts)
        if member_type in structs:
            size = struct_size(member_type, structs, consts, cache)
            align = 16
        elif member_type in BASE_SIZE_ALIGN:
            size, align = BASE_SIZE_ALIGN[member_type]
        else:
            raise ValueError(f"unknown member type '{member_type}' in struct {name}")
        offset = align_up(offset, align)
        stride = size if count == 1 else align_up(size, 16)
        offset += stride * count
    total = align_up(offset, 16)
    cache[name] = total
    return total


def check_layout_discipline(name, members):
    # 布局纪律(D2/2.5):标记 std140 高危成员与未对齐到 16B 的标量组。
    scalar_bytes = 0
    for member_type, _dim in members:
        if member_type in ("vec2", "vec3", "mat3"):
            WARNINGS.append(f"{name}: 成员类型 '{member_type}' 为 std140/std430 对齐风险(建议仅用 vec4/mat4/uvec4)")
        if member_type in ("float", "int", "uint", "uint8"):
            size, _ = BASE_SIZE_ALIGN[member_type]
            scalar_bytes += size
        else:
            if scalar_bytes % 16 != 0:
                WARNINGS.append(f"{name}: 标量组累计 {scalar_bytes}B 未对齐到 16B,存在布局风险")
            scalar_bytes = 0
    if scalar_bytes % 16 != 0:
        WARNINGS.append(f"{name}: 末尾标量组累计 {scalar_bytes}B 未对齐到 16B,存在布局风险")


def verify_struct_layouts(cpp, glsl_common, glsl_material):
    cpp_consts = {
        "MaxShadowCascades": parse_constant(cpp, "MaxShadowCascades"),
        "MaxForwardLights": parse_constant(cpp, "MaxForwardLights"),
    }
    glsl_consts = parse_glsl_defines(glsl_common)

    cpp_names = ["CameraData", "ObjectData", "MaterialGPUData", "LightData",
                 "ShadowData", "IBLData", "ClusterGridData", "FrameUniforms"]
    cpp_structs = {n: parse_cpp_struct(cpp, n) for n in cpp_names}

    glsl_structs = {}
    for n in ["CameraData", "ObjectData", "LightData", "ShadowData", "IBLData",
              "ClusterGridData", "FrameUniforms"]:
        glsl_structs[n] = parse_glsl_struct(glsl_common, n)
    glsl_structs["MaterialData"] = parse_glsl_struct(glsl_material, "MaterialData")

    # 布局纪律只对 CPU 侧受上传结构断言。
    for name in cpp_names:
        check_layout_discipline(name, cpp_structs[name])

    pairs = [
        ("CameraData", "CameraData"),
        ("ObjectData", "ObjectData"),
        ("MaterialGPUData", "MaterialData"),
        ("LightData", "LightData"),
        ("ShadowData", "ShadowData"),
        ("IBLData", "IBLData"),
        ("ClusterGridData", "ClusterGridData"),
        ("FrameUniforms", "FrameUniforms"),
    ]
    cpp_cache, glsl_cache = {}, {}
    for cpp_name, glsl_name in pairs:
        cpp_bytes = struct_size(cpp_name, cpp_structs, cpp_consts, cpp_cache)
        glsl_bytes = struct_size(glsl_name, glsl_structs, glsl_consts, glsl_cache)
        require_equal(f"layout {cpp_name}<->{glsl_name} (bytes)", cpp_bytes, glsl_bytes)


def report_binding_hygiene(buffer_bindings, buffer_define_names):
    # 重载:同一 binding 值被多个枚举名共用。
    by_value = {}
    for name, value in buffer_bindings.items():
        by_value.setdefault(value, []).append(name)
    for value, names in sorted(by_value.items()):
        if len(names) > 1:
            WARNINGS.append(f"binding={value} 被多义重载: {', '.join(sorted(names))}")

    # 死枚举:定义了 binding 但没有任何 shader 引用其 GLSL 宏。
    files = [p for p in SHADER_DIR.rglob("*") if p.suffix in (".glsl", ".vert", ".frag", ".comp")]
    corpus = "\n".join(p.read_text(encoding="utf-8", errors="ignore") for p in files)
    for enum_name, define_name in buffer_define_names.items():
        uses = len(re.findall(rf"\b{define_name}\b", corpus))
        # 减去 Common.glsl 中的 #define 定义自身一次。
        if uses - 1 <= 0:
            WARNINGS.append(f"binding 枚举 {enum_name} (宏 {define_name}) 无 shader 引用,疑似死枚举")


def verify():
    cpp = CPP_CONTRACT.read_text(encoding="utf-8")
    glsl = GLSL_CONTRACT.read_text(encoding="utf-8")
    glsl_material = GLSL_MATERIAL.read_text(encoding="utf-8")
    defines = parse_glsl_defines(glsl)

    require_equal("MaxForwardLights", parse_constant(cpp, "MaxForwardLights"), defines["PHYSARA_MAX_LIGHTS"])
    require_equal("MaxShadowCascades", parse_constant(cpp, "MaxShadowCascades"), defines["PHYSARA_MAX_SHADOW_CASCADES"])

    buffer_bindings = parse_enum(cpp, "GPUBufferBinding")
    buffer_define_names = {
        "FrameUniforms": "PHYSARA_BINDING_FRAME_UNIFORMS",
        "Camera": "PHYSARA_BINDING_CAMERA",
        "Objects": "PHYSARA_BINDING_OBJECTS",
        "Materials": "PHYSARA_BINDING_MATERIALS",
        "Lights": "PHYSARA_BINDING_LIGHTS",
        "InstanceIndices": "PHYSARA_BINDING_INSTANCE_INDICES",
        "PostProcessSettings": "PHYSARA_BINDING_POST_PROCESS_SETTINGS",
        "SkyboxSettings": "PHYSARA_BINDING_SKYBOX_SETTINGS",
        "WorldGridSettings": "PHYSARA_BINDING_WORLD_GRID_SETTINGS",
        "RenderSettings": "PHYSARA_BINDING_RENDER_SETTINGS",
        "Shadow": "PHYSARA_BINDING_SHADOW",
        "IBL": "PHYSARA_BINDING_IBL",
        "MaterialTextureIndices": "PHYSARA_BINDING_MATERIAL_TEXTURE_INDICES",
        "BindlessTextureHandles": "PHYSARA_BINDING_BINDLESS_TEXTURE_HANDLES",
        "ClusterEntries": "PHYSARA_BINDING_CLUSTER_ENTRIES",
        "ClusterLightIndices": "PHYSARA_BINDING_CLUSTER_LIGHT_INDICES",
    }
    for name, define in buffer_define_names.items():
        require_equal(f"GPUBufferBinding::{name}", buffer_bindings[name], defines[define])

    texture_bindings = parse_enum(cpp, "GPUTextureBinding")
    for name, define in {
        "BaseColor": "PHYSARA_BINDING_BASE_COLOR_TEXTURE",
        "MetallicRoughness": "PHYSARA_BINDING_METALLIC_ROUGHNESS_TEXTURE",
        "Normal": "PHYSARA_BINDING_NORMAL_TEXTURE",
        "Occlusion": "PHYSARA_BINDING_OCCLUSION_TEXTURE",
        "Emissive": "PHYSARA_BINDING_EMISSIVE_TEXTURE",
        "Skybox": "PHYSARA_BINDING_SKYBOX_TEXTURE",
        "SceneColor": "PHYSARA_BINDING_SCENE_COLOR_TEXTURE",
        "SceneDepth": "PHYSARA_BINDING_SCENE_DEPTH_TEXTURE",
        "ShadowMap": "PHYSARA_BINDING_SHADOW_MAP",
        "IBLPrefiltered": "PHYSARA_BINDING_IBL_PREFILTERED_TEXTURE",
        "IBLBRDFLut": "PHYSARA_BINDING_IBL_BRDF_LUT",
        "Bloom": "PHYSARA_BINDING_BLOOM_TEXTURE",
        "GBufferBaseColor": "PHYSARA_BINDING_GBUFFER_BASE_COLOR_TEXTURE",
        "GBufferNormal": "PHYSARA_BINDING_GBUFFER_NORMAL_TEXTURE",
        "GBufferMaterial": "PHYSARA_BINDING_GBUFFER_MATERIAL_TEXTURE",
        "GBufferEmissive": "PHYSARA_BINDING_GBUFFER_EMISSIVE_TEXTURE",
    }.items():
        require_equal(f"GPUTextureBinding::{name}", texture_bindings[name], defines[define])

    light_types = parse_enum(cpp, "LightTypeGPU")
    for name, define in {
        "Directional": "PHYSARA_LIGHT_DIRECTIONAL",
        "Point": "PHYSARA_LIGHT_POINT",
        "Spot": "PHYSARA_LIGHT_SPOT",
        "Area": "PHYSARA_LIGHT_AREA",
    }.items():
        require_equal(f"LightTypeGPU::{name}", light_types[name], defines[define])

    shadow_filters = parse_enum(cpp, "ShadowFilterGPU")
    for name, define in {
        "Hard": "PHYSARA_SHADOW_FILTER_HARD",
        "PCF3x3": "PHYSARA_SHADOW_FILTER_PCF_3X3",
        "PCF5x5": "PHYSARA_SHADOW_FILTER_PCF_5X5",
        "Poisson16": "PHYSARA_SHADOW_FILTER_POISSON_16",
        "PCSS": "PHYSARA_SHADOW_FILTER_PCSS",
    }.items():
        require_equal(f"ShadowFilterGPU::{name}", shadow_filters[name], defines[define])

    shading_models = parse_enum(cpp, "ShadingModelGPU")
    require_equal("ShadingModelGPU::Lit", shading_models["Lit"], defines["PHYSARA_SHADING_MODEL_LIT"])
    require_equal("ShadingModelGPU::Unlit", shading_models["Unlit"], defines["PHYSARA_SHADING_MODEL_UNLIT"])

    alpha_modes = parse_enum(cpp, "AlphaModeGPU")
    require_equal("AlphaModeGPU::Opaque", alpha_modes["Opaque"], defines["PHYSARA_ALPHA_OPAQUE"])
    require_equal("AlphaModeGPU::Mask", alpha_modes["Mask"], defines["PHYSARA_ALPHA_MASK"])
    require_equal("AlphaModeGPU::Blend", alpha_modes["Blend"], defines["PHYSARA_ALPHA_BLEND"])

    flags = parse_object_flags(cpp)
    require_equal("ObjectFlags::CastShadow", flags["CastShadow"], defines["PHYSARA_OBJECT_CAST_SHADOW"])
    require_equal("ObjectFlags::ReceiveShadow", flags["ReceiveShadow"], defines["PHYSARA_OBJECT_RECEIVE_SHADOW"])
    require_equal("ObjectFlags::Transparent", flags["Transparent"], defines["PHYSARA_OBJECT_TRANSPARENT"])
    require_equal("ObjectFlags::Unlit", flags["Unlit"], defines["PHYSARA_OBJECT_UNLIT"])

    # 结构体字节布局比对(CPU<->GLSL)。
    verify_struct_layouts(cpp, glsl, glsl_material)

    # binding 卫生报告(死枚举/重载),软告警。
    report_binding_hygiene(buffer_bindings, buffer_define_names)


if __name__ == "__main__":
    try:
        verify()
    except Exception as exc:
        print(f"GPU contract verification failed: {exc}", file=sys.stderr)
        sys.exit(1)
    for warning in WARNINGS:
        print(f"warning: {warning}")
    print("GPU contract verification passed.")
