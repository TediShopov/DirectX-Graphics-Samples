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
// Stored pointer to a range of surfle IDs 
// SurfelList[from SurfelGrid[i] to SurfelGrid[i+1]] is all the surfel that occupy a cell with index I
RWStructuredBuffer<uint> surfelGridUAV : register(u1); 
RWStructuredBuffer<uint> surlfeListUAV : register(u2); // Stored pointers (indices) to the appropriate surfel data
RWStructuredBuffer<uint> surfleStackUAV : register(u3); // Stored pointers (indices) to the appropriate surfel data

groupshared uint groupShareMinCoverage;
groupshared uint groupShareMaxContribution;

float3 ReconstructWorldPosition(float2 uv, float depth, float4x4 invViewProj)
{
    float4 ndc;
    // Convert from UV to NDC [-1, 1]
    ndc.xy = uv * 2.0 - 1.0;
    // For some reason Y has to be flipped
    ndc.y = -ndc.y;
    // Using Raw Depth
    ndc.z =  depth;
    ndc.w = 1.0;
    float4 worldPos = mul( invViewProj,ndc);
    return worldPos.xyz / worldPos.w;
}

uint Hash(uint x)
{
    x ^= x >> 17;
    x *= 0xed5ad4bb;
    x ^= x >> 11;
    x *= 0xac4c1b51;
    x ^= x >> 15;
    x *= 0x31848bab;
    x ^= x >> 14;
    return x;
}

