// Constants and thresholds
//#include "SurfelUniformGridAccelerationStructure.hlsli"
#include "SurfelASAsserts.hlsli"

#define M_PI 3.1415926

cbuffer SurfelGenCB : register(b0)
{
    uint   FrameIndex;
    float  DepthThreshold;
    float  NormalThreshold;
    float  minRadius;

    uint   MaxSurfels;
    int   CurrentSurfelCount;
    int kPerCellSurfelLimit = 20;
    int gPlacementThreshold = 2;

    int gRemovalThreshold = 0;
    float gChancePower = 1.1;
    float gChanceMultiply = 15;

    float maxRadius;
    
    
    
    
    UniformGrid Grid;
};

cbuffer ProjectionData : register(b1)
{
    matrix invViewProjectionMatrix;
    float depthNear;
    float depthFar;
    float fovY;
}
struct SurfelDebugData
{
    uint4 pointedCell;
};

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
RWTexture2D<float4> outputTexture   : register(u4); // RGB = outputTexture
RWStructuredBuffer<SurfelDebugData> debugUAV : register(u0,space1); 
