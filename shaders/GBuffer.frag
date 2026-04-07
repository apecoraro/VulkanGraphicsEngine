#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D texSampler;

layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outPosition;
layout(location = 1) out vec4 outNormal;
layout(location = 2) out vec4 outAlbedoSpec;

void main()
{
    outPosition = vec4(fragPos, 1.0);
    outNormal = vec4(normalize(fragNormal), 1.0);
    outAlbedoSpec = vec4(texture(texSampler, fragTexCoord).rgb * fragColor, 1.0);
}
