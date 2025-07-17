#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D texSampler;

struct Light
{
    vec4 position;
    vec3 color;
    float radius;
};

layout (set = 1, binding = 1) uniform LightingUniforms
{
    Light lights[2];
    int lightCount;
    float ambient;
    vec3 viewPos;
} lighting;


layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 inColor  = texture(texSampler, fragTexCoord).rgb;

    inColor *= fragColor;
    
    outColor = vec4(0, 0, 0, 1);
    for(int i = 0; i < lighting.lightCount; ++i)
    {
        vec3 vecToLight = lighting.lights[i].position.xyz - fragPos;
        // Distance from light to fragment position
        float dist = length(vecToLight);

        // Viewer to fragment
        vec3 vecToViewer = lighting.viewPos.xyz - fragPos;
        vecToViewer = normalize(vecToViewer);
        
        //if(dist <= lighting.lights[i].radius)
        {
            // Light to fragment
            vecToLight = normalize(vecToLight);

            // Attenuation
            float atten = lighting.lights[i].radius / (pow(dist, 2.0) + 1.0);

            // Diffuse part
            vec3 normal = normalize(fragNormal);
            float NdotL = max(0.0, dot(normal, vecToLight));
            vec3 diff = lighting.lights[i].color * inColor * NdotL * atten;

            // Specular part
            vec3 reflectionVec = reflect(-vecToLight, normal);
            float NdotR = max(0.0, dot(reflectionVec, vecToViewer));
            vec3 spec = lighting.lights[i].color * pow(NdotR, 16.0) * atten;

            outColor += vec4(diff + spec, 0.0);
        }
    }
    // Ambient part
    outColor += lighting.ambient;
}
