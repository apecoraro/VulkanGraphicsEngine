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
    vec3 viewPos;
    float ambient;
    Light lights[2];
    int lightCount;
} lighting;


layout(location = 0) in vec3 fragPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 inColor = texture(texSampler, fragTexCoord).rgb;

    inColor *= fragColor;
    
    //for(int i = 0; i < lightCount; ++i)
    {
        vec3 position = vec3(3, 10, 20);
        vec3 vecToLight = position - fragPos;
        // Distance from light to fragment position
        float dist = length(vecToLight);

        // Viewer to fragment
        vec3 viewPos = vec3(2, 2, 2);
        vec3 vecToViewer = viewPos - fragPos;
        vecToViewer = normalize(vecToViewer);
        
        //if(dist <= lighting.lights[i].radius)
        {
            // Light to fragment
            vecToLight = normalize(vecToLight);

            // Attenuation
            float radius = 120;
            float atten = 1;//radius / (pow(dist, 2.0) + 1.0);

            // Diffuse part
            vec3 normal = normalize(fragNormal);
            float NdotL = max(0.0, dot(normal, vecToLight));
            vec3 color = vec3(1, 1, 1);
            vec3 diffuse = color * inColor * NdotL * atten;

            // Specular part
            vec3 reflectionVec = reflect(-vecToLight, normal);
            float NdotR = max(0.0, dot(reflectionVec, vecToViewer));
            vec3 spec = color * pow(NdotR, 4.0) * atten;

            outColor = vec4(diffuse + spec, 0.0);
            //outColor = vec4(reflectionVec, 1);
        }
    }

    // Ambient part
    //outColor += ambient;
}
