

SamplerState samplerLinearClamp : register(s0);

Texture2D<float> SourceDepthTex : register(t0);     // Input full-res texture (e.g., linear depth or luminance)
Texture2D SourceDiffuseTex : register(t1);          // Input full-res diffuse light texture
Texture2D SourceNormalTex : register(t2);           // Input full-res normal texture

RWTexture2D<float> TargetDepthTex : register(u0);  // Output: 1/4 resolution (width / 4, height / 4)
RWTexture2D<float4> TargetDiffuseTex : register(u1);        // Output: 1/4 resolution (width / 4, height / 4)
RWTexture2D<float3> TargetNormalTex : register(u2);         // Output: 1/4 resolution (width / 4, height / 4)



static const float FLT_MAX = 3.402823466e+38f;

// Constants
cbuffer CB0 : register(b0)
{
    float2 InvSourceResolution; // 1.0 / (fullWidth, fullHeight)
};

[numthreads(8, 8, 1)]
void main(uint3 DTid : SV_DispatchThreadID)
{
    // Coordinates in target texture (quarter-res)
    uint2 targetCoord = DTid.xy;

    // Convert to source-space (4x4 block)
    uint2 srcBase = targetCoord * 4;

    float sum = 0.0f;

    float minDepth = FLT_MAX;
    float4 diffuseSum = float4(0, 0, 0, 0);
    float3 normalSum = float3(0, 0, 0);



    // Average 4x4 texel block from source
    [unroll]
    for (uint y = 0; y < 4; ++y)
    {
        [unroll]
        for (uint x = 0; x < 4; ++x)
        {
            float2 uv = (srcBase + uint2(x, y)) * InvSourceResolution;

            sum += SourceDepthTex.SampleLevel(samplerLinearClamp, uv, 0.0f);

            float sampledDepth = SourceDepthTex.SampleLevel(samplerLinearClamp, uv, 0.0f);
            minDepth = min(minDepth, sampledDepth);

            diffuseSum += SourceDiffuseTex.SampleLevel(samplerLinearClamp, uv, 0.0f);
            normalSum += SourceNormalTex.SampleLevel(samplerLinearClamp, uv, 0.0f);
            
            
        }
    }

    float4 avgDiffuse = diffuseSum / 16.0f;
    float3 avgNormal = normalize(normalSum / 16.0f);

    
    
    TargetDiffuseTex[targetCoord] = avgDiffuse;
    TargetNormalTex[targetCoord] = avgNormal;
    
    TargetDepthTex[targetCoord]=  minDepth;
}

