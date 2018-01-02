#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=1) uniform sampler2D tex;

layout(location=0) in vec3 f_col;
layout(location=1) in vec2 f_tex;

layout(location=0) out vec4 outColor;

void main() {
    outColor = texture(tex, f_tex);
}
