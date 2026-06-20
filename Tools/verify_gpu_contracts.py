from pathlib import Path
import re
import sys


ROOT = Path(__file__).resolve().parents[1]
CPP_CONTRACT = ROOT / "Engine" / "Renderer" / "GPUContracts.hpp"
GLSL_CONTRACT = ROOT / "Assets" / "Shaders" / "Includes" / "Common.glsl"


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


def verify():
    cpp = CPP_CONTRACT.read_text(encoding="utf-8")
    glsl = GLSL_CONTRACT.read_text(encoding="utf-8")
    defines = parse_glsl_defines(glsl)

    require_equal("MaxForwardLights", parse_constant(cpp, "MaxForwardLights"), defines["PHYSARA_MAX_LIGHTS"])
    require_equal("MaxShadowCascades", parse_constant(cpp, "MaxShadowCascades"), defines["PHYSARA_MAX_SHADOW_CASCADES"])

    buffer_bindings = parse_enum(cpp, "GPUBufferBinding")
    for name, define in {
        "FrameUniforms": "PHYSARA_BINDING_FRAME_UNIFORMS",
        "Camera": "PHYSARA_BINDING_CAMERA",
        "Objects": "PHYSARA_BINDING_OBJECTS",
        "Materials": "PHYSARA_BINDING_MATERIALS",
        "Lights": "PHYSARA_BINDING_LIGHTS",
        "InstanceIndices": "PHYSARA_BINDING_INSTANCE_INDICES",
        "PostProcessSettings": "PHYSARA_BINDING_POST_PROCESS_SETTINGS",
        "SkyboxSettings": "PHYSARA_BINDING_SKYBOX_SETTINGS",
        "RenderSettings": "PHYSARA_BINDING_RENDER_SETTINGS",
        "Shadow": "PHYSARA_BINDING_SHADOW",
        "IBL": "PHYSARA_BINDING_IBL",
        "MaterialTextureIndices": "PHYSARA_BINDING_MATERIAL_TEXTURE_INDICES",
        "BindlessTextureHandles": "PHYSARA_BINDING_BINDLESS_TEXTURE_HANDLES",
        "ClusterEntries": "PHYSARA_BINDING_CLUSTER_ENTRIES",
        "ClusterLightIndices": "PHYSARA_BINDING_CLUSTER_LIGHT_INDICES",
    }.items():
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

if __name__ == "__main__":
    try:
        verify()
    except Exception as exc:
        print(f"GPU contract verification failed: {exc}", file=sys.stderr)
        sys.exit(1)
    print("GPU contract verification passed.")