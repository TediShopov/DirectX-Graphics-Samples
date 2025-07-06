// Constants and thresholds
#include "Common.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"
#include "CommonSurfelRegisters.hlsli"

[numthreads(256, 1, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{

    uint surfelNum, stride;
    surfelsUAV.GetDimensions(surfelNum,stride);
    uint surfelFlatID = dispatchThreadId.x;
    SurfelCount(surfelsUAV[surfelFlatID], Grid, surfelGridUAV);
}








