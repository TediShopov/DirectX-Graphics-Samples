
#include "Common.hlsli"
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};

cbuffer ProjectionData : register(b0)
{
    matrix invViewProjectionMatrix;
    float depthNear;
    float depthFar;
}


Texture2D<float4> depthResource   : register(t0);
Texture2D<float4> rayOutput   : register(t1);


uint3 ComputeGridIndex(float3 position,float3 gridOrigin,float3 cellSize) {

    //return uint3(floor(((position - gridOrigin), cellSize)));
    float3 relative = position - gridOrigin;
    uint3 res;
    return (uint3)floor(relative / cellSize);
    res.x = (uint) floor(relative.x / cellSize.x);
    res.y = (uint) floor(relative.y / cellSize.y);
    res.z = (uint) floor(relative.z / cellSize.z);
    return res;
}



uint HashGridIndex(uint3 gridIdx, uint3 cellSize) {
    return gridIdx.x  +
           gridIdx.y * cellSize.x   +
           gridIdx.z * cellSize.x * cellSize.y ;
}

uint UniqueHashGridIndex(uint3 gridIdx, uint3 cellSize) {
    const uint p1 = 73856093;
    const uint p2 = 19349663;
    const uint p3 = 83492791;
    return (gridIdx.x * p1) ^ (gridIdx.y * p2) ^ (gridIdx.z * p3);
}



float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj)
{
    // Convert from UV to NDC [-1, 1]
    float4 ndc;


    ndc.xy = uv * 2.0 - 1.0;
    ndc.y = -ndc.y;

    //ndc.z = depth * 2.0 - 1.0;       // If depth is linearized

    //ndc.z = (1 - depth);
    ndc.z =  depth;

    
    
    ndc.w = 1.0;

    // Transform from NDC to world
    //float4 worldPos = mul(ndc, invViewProj);
    float4 worldPos = mul( invViewProj,ndc);
    //float4 worldPos = mul(ndc,invViewProj);
    return worldPos.xyz / worldPos.w;
}

float LinearizeDepth(float z, float nearZ, float farZ)
{
    return (nearZ * farZ) / (farZ - z * (farZ - nearZ));
}
//float LinearizeDepth(float z, float nearZ, float farZ)
//{
//    float z_ndc = z * 2.0f - 1.0f; // Back to NDC space
//    return (2.0 * nearZ * farZ) / (farZ + nearZ - z_ndc * (farZ - nearZ));
//}

float RemapFloat(float value, float inMin, float inMax, float outMin, float outMax)
{
    float normalized = (value - inMin) / (inMax - inMin);
    normalized = saturate(normalized);
    return outMin + normalized * (outMax - outMin);
}

static const float4 ColorArray[16] = {
    float4(0.0, 0.0, 0.0, 1.0),     // Black
    float4(1.0, 0.0, 0.0, 1.0),     // Red
    float4(0.0, 1.0, 0.0, 1.0),     // Green
    float4(0.0, 0.0, 1.0, 1.0),     // Blue
    float4(1.0, 1.0, 0.0, 1.0),     // Yellow
    float4(1.0, 0.0, 1.0, 1.0),     // Magenta
    float4(0.0, 1.0, 1.0, 1.0),     // Cyan
    float4(1.0, 0.5, 0.0, 1.0),     // Orange
    float4(0.5, 0.0, 0.5, 1.0),     // Purple
    float4(0.5, 0.5, 0.5, 1.0),     // Gray
    float4(1.0, 1.0, 1.0, 1.0),     // White
    float4(0.5, 0.25, 0.0, 1.0),    // Brown
    float4(0.0, 0.5, 0.0, 1.0),     // Dark Green
    float4(0.0, 0.0, 0.5, 1.0),     // Dark Blue
    float4(0.5, 0.0, 0.0, 1.0),     // Dark Red
    float4(0.5, 0.5, 1.0, 1.0)      // Light Blue
};


float4 main(PSInput input) : SV_TARGET
{
    float2 uv;
    //TEMPORARY RESOLUTION TO USE PIXEL POSITION FROM RAYTRACE OUTPUT
    uv.x = input.position.x / 1920;
    uv.y = input.position.y / 1080;
    float4 res3 = rayOutput.Sample(defaultSampler, uv);
        
    float4 depthRaw = depthResource.Sample(defaultSampler, uv);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv,depthRaw.x,invViewProjectionMatrix);
    
    float3 gridOrigin= float3(-2000, -2000, -2000);
    float cellSizeDim = 50;
    float3 cellSize= float3(cellSizeDim,cellSizeDim,cellSizeDim);
    uint3 index = ComputeGridIndex(worldPos, gridOrigin, cellSize);
    uint indexAll = UniqueHashGridIndex(index, cellSize);
    //indexAll = index.x + index.y + index.z;
    float4 finalColor = ColorArray[indexAll % 16];

//    float4 floatX = ColorArray[index.x % 16];
//    float4 floatY = ColorArray[index.y % 16];
//    float4 floatZ = ColorArray[index.z % 16];
//    float4 finalColor = (floatX + floatY + floatZ) / 3;
    return finalColor;
    
    
    
}
