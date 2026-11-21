#include "Backend.hpp"

/// @file
/// @brief 设备创建时查询 Shader 阶段和工作组限额. texture-only 用到的固定 sampler 由设备按需创建.

#include <memory>
#include <utility>

namespace Physara::RHI::OpenGL
{
    bool BuildShaderCaps(OpenGLDevice &device, ShaderCaps &caps)
    {
        // 设备版本不证明 loader 已取得每个实际使用的入口, 查询前先确认准备链完整.
        if (!glGetInteger64v || !glCreateShader || !glShaderSource || !glCompileShader || !glGetShaderiv ||
            !glGetShaderInfoLog || !glDeleteShader || !glCreateProgram || !glAttachShader || !glLinkProgram ||
            !glDetachShader || !glGetProgramiv || !glGetProgramInfoLog || !glDeleteProgram || !glGetProgramResourceiv ||
            !glGetProgramResourceIndex || !glGetUniformLocation || !glGetUniformiv || !glGetAttachedShaders)
        {
            device.Report("CreateDevice.ShaderCaps", "Unsupported: missing program preparation entry");
            return false;
        }
        const auto query = [](GLenum name, GLint &value) { glGetIntegerv(name, &value); };
        query(GL_MAX_VERTEX_UNIFORM_BLOCKS, caps.uniformBlocks[0]);
        query(GL_MAX_FRAGMENT_UNIFORM_BLOCKS, caps.uniformBlocks[1]);
        query(GL_MAX_VERTEX_SHADER_STORAGE_BLOCKS, caps.storageBlocks[0]);
        query(GL_MAX_FRAGMENT_SHADER_STORAGE_BLOCKS, caps.storageBlocks[1]);
        query(GL_MAX_VERTEX_TEXTURE_IMAGE_UNITS, caps.textureUnits[0]);
        query(GL_MAX_TEXTURE_IMAGE_UNITS, caps.textureUnits[1]);
        query(GL_MAX_UNIFORM_BUFFER_BINDINGS, caps.uniformBindings);
        query(GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, caps.storageBindings);
        query(GL_MAX_COMBINED_UNIFORM_BLOCKS, caps.combinedUniformBlocks);
        query(GL_MAX_COMBINED_SHADER_STORAGE_BLOCKS, caps.combinedStorageBlocks);
        query(GL_MAX_COMBINED_TEXTURE_IMAGE_UNITS, caps.combinedTextureUnits);
        query(GL_UNIFORM_BUFFER_OFFSET_ALIGNMENT, caps.uniformAlignment);
        query(GL_SHADER_STORAGE_BUFFER_OFFSET_ALIGNMENT, caps.storageAlignment);
        glGetInteger64v(GL_MAX_UNIFORM_BLOCK_SIZE, &caps.uniformBlockBytes);
        glGetInteger64v(GL_MAX_SHADER_STORAGE_BLOCK_SIZE, &caps.storageBlockBytes);
        query(GL_MAX_VERTEX_ATTRIBS, caps.vertexAttributes);
        query(GL_MAX_VERTEX_ATTRIB_BINDINGS, caps.vertexBindings);
        query(GL_MAX_VERTEX_ATTRIB_RELATIVE_OFFSET, caps.vertexOffset);
        query(GL_MAX_VERTEX_ATTRIB_STRIDE, caps.vertexStride);
        // 直接 Dispatch 使用 Graphics 队列. 单独记录所需入口和计算限额, 供创建及录制边界使用.
        caps.compute = glDispatchCompute && glMemoryBarrier && glBindImageTexture && glGetIntegeri_v;
        if (caps.compute)
        {
            query(GL_MAX_COMPUTE_UNIFORM_BLOCKS, caps.uniformBlocks[2]);
            query(GL_MAX_COMPUTE_SHADER_STORAGE_BLOCKS, caps.storageBlocks[2]);
            query(GL_MAX_COMPUTE_TEXTURE_IMAGE_UNITS, caps.textureUnits[2]);
            query(GL_MAX_IMAGE_UNITS, caps.imageUnits);
            query(GL_MAX_COMPUTE_IMAGE_UNIFORMS, caps.computeImages);
            query(GL_MAX_COMBINED_IMAGE_UNIFORMS, caps.combinedImages);
            query(GL_MAX_COMBINED_SHADER_OUTPUT_RESOURCES, caps.combinedOutputs);
            query(GL_MAX_COMPUTE_WORK_GROUP_INVOCATIONS, caps.workGroupInvocations);
            for (GLuint axis = 0; axis < caps.groupCount.size(); ++axis)
            {
                GLint size = 0;
                GLint count = 0;
                glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_SIZE, axis, &size);
                glGetIntegeri_v(GL_MAX_COMPUTE_WORK_GROUP_COUNT, axis, &count);
                caps.groupSize[axis] = static_cast<std::uint32_t>(size > 0 ? size : 0);
                caps.groupCount[axis] = static_cast<std::uint32_t>(count > 0 ? count : 0);
            }
        }
        if (!device.CheckNative("CreateDevice.ShaderCaps"))
        {
            return false;
        }
        if (caps.uniformAlignment <= 0 || caps.storageAlignment <= 0 || caps.uniformBlockBytes <= 0 ||
            caps.storageBlockBytes <= 0)
        {
            device.Report("CreateDevice.ShaderCaps", "Unsupported: invalid block size or offset alignment");
            return false;
        }
        return true;
    }

    GLuint OpenGLDevice::PrepareDummySampler()
    {
        if (m_DummySampler)
        {
            return m_DummySampler->name;
        }
        // 调用点已进入 owner/current 的原生准备边界. 节点先于 GL 分配, 失败可直接回滚.
        auto record = std::make_unique<NativeDeletion>(NativeKind::Sampler);
        glCreateSamplers(1, &record->name);
        if (record->name != 0)
        {
            glSamplerParameteri(record->name, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glSamplerParameteri(record->name, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glSamplerParameteri(record->name, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(record->name, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glSamplerParameteri(record->name, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);
        }
        if (!CheckNative("PrepareDummySampler", record->name) || record->name == 0)
        {
            ReleaseAllocation(std::move(record), false);
            return 0;
        }
        // Device 仅保存原生删除节点, 避免使用持有 Device 的 Sampler wrapper 形成引用环.
        m_DummySampler = std::move(record);
        return m_DummySampler->name;
    }
} // namespace Physara::RHI::OpenGL
