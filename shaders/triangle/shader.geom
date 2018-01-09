#version 450

layout (binding=0) uniform Uniforms {
    mat4 model;
    mat4 view;
    mat4 proj;
} u;

layout(points) in;
layout(triangle_strip) out;
layout(max_vertices=4) out;

void main() {
    vec4 origin;
    vec4 pos;

    origin = gl_in[0].gl_Position;

    pos = origin + vec4(0.0, 0.0, 0.0, 0.0);
    gl_Position = u.proj * u.view * u.model * pos;
    EmitVertex();

    pos = origin + vec4(0.0, 0.0, 0.99, 0.0);
    gl_Position = u.proj * u.view * u.model * pos;
    EmitVertex();

    pos = origin + vec4(0.99, 0.0, 0.0, 0.0);
    gl_Position = u.proj * u.view * u.model * pos;
    EmitVertex();

    pos = origin + vec4(0.99, 0.0, 0.99, 0.0);
    gl_Position = u.proj * u.view * u.model * pos;
    EmitVertex();

    EndPrimitive();
}
