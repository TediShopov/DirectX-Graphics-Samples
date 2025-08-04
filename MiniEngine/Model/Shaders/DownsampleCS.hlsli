
SamplerState samplerLinearClamp : register(s0);
Texture2D<float> SourceTex : register(t0);     // Input full-res texture (e.g., linear depth or luminance)
RWTexture2D<float> TargetTex : register(u0);  // Output: 1/4 resolution (width / 4, height / 4)

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

    // Average 4x4 texel block from source
    [unroll]
    for (uint y = 0; y < 4; ++y)
    {
        [unroll]
        for (uint x = 0; x < 4; ++x)
        {
            float2 uv = (srcBase + uint2(x, y)) * InvSourceResolution;
            sum += SourceTex.SampleLevel(samplerLinearClamp, uv, 0.0f);
        }
    }

    TargetTex[targetCoord] = sum / 16.0f;
}

