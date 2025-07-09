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


    

    uint2 tilePos = groupdId.xy;
    uint2 pixelPos = dispatchThreadId.xy;

    uint threadRandomnessSeed = GetThreadTemporalSeed(dispatchThreadId,FrameIndex);


    int index = pixelPos.x * gResolution.x + pixelPos.y;
    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);
    float4 atNormal = gNormal.SampleLevel(defaultSampler, uv, 0);
    //Create a random "state" 
    
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    //Find cell in acceleratoin structure



    //Compute cell indices
    uint3 cellIndex = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, Grid);

    uint surfelIdFrom = surfelGridUAV[flattenedIndex];
    uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];

    uint surfelCount = surfelIdTo - surfelIdFrom;

    //

     // Evaluate min coverage value and pixel position.
    // Also evaluate max contribution and surfel index (for handling over-coverage).
    // Also evalute weighted color output (indrect lighting).
    float4 indirectLighting = float4(0, 0, 0, 1);
    if (true)
    {
        // Other surfel heuristics
        float coverage = 0.f; //Represent how well covered is the grid cell 
        float varianceEx = 0.f; //Heurstic used for convergence
        float rayCountEx = 0.f; //Heuristic used for global-ray budget
        uint refCount = 0;
        uint life = 0; //Heuristic of surfel lifetime

        float maxVariance = 0.f;
        float maxContribution = 0.f;


        uint maxContributionSurfelIndex = RandomUintInRange(threadRandomnessSeed, 0, surfelCount);

        

        //For each surfel into the current Surfle Acceleration Structure Cell
        //In this case is the uniform grid
        for (uint i = surfelIdFrom; i < surfelIdTo; ++i)
        {
            //uint surfelIndex = surlfeListUAV[index];
            uint surfelIndex = surlfeListUAV[i];
            SurfelData surfel = surfelsUAV[surfelIndex];

            //Bias is relative position from surfel world to the current reconstructed world 
            float3 bias = worldPos - (float3) surfel.position;
            //Optimization that produces the squared distance to avoid compute sqrt()
            float dist2 = dot(bias, bias);

            //If the reconstructed pixel is in the sphere of influence of the surfle
            if (dist2 < surfel.radius * surfel.radius)
            {
                float3 normal = normalize(surfel.normal);
                //float dotN = dot((float3) atNormal, normal);
                float dotN = dot(normalize(bias), normal);

                //If normal from GBuffer dotted with surfel normal is positive
                //-> Is surfel facing the general direction of the reconstructed surface
                if (dotN > 0)
                {

                    //Other surfel states
//                    const SurfelRecycleInfo surfelRecycleInfo = gSurfelRecycleInfoBuffer[surfelIndex];
//                    bool isSleeping = surfelRecycleInfo.status & 0x0001;
//                    bool lastSeen = surfelRecycleInfo.status & 0x0002;

                    float dist = sqrt(dist2);
                        //Initial contribution is max
                        //Represent how well the surfel contributs to the cell
                    float contribution = 1.f;

                        //Contributiong calculation
                        //Contribution is in range [0-1]
                    contribution *= saturate(dotN);
                    contribution *= saturate(1 - dist / surfel.radius);
                    contribution = smoothstep(0, 1, contribution);

                        
                    coverage += contribution;

//                    // Delay blending if not sufficient sample is accumulated.
//                        // Because samples are updated per frame, so do not use sample count directly.
//                        indirectLighting += float4(surfel.radiance, 1.f) * contribution * smoothstep(0, gBlendingDelay, surfelRecycleInfo.frame);
//
//                        varianceEx += length(surfel.msmeData.variance) * contribution;
//                        rayCountEx += surfel.rayCount * contribution;
//
//                        refCount = max(refCount, gSurfelRefCounter.Load(surfelIndex));
//                        life = max(life, e);

                    
                    

                    if (maxContribution < contribution)
                    {
                        maxContribution = contribution;
                        maxContributionSurfelIndex = i;
                    }

//                          maxVariance = max(maxVariance, length(surfel.msmeData.variance));


                }
            }
        }


        if (indirectLighting.w > 0)
        {
            indirectLighting.xyz /= indirectLighting.w;
            indirectLighting.w = saturate(indirectLighting.w);

            uint coverageData = 0;

        //coverageData is all packed in UINT 32-bits
        //coverage half-precision [0-16], random (0-255) [16-24], threadX [24-28], threadY [28-32]
            coverageData |= ((f32tof16(coverage) & 0x0000FFFF) << 16);
            coverageData |= ((RandomUintInRange(threadRandomnessSeed, 0, 255) & 0x000000FF) << 8);
            coverageData |= ((groupThreadID.x & 0x0000000F) << 4);
            coverageData |= ((groupThreadID.y & 0x0000000F) << 0);

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

    }

//    uint coverageData = groupShareMinCoverage;
//    float coverage = f16tof32((coverageData & 0xFFFF0000) >> 16);
//    uint rndThreshold = f16tof32((coverageData & 0xFF000000) >> 16);
//    uint x = (coverageData & 0x0000000F) >> 4;
//    uint y = (coverageData & 0x0000000F) >> 0;
    
    uint coverageData = groupShareMinCoverage;
    
    uint x = coverageData & 0xF; // bits 0–3
    uint y = (coverageData >> 4) & 0xF; // bits 4–7
    uint rndThreshold = (coverageData >> 8) & 0xFF; // bits 8–15
    uint coverage = f16tof32((coverageData >> 16) & 0xFFFF); // bits 16–31


    
    
    float linearDepth = LinearizeDepth(depthRaw, depthFar, depthNear);
    float normalizedDepth = RemapFloat(LinearizeDepth(depthRaw, depthNear, depthFar), depthNear, depthFar, 0, 1);
    
    
    if (surfelCount < kPerCellSurfelLimit)
    {
        if (groupThreadID.x == x && groupThreadID.y == y)
        {
            // If seat for surfel in current cell avaliable and coverage is under threshold,
            // genearte new surfel probabilistically.
            if (coverage <= gPlacementThreshold)
            {
                float chanceSpawn = pow(depthRaw, gChancePower) * gChanceMultiply;
                float changeAgainst = RandomFloat01(threadRandomnessSeed);

                if (changeAgainst < chanceSpawn)
                {
                    
//                //Write data to the surfel data as per the poitner in the surfel stack
                //retrieve the stack poitner found at position 0
                    uint prevStackPointer;
                    uint prevSurfelCount;
                    InterlockedAdd(surfleStackUAV[0], 1, prevStackPointer);
                    InterlockedAdd(surfleStackUAV[1], 1, prevSurfelCount);
                    //uint surfelStackPointer = prevStackPointer + 2 + 1;
                    //uint surfelStackPointer = prevStackPointer + 3;
                    //Fill the currently free surfel at stack ptr
                    //uint surfelStackPointer = surfleStackUAV[prevStackPointer];
                    uint surfelStackPointer = prevStackPointer+1;
                    

                    
                    if (surfelStackPointer < MaxSurfels+2)
                    {
                        uint surfelID = surfleStackUAV[prevStackPointer];
                        SurfelData newSurfel;

                        //float v = 1 - depthRaw;
                        float v = linearDepth;
                        float calcProjArea = calcProjectArea(10, 250, fovY, gResolution.xy);
                        float varRadius = clamp(calcSurfelRadius(v, fovY, gResolution.xy, calcProjArea, 100000),minRadius,maxRadius);

                        newSurfel.position = float4(worldPos, 1) + atNormal * 1.0f;
                        newSurfel.normal = atNormal;
                        newSurfel.radius = varRadius;
                        //newSurfel.padding = float3(0, 0, 0);
                        newSurfel.padding = float3(FrameIndex, FrameIndex,FrameIndex);
                        newSurfel.tilePos = tilePos;
                        newSurfel.pixelPos = pixelPos;
                        newSurfel.randomValues = float4(changeAgainst, chanceSpawn, changeAgainst,1);
                        newSurfel.color = float4(0,0,0,1);
                        newSurfel.contribution = uint4(0, FrameIndex, 0, 0);
                        newSurfel.mean = float4(0,0,3,4);
                        newSurfel.raySamples = float4(10, 0, 0, 0);

                        newSurfel.msme.mean = float4(0, 0, 0, 0);
                        newSurfel.msme.shortMean = float3(0, 0, 0);
                        newSurfel.msme.variance = float3(1e-4, 1e-4, 1e-4);
                        newSurfel.msme.inconsistency = 0;
                        newSurfel.msme.vbbr = 1;
                        
                        
                        
                        surfelsUAV[surfelID] = newSurfel;
                    }
                    else
                    {
                        InterlockedAdd(surfleStackUAV[0], -1);
                        InterlockedAdd(surfleStackUAV[1], -1);

                    }
                }
            }
        }
    }



    

        //--SURFEL TO DESTROY --
    if (surfelCount > 0)
    {
        if (groupThreadID.x == x && groupThreadID.y == y)
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








