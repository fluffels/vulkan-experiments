#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(binding=1) uniform sampler2D tex;
layout(binding=3) uniform sampler2D noiseTex;
layout(binding=4) uniform sampler2D texOpacity;

layout(location=0) out vec4 outColor;

layout(location=0) in vec2 geometryTexCoord;
layout(location=1) in flat vec2 gridCoord;
layout(location=2) in flat uint geometryType;

const vec3 green = vec3(0.274509f, 0.537254f, 0.086274f);
const vec3 yellow = vec3(0.933333f, 0.862745f, 0.509803f);

void main() {
	vec2 texCoord = geometryTexCoord;
	texCoord.x += (geometryType % 4) * 0.25f;
	texCoord.y += (geometryType / 4) * 0.25f;

    vec4 texColor = texture(tex, texCoord);
	vec4 texOpacity = texture(tex, texCoord);
	if (texOpacity.r == 0) { discard; }

	float texNoise = texture(noiseTex, texCoord).x;
	if (texNoise < 0.1) { discard; }

	float gridNoise = texture(noiseTex, gridCoord).x;
	vec3 colorVariation = mix(yellow, green, gridNoise);
	vec3 mixedColor = mix(texColor.xyz, colorVariation, 0.7f);

	outColor = vec4(mixedColor.xyz, texColor.w);
}
