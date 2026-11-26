#version 450 core

// 全屏三角形使用 Pass 的 viewport. 顶点阶段只交付位置, 片元从当前像素读取图资源.
void main()
{
    vec2 corner = vec2(float((gl_VertexIndex << 1) & 2), float(gl_VertexIndex & 2));
    gl_Position = vec4(corner * 2.0 - 1.0, 0.0, 1.0);
}
