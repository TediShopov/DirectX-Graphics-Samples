
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

static const float M_PI = 3.14159265359;


float3 EvaluateIndirectDiffuseLin(float3 baseColor_lin, float metallic, float3 E_ind) {
    float3 rhoD = baseColor_lin * (1.0 - saturate(metallic));
    return (rhoD * E_ind) / M_PI;
}
float4 main(PSInput input) : SV_TARGET
{
    uint2 res;
    uint numberOfLevels;
    diffuseLightNonAO.GetDimensions(0, res.x, res.y, numberOfLevels);
    float2 uv = input.position.xy / res; 
    // If texDiffuse is created as an sRGB SRV, read is already linear; else convert:
    //float3 baseColor_lin = texDiffuse.Sample(defaultSampler, uv).rgb;
    float3 baseColor_lin = float3(1, 1, 1);
       // baseColor_lin = SRGBToLinear(baseColor_lin); // if needed

//float metallic = texMetallic.Sample(defaultSampler, uv);
    float metallic = 0.5f;

// Your existing lighting (colorSum) ...

// Indirect GI add:
float3 E_ind = diffuseLightNonAO.Sample(defaultSampler, uv); // irradiance, linear HDR
float3 L_ind = EvaluateIndirectDiffuseLin(baseColor_lin, metallic, E_ind);
    return float4(L_ind, 1);
//colorSum += L_ind;

//mrt.Color  = float4(colorSum, 1);
    //return col
    
    
    
    
   // float4 sampleNonAODiffuseLight = diffuseLightNonAO.Sample(defaultSampler, uv);
   // float4 sampleAODiffuseLight = diffuseLightAO.Sample(defaultSampler, uv);
   // float4 sampleAO = AO.Sample(defaultSampler, uv);


   // //return lerp(sampleNonAODiffuseLight, lerp(sampleNonAODiffuseLight, sampleAODiffuseLight, 1 - sampleAO), lerpSBGItoInformedSBGI);
   // return lerp(sampleNonAODiffuseLight, lerp(sampleNonAODiffuseLight, sampleAODiffuseLight, 1-sampleAO), lerpSBGItoInformedSBGI);
}
