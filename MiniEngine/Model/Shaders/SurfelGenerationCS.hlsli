// Constants and thresholds
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
#include "SurfelSpawningUtility.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"

groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;

[numthreads(16, 16, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{

    // Initialize group shared values.
    if (groupIndex == 0)
    {
        groupShareMinCoverage = ~0;
        groupShareMaxContribution = 0;
    }
    //surfleStackUAV[0] = 0;

    GroupMemoryBarrierWithGroupSync();


    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    if (dispatchThreadId.x >= gResolution.x || dispatchThreadId.y >= gResolution.y)
        return;
//    if (dispatchThreadId.x <= 0 || dispatchThreadId.y <= 0)
//        return;

    uint2 tilePos = groupdId.xy;
    uint2 pixelPos = dispatchThreadId.xy;

    uint threadRandomnessSeed = GetThreadTemporalSeed(dispatchThreadId,FrameIndex);

    int index = pixelPos.x * gResolution.x + pixelPos.y;
    //float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);
    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x , gResolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    //Create a random "state" 
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;

    //

     // Evaluate min coverage value and pixel position.
    // Also evaluate max contribution and surfel index (for handling over-coverage).
    // Also evalute weighted color output (indrect lighting).
    if (surfelCount < kPerCellSurfelLimit-2)
    {
        float maxContribution = 0.0f;

        uint maxContributionSurfelIndex = RandomUintInRange(threadRandomnessSeed, 0, surfelCount);
        float coverage = EstimateSurfelCoverage(uv, depthRaw.x, maxContribution, maxContributionSurfelIndex); //Represent how well covered is the grid cell 
        uint coverageData = PackCoverageData(coverage, threadRandomnessSeed, groupThreadID);

            //This is clever trick to utilize InterlockedMin.
            //As the most significant bits are is the coverage and other properties could be though of tiebrakers.
        InterlockedMin(groupShareMinCoverage, coverageData);

            //Analagous but used to define max contribution
        uint contributionData = 0;
        contributionData |= ((f32tof16(maxContribution) & 0x0000FFFF) << 16);
        contributionData |= ((maxContributionSurfelIndex & 0x0000FFFF) << 0);

        InterlockedMax(groupShareMaxContribution, contributionData);

            GroupMemoryBarrierWithGroupSync();

    }

    uint coverageData = groupShareMinCoverage;
    uint3 minCoverageThreadID;
    uint rndThreshold ; 
    uint coverage ; 
    UnpackCoverageData(coverageData, coverage, rndThreshold, minCoverageThreadID);
    
    float normalizedDepth = RemapFloat(LinearizeDepth(depthRaw, depthNear, depthFar), depthNear, depthFar, 0, 1);
    
    
    //--- PROBABILISTIC SURFEL SPAWNING ---
    if (surfelCount < kPerCellSurfelLimit)
    {

        if (groupThreadID.x == minCoverageThreadID.x && groupThreadID.y == minCoverageThreadID.y)
        {
            // If seat for surfel in current cell avaliable and coverage is under threshold,
            // genearte new surfel probabilistically.
            if (coverage < gPlacementThreshold)
            {
                float chanceSpawn = EstimateSpawnChance(coverage,depthRaw.x);
                float changeAgainst = RandomFloat01(threadRandomnessSeed);

                if (changeAgainst < chanceSpawn) {
                    SurfelData newSurfel = SurfelPrototype(worldPos,depthRaw.x, sampledNormal, gResolution.xy);

                    AttemptSpawnSurfel(newSurfel);
                }
            }
        }
    }



    

    //--- PROBABILISTIC SURFEL RECYCLIGN BASED ON COVERAGE ---
    if (surfelCount > 0)
    {
        if (groupThreadID.x == minCoverageThreadID.x && groupThreadID.y == minCoverageThreadID.y)
        {
            // If coverage is upper removal threshold,
            // remove surfel that most contribute to coverage probabilistically.
            if (coverage > gRemovalThreshold)
            {

                float chanceRemove = pow(depthRaw, gChancePower) * gChanceMultiply;
                float changeAgainst = RandomFloat01(threadRandomnessSeed);
//                if (changeAgainst < chanceRemove)
//                {
//                    uint contributionData = groupShareMaxContribution;
//                    float maxContribution = f16tof32((contributionData & 0xFFFF0000) >> 16);
//                    uint maxContributionSurfelIndex = (contributionData & 0x0000FFFF) >> 0;
//
//                    
//
//                    uint toDestroySurfelIndex = surlfeListUAV[maxContributionSurfelIndex];
//                    surfelsUAV[toDestroySurfelIndex].radius = 0;
//
//                    //Decrement surfel stack pointer by one 
//                    uint orig;
//                    InterlockedAdd(surfleStackUAV[0], -1, orig);
//                    InterlockedAdd(surfleStackUAV[1], -1, orig);
//                    //InterlockedAdd(CurrentSurfelCount, -1);
//                    surfleStackUAV[orig-1] = toDestroySurfelIndex;
//
//                    
//                     
//
//                    
//
//                }
            }
        }
    }

    
    

    GroupMemoryBarrierWithGroupSync();
}








