#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D texPosition;
layout(set = 1, binding = 1) uniform sampler2D texNormal;
layout(set = 1, binding = 2) uniform sampler2D texAlbedoSpec;

struct Light
{
    vec4 position;
    vec3 color;
    float radius;
};

layout (set = 1, binding = 1) uniform LightingUniforms
{
    vec3 viewPos;
    float ambient;
    Light light;
} lighting;

layout(location = 0) in vec2 lightFragPos;
layout(location = 1) in vec2 fragTexCoord;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 fragPos = texture(texPosition, fragTexCoord).rgb;
    vec3 fragNormal = texture(texNormal, fragTexCoord).rgb;
    vec3 inColor  = texture(texAlbedoSpec, fragTexCoord).rgb;

    outColor = vec4(0, 0, 0, 1);
    vec3 vecToLight = lighting.light.position.xyz - fragPos;
    // Distance from light to fragment position
    float dist = length(vecToLight);

    // Viewer to fragment
    vec3 vecToViewer = lighting.viewPos.xyz - fragPos;
    vecToViewer = normalize(vecToViewer);
        
    if(dist <= lighting.light.radius)
    {
        // Light to fragment
        vecToLight = normalize(vecToLight);

        // Attenuation
        float atten = lighting.light.radius / (pow(dist, 2.0) + 1.0);

        // Diffuse part
        vec3 normal = normalize(fragNormal);
        float NdotL = max(0.0, dot(normal, vecToLight));
        vec3 diff = lighting.light.color * inColor * NdotL * atten;

        // Specular part
        vec3 reflectionVec = reflect(-vecToLight, normal);
        float NdotR = max(0.0, dot(reflectionVec, vecToViewer));
        vec3 spec = lighting.light.color * pow(NdotR, 16.0) * atten;

        outColor += vec4(diff + spec, 0.0);
    }
    // Ambient part
    outColor += lighting.ambient;
}
