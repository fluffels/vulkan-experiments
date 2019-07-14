#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=2) uniform sampler2D tex;
layout(binding=3) uniform sampler2D noiseTex;

layout(location=0) out vec4 outColor;

layout(location=0) in vec2 texCoord;

const vec3 green = vec3(0.274509f, 0.537254f, 0.086274f);
const vec3 yellow = vec3(0.933333f, 0.862745f, 0.509803f);

void main() {
    vec4 texColor = texture(tex, texCoord);

	float noise = texture(noiseTex, texCoord).x;
	vec3 colorVariation = mix(yellow, green, noise);
	vec3 mixedColor = mix(texColor.xyz, colorVariation, 0.8f);

	outColor = vec4(mixedColor.xyz, texColor.w);
}
