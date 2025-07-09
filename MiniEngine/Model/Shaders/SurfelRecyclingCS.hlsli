// Constants and thresholds
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
[numthreads(1, 1, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{
    //For now simply remove surfels that have more than X sampled ray count but are below certain 
    //Offsetting for the reserved position
    uint surfelID = dispatchThreadId;

    uint contributions = surfelsUAV[surfelID].contribution.x;
    uint framesSinceUsed = FrameIndex - surfelsUAV[surfelID].contribution.y;
    uint actualSurfelCount = surfleStackUAV[1];
    uint rays = surfelsUAV[surfelID].raySamples.y;

    
    //-- Get Heuristic Pressure from surfel count 
    uint remaining = MaxSurfels - actualSurfelCount;
    float percentReaminaing =((float) actualSurfelCount)  / (float)MaxSurfels ;

    float hSurfelCountWeight = 1;
    float hSurfelCount = pow(percentReaminaing, 3);

    uint3 index = uint3(surfelID, FrameIndex, 1);
    uint seed = GetThreadTemporalSeed(index, FrameIndex);
    float chance = RandomFloat01(seed);


    //Set current contributoins to zero
    //The contributions are meant to be recorded per frame
    //surfelsUAV[surfelID].contribution = 0;

    uint raysThreshold = 500;
    uint contributionThreshold = 20;

    //Time-since-used - how many frames have passed since its last use
    //Surfel coverage - measures local density. If too high surfels are candidates for recycling
    //Surfe Count Pressure - global cap on surfel count. When near limit recycling should be more aggressive
    //Probabilistc recycling

    float irradianceLenght = length(surfelsUAV[surfelID].color);
    
    
    
    

    //if (rays > raysThreshold && (framesSinceUsed ) > 300 && chance < hSurfelCount) 
    //if (rays > raysThreshold && (framesSinceUsed ) > 60 && chance < hSurfelCount)
    if (irradianceLenght<=0.3f && rays > raysThreshold && (framesSinceUsed ) > 60 && chance < hSurfelCount)
    //if ((framesSinceUsed ) > 3000 && chance < hSurfelCount)
    //if (FrameIndex % 60 == 0)
    {
        //Recycle the surfle the surfel


        //Decrement surfel stack pointer by one 
        //If surfel stack pointer is at two cannot decrement more as position 0 and 1 are reserved 
        //for the stack pointer itself and the size
        
        uint orig;
        uint prevSurfelCount;
        InterlockedAdd(surfleStackUAV[0], -1, orig);
        InterlockedAdd(surfleStackUAV[1], -1, prevSurfelCount);
        uint newIndex = orig - 1;
        //surfleStackUAV[orig - 1] = toDestroySurfelIndex;
        //Accesing correct surfel data
        if (newIndex >= 2)
        {
            uint toDestroySurfelIndex = surfelID;
            surfelsUAV[toDestroySurfelIndex].radius = 0;
            surfelsUAV[toDestroySurfelIndex].color = float4(1, 0, 0, 1);
            surfelsUAV[toDestroySurfelIndex].contribution = float4(0, 0, 0, 0);
            surfelsUAV[toDestroySurfelIndex].raySamples.x = 0;
            surfelsUAV[toDestroySurfelIndex].raySamples.y = 0;
            surfleStackUAV[newIndex] = toDestroySurfelIndex;
        }
        else
        {
            InterlockedExchange(surfleStackUAV[0], orig, orig);
            InterlockedExchange(surfleStackUAV[1], prevSurfelCount, prevSurfelCount);
        }
    }

    

    


    
    


}