uint RandomUintInRange(uint seed, uint minVal, uint maxVal)
{
    uint range = maxVal - minVal;
    return minVal + (Hash(seed) % range);
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
    surfleStackUAV[0] = 0;

    GroupMemoryBarrierWithGroupSync();

//    bool results[8];
//    runAsserts(results, surfelGridUAV);
//    surfelGridUAV[0] = results[0];
//    surfelGridUAV[1] = results[1];
//    surfelGridUAV[2] = results[2];
//    surfelGridUAV[3] = results[3];

    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    if (dispatchThreadId.x >= gResolution.x || dispatchThreadId.y >= gResolution.y)
        return;

    uint2 tilePos = groupdId.xy;
    uint2 pixelPos = dispatchThreadId.xy;


    int index = pixelPos.x * gResolution.x + pixelPos.y;
    float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);
    float4 atNormal = gNormal.SampleLevel(defaultSampler, uv, 0);
    //Create a random "state" 
    
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    //Find cell in acceleratoin structure


    //Setting up test grid data
    float3 gridOrigin = float3(-2000, -2000, -2000);
    float cellSizeDim = 50;
    float3 cellSize = float3(cellSizeDim, cellSizeDim, cellSizeDim);

    //Compute cell indices
    uint3 cellIndex = ComputeGridIndex(worldPos, gridOrigin, cellSize);
    uint flattenedIndex = HashGridIndex(cellIndex, cellSize);

    uint surfelIdFrom = surfelGridUAV[flattenedIndex];
    uint surfelIdTo = surfelGridUAV[flattenedIndex + 1];

    uint surfelCount = surfelIdTo - surfelIdFrom;
    //

     // Evaluate min coverage value and pixel position.
    // Also evaluate max contribution and surfel index (for handling over-coverage).
    // Also evalute weighted color output (indrect lighting).
    float4 indirectLighting = float4(0,0,0,1);
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
        //uint maxContributionSurfelIndex = randomState.next_uint(cellInfo.surfelCount);
        //Maybe picks a random surfels contribution value to be the initial index ???


        FillAccelerationStructure(Grid, surfelsUAV, surlfeListUAV, surfelGridUAV);

        uint maxContributionSurfelIndex = RandomUintInRange(1234, 0, surfelCount);

        

        //For each surfel into the current Surfle Acceleration Structure Cell
        //In this case is the uniform grid
        for (uint i = surfelIdFrom; i < surfelIdTo; ++i)
        {
            uint surfelIndex = surlfeListUAV[index];
            SurfelData surfel = surfelsUAV[surfelIndex];

            //Bias is relative position from surfel world to the current reconstructed world 
            float3 bias = worldPos - (float3) surfel.position;
            //Optimization that produces the squared distance to avoid compute sqrt()
            float dist2 = dot(bias, bias);

            //If the reconstructed pixel is in the sphere of influence of the surfle
            if (dist2 < surfel.radius * surfel.radius)
            {
                float3 normal = normalize(surfel.normal);
                float dotN = dot((float3) atNormal, normal);

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
//                        life = max(life, surfelRecycleInfo.life);

                    
                    

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

        //varianceEx /= indirectLighting.w;
        //rayCountEx /= indirectLighting.w;


            uint coverageData = 0;

        //coverageData is all packed in UINT 32-bits
        //coverage half-precision [0-16], random (0-255) [16-24], threadX [24-28], threadY [28-32]
            coverageData |= ((f32tof16(coverage) & 0x0000FFFF) << 16);
            coverageData |= ((RandomUintInRange(1234, 0, 255) & 0x000000FF) << 8);
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

    uint coverageData = groupShareMinCoverage;
    float coverage = f16tof32((coverageData & 0xFFFF0000) >> 16);
    uint x = (coverageData & 0x000000F0) >> 4;
    uint y = (coverageData & 0x0000000F) >> 0;


    
    uint kPerCellSurfelLimit = 20;
    uint gPlacementThreshold = 0.1;
    uint gRemovalThreshold = 0.1;
    uint kTotalSurfelLimit = 100;
    if (surfelCount < kPerCellSurfelLimit)
    {
        if (groupThreadID.x == x && groupThreadID.y == y)
        {
            // If seat for surfel in current cell avaliable and coverage is under threshold,
            // genearte new surfel probabilistically.
            if (coverage <= gPlacementThreshold)
            {
                //SURFEL INSERTION INTO ACCELERATION STRUCTURES
//                const float chance = pow(depth, gChancePower);
//                if (randomState.next_float() < chance * gChanceMultiply)
//                {
//                    int freeSurfelCount;
//                    gSurfelCounter.InterlockedAdd((int) SurfelCounterOffset::FreeSurfel, -1, freeSurfelCount);
//
//                    if (0 < freeSurfelCount && freeSurfelCount <= kTotalSurfelLimit)
//                    {
//                        uint validSurfelCount;
//                        gSurfelCounter.InterlockedAdd((int) SurfelCounterOffset::ValidSurfel, 1, validSurfelCount);
//
//                        if (validSurfelCount < kTotalSurfelLimit)
//                        {
//                            uint newIndex = gSurfelFreeIndexBuffer[freeSurfelCount - 1];
//
//                            float varRadius = calcSurfelRadius(
//                                distance(gScene.camera.getPosition(), v.posW),
//                                gFOVy,
//                                gResolution,
//                                kSurfelTargetArea,
//                                kCellUnit
//                            );
//
//                            Surfel newSurfel = Surfel(v.posW, v.normalW, varRadius);
//
//                            newSurfel.radiance = indirectLighting.xyz;
//                            newSurfel.msmeData.mean = indirectLighting.xyz;
//                            newSurfel.msmeData.shortMean = indirectLighting.xyz;
//
//                            gSurfelValidIndexBuffer[validSurfelCount] = newIndex;
//                            gSurfelBuffer[newIndex] = newSurfel;
//                            gSurfelRecycleInfoBuffer[newIndex] = {
//                                kMaxLife, 0u, 0u};
//                                gSurfelGeometryBuffer[newIndex] = hitInfo.data;
//                                gSurfelRefCounter.Store(newIndex, 0);
//                            }
//                        }
//                    }
//                }
            }
        }
    }

        //--SURFEL TO DESTROY --
//        if (cellInfo.surfelCount > 0)
//        {
//            if (groupThreadID.x == x && groupThreadID.y == y)
//            {
//            // If coverage is upper removal threshold,
//            // remove surfel that most contribute to coverage probabilistically.
//                if (coverage > gRemovalThreshold)
//                {
//                    const float chance = pow(depth, gChancePower);
//                    if (randomState.next_float() < chance * gChanceMultiply)
//                    {
//                        uint contributionData = groupShareMaxContribution;
//                        float maxContribution = f16tof32((contributionData & 0xFFFF0000) >> 16);
//                        uint maxContributionSurfelIndex = (contributionData & 0x0000FFFF) >> 0;
//
//                        uint toDestroySurfelIndex = gCellToSurfelBuffer[cellInfo.cellToSurfelBufferOffset + maxContributionSurfelIndex];
//                        Surfel toDestroySurfel = gSurfelBuffer[toDestroySurfelIndex];
//
//                        toDestroySurfel.radius = 0;
//                        gSurfelBuffer[toDestroySurfelIndex] = toDestroySurfel;
//                    }
//                }
//            }
//        }

    
    

        GroupMemoryBarrierWithGroupSync();
    }








