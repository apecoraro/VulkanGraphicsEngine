#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 0, binding = 0) uniform MVP {
    mat4 model;
    mat4 view;
    mat4 proj;
} xforms;

layout(location = 0) in vec3 inPosition;
layout(location = 1) in vec3 inColor;
layout(location = 2) in vec2 inTexCoord;
layout(location = 3) in vec3 inNormal;

layout(location = 0) out vec3 fragPos;
layout(location = 1) out vec3 fragColor;
layout(location = 2) out vec2 fragTexCoord;
layout(location = 3) out vec3 fragNormal;

void main()
{
    fragColor = inColor;

    fragTexCoord = inTexCoord;

    mat4 normalTransform = transpose(inverse(xforms.model));
    fragNormal = (normalTransform * vec4(inNormal, 0.0)).xyz;

    fragPos = (xforms.model * vec4(inPosition, 1.0)).xyz;
    gl_Position = xforms.proj * xforms.view * vec4(fragPos, 1.0);
}
