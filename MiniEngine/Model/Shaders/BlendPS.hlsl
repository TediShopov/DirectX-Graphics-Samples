
#include "Common.hlsli"
cbuffer CB_BlendControl : register(b0)
{
    float lerpSBGItoInformedSBGI;
    
    
}

Texture2D diffuseLightNonAO : register(t0);
Texture2D diffuseLightAO : register(t1);
Texture2D<float> AO : register(t2);

struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};
float4 main(PSInput input) : SV_TARGET
{
    uint2 res;
    uint numberOfLevels;
    diffuseLightNonAO.GetDimensions(0, res.x, res.y, numberOfLevels);
    float2 uv = input.position.xy / res; 
    float4 sampleNonAODiffuseLight = diffuseLightNonAO.Sample(defaultSampler, uv);
    float4 sampleAODiffuseLight = diffuseLightAO.Sample(defaultSampler, uv);
    float4 sampleAO = AO.Sample(defaultSampler, uv);

    return lerp(sampleNonAODiffuseLight, lerp(sampleNonAODiffuseLight, sampleAODiffuseLight, 1 - sampleAO), lerpSBGItoInformedSBGI);
}
