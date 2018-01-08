#version 450

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(points) in;
layout(points) out;
layout(max_vertices=1) out;

void main() {
    gl_Position = gl_in[0].gl_Position;
    EmitVertex();
    EndPrimitive();
}
