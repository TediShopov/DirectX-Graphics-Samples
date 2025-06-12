// Constants and thresholds
#include "Common.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"
#include "SurfelASAsserts.hlsli"




cbuffer SurfelGenCB : register(b0)
{
    uint   FrameIndex;
    float  DepthThreshold;
    float  NormalThreshold;
    float  ViewDistThreshold;
    uint   MaxSurfels;
    uint3   padding;
    UniformGrid Grid;
};

cbuffer ProjectionData : register(b1)
{
    matrix invViewProjectionMatrix;
    float depthNear;
    float depthFar;
}

// G-buffer input textures
Texture2D<float4> gDepth : register(t0); // RGB = world pos
Texture2D<float4> gNormal   : register(t1); // RGB = world normal

RWStructuredBuffer<SurfelData> surfelsUAV : register(u0); // world position
RWStructuredBuffer<uint> surlfeListUAV : register(u1); // Stored pointers (indices) to the appropriate surfel data
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u2); 
//The first index of this structure is the stack pointer
RWStructuredBuffer<uint> surfleStackUAV : register(u3); // Stored pointers (indices) to the appropriate surfel data
groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;

//float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj)
//{
//    float4 ndc;
//    // Convert from UV to NDC [-1, 1]
//    ndc.xy = uv * 2.0 - 1.0;
//    // For some reason Y has to be flipped
//    ndc.y = -ndc.y;
//    // Using Raw Depth
//    ndc.z =  depth;
//    ndc.w = 1.0;
//    float4 worldPos = mul( invViewProj,ndc);
//    return worldPos.xyz / worldPos.w;
//}

//uint Hash(uint x)
//{
//    x ^= x >> 17;
//    x *= 0xed5ad4bb;
//    x ^= x >> 11;
//    x *= 0xac4c1b51;
//    x ^= x >> 15;
//    x *= 0x31848bab;
//    x ^= x >> 14;
//    return x;
//}

//uint RandomUintInRange(uint seed, uint minVal, uint maxVal)
//{
//    uint range = maxVal - minVal;
//    return minVal + (Hash(seed) % range);
//}


[numthreads(1, 1, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{
    //Test if the shader is running properly 
    FillAccelerationStructure(Grid, surfelsUAV, surlfeListUAV, surfelGridUAV);

}








