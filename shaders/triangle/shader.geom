#version 450

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=3) out;

void main() {
    gl_Position = vec4(0.0, 0.5, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(-0.5, 0.0, 0.0, 1.0);
    EmitVertex();
    gl_Position = vec4(0.5, 0.0, 0.0, 1.0);
    EmitVertex();
    EndPrimitive();
}
