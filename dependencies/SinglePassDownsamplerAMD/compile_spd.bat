 glslc --target-env=vulkan1.1 -fshader-stage=compute -fentry-point=main -DA_HALF=1 -DSPD_PACKED_ONLY=1 SPDIntegrationLinearSampler.glsl -o ..\..\data\SPDIntegrationLinearSamplerFloat16.spv
 glslc --target-env=vulkan1.1 -fshader-stage=compute -fentry-point=main SPDIntegrationLinearSampler.glsl -o ..\..\data\SPDIntegrationLinearSamplerFloat32.spv
