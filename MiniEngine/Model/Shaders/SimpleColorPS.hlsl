
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
//#include "SurfelASAsserts.hlsli"
struct PSInput
{
    float4 position : SV_POSITION;
    float4 color : COLOR;
};


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
    uint2 gRes;
    uint numberOfLevels;
    gDepth.GetDimensions(0, gRes.x, gRes.y, numberOfLevels);

    float2 uv = input.position.xy / gRes; 
    //uv.y = 1.0f - uv.y;

    //Reconstruct world position
    float4 depthRaw = gDepth.Sample(defaultSampler, uv);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);

    //Compute cell indices
    uint3 cellIndex = ComputeGridIndex(worldPos, Grid.gridOrigin,Grid.cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, Grid);

    uint numStructs, stride;
    surfelGridUAV.GetDimensions(numStructs, stride);
    if ((flattenedIndex + 1) >= numStructs)
        return float4(0, 0, 1, 1); // Debug blue

    uint surfelIdFrom = surfelGridUAV[flattenedIndex];
    uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];
    uint surfelCount = surfelIdTo - surfelIdFrom;

    float redValue = saturate(RemapFloat(surfelCount, 0, 20, 0, 1));
    float4 colorBasedOnSurfelCount = float4(redValue, 1-redValue, 0, 1);
    return colorBasedOnSurfelCount;
    
    float4 finalColor = colorBasedOnSurfelCount;
    return finalColor;
}
