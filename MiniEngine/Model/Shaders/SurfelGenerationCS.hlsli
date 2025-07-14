// Constants and thresholds
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"

groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;
float calcProjectArea(float radius, float distance, float fovy, uint2 resolution)
{
    float projRadius = atan(radius / distance) * max(resolution.x, resolution.y) / fovy;
    return M_PI * projRadius * projRadius;
}

float calcRadiusApprox(float area, float distance, float fovy, uint2 resolution)
{
    return distance * tan(sqrt(area / M_PI) * fovy / max(resolution.x, resolution.y));
}

float calcRadius(float area, float distance, float fovy, uint2 resolution)
{
    float cosTheta = 1 - (area * (1 - cos(fovy / 2)) / M_PI) / (resolution.x * resolution.y);
    float sinTheta = sqrt(1 - pow(cosTheta, 2));
    return distance * sinTheta;
}

float calcSurfelRadius(float distance, float fovy, uint2 resolution, float area, float cellUnit)
{
    return min(calcRadiusApprox(area, distance, fovy, resolution), cellUnit * 2);
}

uint PackCoverageData(float coverage, uint threadRandomnessSeed,uint3 groupThreadID)
{
    uint coverageData = 0;
    coverageData |= ((f32tof16(coverage) & 0x0000FFFF) << 16);
    coverageData |= ((RandomUintInRange(threadRandomnessSeed, 0, 255) & 0x000000FF) << 8);
    coverageData |= ((groupThreadID.x & 0x0000000F) << 4);
    coverageData |= ((groupThreadID.y & 0x0000000F) << 0);
    return coverageData;
}
void UnpackCoverageData(uint coverageData,out float coverage, out uint threadRandomnessSeed,out uint3 groupThreadID)
{
    groupThreadID.x  = coverageData & 0xF; // bits 0–3
    groupThreadID.y = (coverageData >> 4) & 0xF; // bits 4–7
    threadRandomnessSeed = (coverageData >> 8) & 0xFF; // bits 8–15
    coverage = f16tof32((coverageData >> 16) & 0xFFFF); // bits 16–31
}
uint2 ComputeRelevantSurfelRange(float3 worldPos)
{
    uint2 fromTo;
    //Compute cell indices
    uint3 cellIndex = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, Grid);

    fromTo.x = surfelGridUAV[flattenedIndex];
    fromTo.y = surfelGridUAV[flattenedIndex + 1];
    return fromTo;
}


void AttemptSpawnSurfel(SurfelData newSurfel)
{
    uint prevStackPointer, prevSurfelCount;
    InterlockedAdd(surfleStackUAV[0], 1, prevStackPointer);
    InterlockedAdd(surfleStackUAV[1], 1, prevSurfelCount);
    uint surfelStackPointer = prevStackPointer + 1;

    if (surfelStackPointer < MaxSurfels + 2)
    {
        uint surfelID = surfleStackUAV[prevStackPointer];
        surfelsUAV[surfelID] = newSurfel;
    }
    else
    {
        InterlockedAdd(surfleStackUAV[0], -1);
        InterlockedAdd(surfleStackUAV[1], -1);

    }

}
bool IsInSurfelInfluence(float3 relativePosition, SurfelData surfel)
{
    float dist2 = dot(relativePosition, relativePosition);
    return dist2 < surfel.radius * surfel.radius;
}
bool IsInSurfelGeneralDirection(float3 relativePosition, SurfelData surfel, out float dotN)
{
    float3 normal = normalize(surfel.normal);
     dotN = dot(normalize(relativePosition), normal);
    return dotN > 0;

}
void ProbabilistcSpawn(float coverage, uint3 minCoverageThreadID,float3 worldPos, float4 sampledNormal, float4 depthRaw, float2 gResolution)
{
}


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
    if (true)
    {
        float coverage = 0.f; //Represent how well covered is the grid cell 
        float maxContribution = 0.0f;

        uint maxContributionSurfelIndex = RandomUintInRange(threadRandomnessSeed, 0, surfelCount);

        //For each surfel into the current Surfle Acceleration Structure Cell
        //In this case is the uniform grid
        for (uint i = surfelFromTo.x; i < surfelFromTo.y; ++i)
        {
            //uint surfelIndex = surlfeListUAV[index];
            uint surfelIndex = surlfeListUAV[i];
            SurfelData surfel = surfelsUAV[surfelIndex];
            float dotN = 1;

            //Bias is relative position from surfel world to the current reconstructed world 
            float3 bias = worldPos - (float3) surfel.position;
            if (IsInSurfelInfluence(bias, surfel) == false)
                continue;
//            if (IsInSurfelGeneralDirection(bias, surfel, dotN) == false)
//                continue;

            float dist = length(bias);
            float contribution = 1.f;

            contribution *= saturate(dotN);
            contribution *= saturate(1 - dist / surfel.radius);
            contribution = smoothstep(0, 1, contribution);

            coverage += contribution;
            if (maxContribution < contribution)
            {
                maxContribution = contribution;
                maxContributionSurfelIndex = i;
            }
        }



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
    
    float linearDepth = LinearizeDepth(depthRaw, depthFar, depthNear);
    float normalizedDepth = RemapFloat(LinearizeDepth(depthRaw, depthNear, depthFar), depthNear, depthFar, 0, 1);
    
    
    //--- PROBABILISTIC SURFEL SPAWNING ---
    if (surfelCount < kPerCellSurfelLimit)
    {

        if (groupThreadID.x == minCoverageThreadID.x && groupThreadID.y == minCoverageThreadID.y)
        {
            // If seat for surfel in current cell avaliable and coverage is under threshold,
            // genearte new surfel probabilistically.
            if (coverage <= gPlacementThreshold)
            {
                float chanceSpawn = pow(depthRaw, gChancePower) * gChanceMultiply;
                float changeAgainst = RandomFloat01(threadRandomnessSeed);

                if (changeAgainst < chanceSpawn)
                {
                    SurfelData newSurfel;
                    float v = linearDepth;
                    float calcProjArea = calcProjectArea(10, 250, fovY, gResolution.xy);
                    float varRadius = clamp(calcSurfelRadius(v, fovY, gResolution.xy, calcProjArea, 100000), minRadius, maxRadius);

                    newSurfel.position = float4(worldPos, 1) + sampledNormal * 1.0f;
                    newSurfel.randomValues = float4(changeAgainst, chanceSpawn, changeAgainst, 1);
                    newSurfel.color = float4(0, 0, 0, 1);
                    newSurfel.contribution = uint4(0, FrameIndex, 0, 0);
                    newSurfel.mean = float4(0, 0, 3, 4);
                    newSurfel.raySamples = float4(10, 0, 0, 0);
                    newSurfel.padding = float3(FrameIndex, FrameIndex, FrameIndex);
                    newSurfel.normal = sampledNormal;
                    newSurfel.radius = varRadius;
                    newSurfel.tilePos = tilePos;
                    newSurfel.pixelPos = pixelPos;

                    newSurfel.msme.mean = float4(0, 0, 0, 0);
                    newSurfel.msme.shortMean = float3(0, 0, 0);
                    newSurfel.msme.variance = float3(1e-4, 1e-4, 1e-4);
                    newSurfel.msme.inconsistency = 0;
                    newSurfel.msme.vbbr = 1;
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








