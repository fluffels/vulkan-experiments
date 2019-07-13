#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=1) uniform sampler2D tex;
layout(binding=3) uniform sampler2D noise;

layout(location=0) out vec4 outColor;

layout(location=0) in vec2 texCoord;
layout(location=1) in flat vec2 gridCoord;

const vec3 green = vec3(0.274509f, 0.537254f, 0.086274f);
const vec3 yellow = vec3(0.933333f, 0.862745f, 0.509803f);

void main() {
    vec4 texColor = texture(tex, texCoord);
	if (texColor.a < 0.85) {
		discard;
	}

	float noise = texture(noise, gridCoord).x;
	vec3 noiseColor = mix(yellow, green, noise);

	vec3 mixedColor = mix(texColor.xyz, noiseColor, 0.9f);

	outColor = vec4(mixedColor.xyz, texColor.w);
}
