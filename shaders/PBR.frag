#version 450
#extension GL_ARB_separate_shader_objects : enable

layout(set = 1, binding = 0) uniform sampler2D texSampler;

struct Light
{
    vec4 position;
    vec3 color;
    float radius;
};

struct Material
{
    float metalness; // how metal is this material vs. dialetic (0 to 1)
    float roughness;
    float ambientOcclusion; // amount of ambient light occlusion (0 to 1)
};

#define LIGHT_COUNT 4
layout (set = 1, binding = 1) uniform PhysicsBasedReflectanceUniforms
{
    Light lights[LIGHT_COUNT];
    vec4 viewPos;
    Material material;
} pbr;

// Calculates the ratio between specular and diffuse reflection using the Fresnel-Schlick approximation.
// cosTheta - cosine of the angle between the vector to camera and the halfway vector.
// F0 - the current material's zero incidence surface reflection parameter.
vec3 FresnelSchlick(float cosTheta, vec3 F0)
{
    return F0 + (1.0 - F0) * pow(clamp(1.0 - cosTheta, 0.0, 1.0), 5.0);
}

#define PI 3.14159265359
// This function statistically approximates the relative surface area of
// microfacets exactly aligned to the (halfway) vector given the current material's
// roughness factor.
// surfaceNormal - normalized surface normal.
// halfwayVector - the vector halfway between the vector to the camera and the vector to the light.
// roughness - the roughness of the current material.
float DistributionGGX(vec3 surfaceNormal, vec3 halfwayVector, float roughness)
{
    float NdotH = max(dot(surfaceNormal, halfwayVector), 0.0);
    float NdotH2 = NdotH * NdotH;
	
    float a2 = roughness * roughness;
    a2 *= a2;
    float denom = (NdotH2 * (a2 - 1.0) + 1.0);
    denom = PI * denom * denom;
	
    return a2 / denom;
}

float GeometrySchlickGGX(float NdotV, float roughness)
{
    float r = (roughness + 1.0);
    float k = (r * r) / 8.0;

    float num   = NdotV;
    float denom = NdotV * (1.0 - k) + k;
	return num / denom;
}

// This function statistically approximates the relative surface area where the
// microfacets of the current material overshadow each other, causing light rays to be occluded.
// Uses a combination of the GGX and Schlick-Beckmann approximation known as Schlick-GGX.
// surfaceNormal - normalized surface vector.
// vecToCamera - vector to camera..
// vecToLight - vector to light.
float GeometrySmith(vec3 surfaceNormal, vec3 vecToCamera, vec3 vecToLight, float roughness)
{
    float NdotV = max(dot(surfaceNormal, vecToCamera), 0.0);
    float NdotL = max(dot(surfaceNormal, vecToLight), 0.0);
    float ggx2  = GeometrySchlickGGX(NdotV, roughness); // geometry obstruction factor (microfacets prevent light from making it to camera)
    float ggx1  = GeometrySchlickGGX(NdotL, roughness); // geometry shadowing factor (microfacet self shadowing)
	
    return ggx1 * ggx2;
}

layout(location = 0) in vec3 fragWorldPos;
layout(location = 1) in vec3 fragColor;
layout(location = 2) in vec2 fragTexCoord;
layout(location = 3) in vec3 fragNormal;

layout(location = 0) out vec4 outColor;

void main()
{
    vec3 surfaceNormal = normalize(fragNormal); 
    vec3 vecToCamera = normalize(pbr.viewPos.xyz - fragWorldPos);

    // If albedo texture is SRGB then need to map into linear space.
    //vec3 albedo = pow(texture(texSampler, fragTexCoord).rgb, 2.2);
    vec3 albedo = texture(texSampler, fragTexCoord).rgb;
    albedo *= fragColor;

    const vec3 kDialeticSurfReflection = vec3(0.04); // most dialetic surfaces look correct with 0.04
    vec3 materialSurfReflection = mix(kDialeticSurfReflection, albedo, pbr.material.metalness);

    vec3 radiance = vec3(0.0);
    for(int i = 0; i < LIGHT_COUNT; ++i) 
    {
        vec3 vecToLight = normalize(pbr.lights[i].position.xyz - fragWorldPos);
        vec3 halfwayVec = normalize(vecToCamera + vecToLight);
        float attenuation = 1.0 / dot(vecToLight, vecToLight);
        vec3 radiance = pbr.lights[i].color * attenuation;
        // The normal distribution function statistically approximates the relative surface area of
        // microfacets exactly aligned to the (halfway) vector.
        float normalDistributionFactor = DistributionGGX(surfaceNormal, halfwayVec, pbr.material.roughness);
        // The geometry function statistically approximates the relative surface area where the material's
        // micro surface-details overshadow each other, causing light rays to be occluded.
        float microfacetOcclusionFactor =
            GeometrySmith(surfaceNormal, vecToCamera, vecToLight, pbr.material.roughness);
        // The fresnel function calculates the ratio between specular and diffuse reflection.
        vec3 specularFactor =
            FresnelSchlick(max(dot(halfwayVec, vecToCamera), 0.0), materialSurfReflection);

        vec3 diffuseFactor = vec3(1.0) - specularFactor;
        // Metal surfaces have no diffuse factor.
        diffuseFactor *= (1.0 - pbr.material.metalness);

        vec3 numerator = normalDistributionFactor * microfacetOcclusionFactor * specularFactor;
        float denominator = 4.0 * max(dot(surfaceNormal, vecToCamera), 0.0) * max(dot(surfaceNormal, vecToLight), 0.0) + 0.0001;
        vec3 specular = numerator / denominator;
            
        // add to outgoing radiance
        float NdotL = max(dot(fragNormal, vecToLight), 0.0);
        radiance += (diffuseFactor * albedo / PI + specular) * radiance * NdotL;
    }

    vec3 ambient = vec3(0.03) * albedo * pbr.material.ambientOcclusion;
    vec3 color = ambient + radiance;

    // Simple Reinhard Tonemap
    color /= (color + vec3(1.0));

    // If writing to SRGB render target then no need to gamma correct
    //outColor = vec4(pow(color, vec3(1.0/2.2)), 1.0);
    outColor = vec4(color, 1.0);
}