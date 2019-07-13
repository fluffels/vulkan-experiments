#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=2) uniform sampler2D tex;

layout(location=0) out vec4 outColor;

layout(location=0) in vec2 texCoord;

void main() {
    outColor = texture(tex, texCoord);
}
