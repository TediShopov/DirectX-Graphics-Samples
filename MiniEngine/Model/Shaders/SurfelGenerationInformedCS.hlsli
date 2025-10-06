// Constants and thresholds
#include "Common.hlsli"
#include "CommonSurfelRegisters.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"

//#define STATIC_BENT_CONE_OFFSET 15.0f
groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;

Texture2D bentCones : register(t2);
Texture2D ambientOcclusion : register(t3);

#include "SurfelSpawningUtility.hlsli"


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
    if (dispatchThreadId.x >= resolution.x || dispatchThreadId.y >= resolution.y)
        return;

    uint2 tilePos = groupdId.xy;
    uint2 pixelPos = dispatchThreadId.xy;

    uint threadRandomnessSeed = GetThreadTemporalSeed(dispatchThreadId,FrameIndex);

    int index = pixelPos.x * resolution.x + pixelPos.y;
    //float2 uv = float2(dispatchThreadId.xy) / float2(resolution.x - 1, resolution.y - 1);
    float2 uv = float2(dispatchThreadId.xy) / float2(resolution.x , resolution.y );
    float4 sampledNormal = gNormal.SampleLevel(defaultSampler, uv, 0);

    //Create a random "state" 
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    uint2 surfelFromTo = ComputeRelevantSurfelRange(worldPos);
    uint surfelCount = surfelFromTo.y - surfelFromTo.x;

    float3 bentNormal = float3(0, 0, 0);
    float radius = 0;
    float cosAngle;
    float height;
    
    //

     // Evaluate min coverage value and pixel position.
    // Also evaluate max contribution and surfel index (for handling over-coverage).
    // Also evalute weighted color output (indrect lighting).
    //if (surfelCount < kPerCellSurfelLimit-2)
    if (true)
    {
        float maxContribution = 0.0f;

        uint maxContributionSurfelIndex = RandomUintInRange(threadRandomnessSeed, 0, surfelCount);


        float coverage = EstimateSurfelCoverageInformed(uv, depthRaw,sampledNormal, maxContribution, maxContributionSurfelIndex);

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
            if (coverage < gPlacementThreshold ) {

                float AO = ambientOcclusion.SampleLevel(defaultSampler, uv, 0);


                float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
                ContributionFromBentCone(worldPos, uv, bentNormal, radius, cosAngle, height,bentCones,ambientOcclusion);
                float RContribution = 1 - RemapFloat(radius, 0, AOVariables.y, 0, 1);
                float contribution = lerp(AO, RContribution, AOVariables.w);

                

                

                //Possibly modify this by the augmented depth value to 
                //make it easier to react in AO in the distance

                if (contribution < AOVariables.x)
                {
                    float spawnChance = EstimateSpawnChance(coverage, depthRaw);
                    float changeAgainst = RandomFloat01(threadRandomnessSeed);
                    if (spawnChance > changeAgainst)
                    {
                        //SurfelData newSurfel = SurfelPrototype(worldPos, depthRaw.x, sampledNormal, resolution.xy);
                        //newSurfel.isSurfelCap = true;
                        //newSurfel.height = 15;
                        SurfelData newSurfel;
                        float v = linearDepth;
                        float calcProjArea = calcProjectArea(10, 250, fovY, resolution.xy);
                        float varRadius = clamp(calcSurfelRadius(v, fovY, resolution.xy, calcProjArea, 100000), minRadius, maxRadius);

                        // --- STATEGY FOR GENERATION SURFEL NearFar Sphere (Surfel NF)
                        //The position used for the sphere (used for contribution and coverage) would the sampled world position
                        newSurfel.position = float4(worldPos, 1);

                        //newSurfel.position = float4(worldPos, 1) + float4(normalize(bentNormal), 0) * height;
                        //newSurfel.position = float4(worldPos, 1);
                        newSurfel.randomValues = float4(changeAgainst, spawnChance, changeAgainst, 1);

                        newSurfel.color = float4(0, 0, 0, 1);

                        newSurfel.contribution = uint4(0, FrameIndex, 0, 0);

                        newSurfel.mean = float4(0, 0, 0, 0);

                        newSurfel.raySamples = float4(10, 0, 0, 0);

                        newSurfel.padding = float3(FrameIndex, FrameIndex, FrameIndex);
                    //newSurfel.normal = sampledNormal;
                        newSurfel.normal = float4(bentNormal, 0);
                    //newSurfel.radius = varRadius;
                        newSurfel.radius = clamp(radius, minRadius, maxRadius);
                        newSurfel.tilePos = tilePos;
                        newSurfel.pixelPos = pixelPos;

                        newSurfel.msme.mean = float4(0, 0, 0, 0);
                        newSurfel.msme.shortMean = float3(0, 0, 0);
                        newSurfel.msme.variance = float3(1e-4, 1e-4, 1e-4);
                        newSurfel.msme.inconsistency = 1;
                        newSurfel.msme.vbbr = 1;

                    
                    //Not a surfel cap by default
                        // The new surfle position that would be calculating the far-field radiance 
                        // can be reconstructed by offsetting the (height) in (bentNormal)
                        newSurfel.isSurfelCap = true;
                        newSurfel.height = height;
                        newSurfel.angle = cosAngle;
                        newSurfel.padSurfelCap = radius;
                        AttemptSpawnSurfel(newSurfel);

                        //AttemptSpawnSurfel(newSurfel);
                    }

            
                }
                else
                {
//                    //Default Surfel Spawnign
                    float chanceSpawn = EstimateSpawnChance(coverage, depthRaw.x);
                    float changeAgainst = RandomFloat01(threadRandomnessSeed);


                    if (chanceSpawn > changeAgainst)
                    {
                        SurfelData newSurfel = SurfelPrototype(worldPos, depthRaw.x, sampledNormal, resolution.xy);
                        AttemptSpawnSurfel(newSurfel);
                    }
            
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

                uint contributionData = groupShareMaxContribution;
                float maxContribution = f16tof32((contributionData & 0xFFFF0000) >> 16);
                uint maxContributionSurfelIndex = (contributionData & 0x0000FFFF) >> 0;
                //float chanceRemove = pow(depthRaw, gChancePower) * gChanceMultiply;
                float chanceRemove = smoothstep(0, 1, maxContribution);
                float changeAgainst = RandomFloat01(threadRandomnessSeed);
                if (changeAgainst < chanceRemove)
                {
                    uint toDestroySurfelIndex = surlfeListUAV[maxContributionSurfelIndex];
                    surfelsUAV[toDestroySurfelIndex].radius = 0;

                    //Decrement surfel stack pointer by one 
                    uint orig;
                    InterlockedAdd(surfleStackUAV[0], -1, orig);
                    InterlockedAdd(surfleStackUAV[1], -1, orig);
                    //InterlockedAdd(CurrentSurfelCount, -1);
                    surfleStackUAV[orig-1] = toDestroySurfelIndex;

                    
                     

                    

                }
            }
        }
    }

    
    

    GroupMemoryBarrierWithGroupSync();
}








