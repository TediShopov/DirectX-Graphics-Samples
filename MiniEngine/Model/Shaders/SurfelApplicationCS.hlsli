// Constants and thresholds
#include "Common.hlsli"
//#include "SurfelUniformGridAccelerationStructure.hlsli"
#include "CommonSurfelRegisters.hlsli"



//Calculate the surfel contribution by using Mahalonobis distance and angular fallof.
//The actual inverse covariance matrix need be calculated. 
//A scalling factor based on the scene must be supplied for appropriate results.
float3 calculateSurfelsContribution_WActualCovarianceMatrix(SurfelData surfel, float3 worldPos, float sclalingFactor)
{
    
    float3 colorContribution = float3(0, 0, 0);
        
//    float3 d = worldPos - surfel.position;
//    float3 L = -normalize(d); // light dir toward surfel
//    float NdotL = saturate(dot(surfel.normal, L));
//
//    if (NdotL <= 0.0f)
//    {
//        return colorContribution;
//        
//    }
//
//    float3x3 covarianceInverse = 0;
//    covarianceInverse._m00_m01_m02 = surfel.co1;
//    covarianceInverse._m10_m11_m12 = surfel.co2;
//    covarianceInverse._m20_m21_m22 = surfel.co3;
//
//    float3 dTransformed = mul(covarianceInverse, d);
//    float D2 = dot(d, dTransformed); // Mahalanobis distance squared
//    D2 /= sclalingFactor;
//    float w = exp(-0.5 * D2);
//    colorContribution = surfel.color * NdotL * w;;
    return colorContribution;
    
}


//Compute the surfel contributing color based on Mahalonobis-like metric and angular fallof.
// The mahalonobis metric does NOT use an inverse covariance matrix but instead the 
// distance is "squashed" by a factor in the surfels normal direction
float3 calculateSurfelsContribution_MahalonobisLikeMetric(SurfelData surfel, float3 worldPos, float squshFactor=2, float relScalingFactor = 2)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = worldPos - surfel.position;

        //  Ellipsoidal distance without covariance matrix 
    float dDotN = dot(d, surfel.normal);
    float3 tangentOffset = d - dDotN * surfel.normal;
    float3 normalOffset = dDotN * surfel.normal;

    float3 squashOffset = tangentOffset + squshFactor * normalOffset;
    float D2 = dot(squashOffset, squashOffset);
    D2 /= pow(surfel.radius, relScalingFactor);
    float w = exp(-0.5 * D2); // Spatial weight

    colorContribution = surfel.color * w;

    return colorContribution;
        
}

//Compute the surfel contributing color based on Mahalonobis-like metric and angular fallof.
// The mahalonobis metric does NOT use an inverse covariance matrix but instead the 
// distance is "squashed" by a factor in the surfels normal direction
float3 calculateSurfelsContribution_MahalonobisLikeMetricAndAngularFallof(SurfelData surfel, float3 worldPos, float3 interpolatedNormal,float squshFactor=2, float relScalingFactor = 2)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = worldPos - surfel.position;

        //  Ellipsoidal distance without covariance matrix 
    float dDotN = dot(d, surfel.normal);
    float3 tangentOffset = d - dDotN * surfel.normal;
    float3 normalOffset = dDotN * surfel.normal;

    float squash = 2.0f; // faster falloff in normal direction
    float3 squashOffset = tangentOffset + squash * normalOffset;
    float D2 = dot(squashOffset, squashOffset);
    D2 /= pow(surfel.radius, relScalingFactor);

    float w = exp(-0.5 * D2); // Spatial weight

        // Angular weighting (optional, like PICA PICA) 
    float NdotN = saturate(dot(interpolatedNormal, surfel.normal));
    w *= NdotN * NdotN;

    float3 L = -normalize(d); // light direction from surfel
    float NdotL = saturate(dot(surfel.normal, L));
    if (NdotL > 0.0f)
    {
        colorContribution = surfel.color * NdotL * w;;
    }
    return colorContribution;
        
}
float angularWeight(float3 worldPos ,float3 surfelPosition, float3 surfelNormal)
{
    float3 L = normalize(worldPos - surfelPosition); // Light Direction out of the surfel
    return saturate(dot(L, surfelNormal));
    //return saturate(dot(surfelNormal, L));
    
}

float3 calculateSurfelsContribution_Experimental(SurfelData surfel, float3 worldPos, float3 interpolatedNormal)
{
    float3 colorContribution = float3(0, 0, 0);
    float3 d = length(worldPos - surfel.position);

     float contribution = 1.f;
    float dotN = dot(interpolatedNormal, normalize(surfel.normal));

    contribution *= saturate(dotN);
    contribution *= saturate(1 - d / surfel.radius);
    contribution = smoothstep(0, 1, contribution);

//    float w; // Spatial weight
//    float maxDistance = surfel.radius; 
//    float t = clamp(d / maxDistance, 0.0, 1.0); // Normalize distance
//    float attenuation = smoothstep(0.0, 1.0, 1.0 - t); // Smoothstep attenuation


    colorContribution = surfel.color * contribution;
    return colorContribution;
        
}
//float3 calculateSurfelsContribution_Experimental2(SurfelData surfel, float3 worldPos, float3 interpolatedNormal)
//{
//    float3 colorContribution = float3(0, 0, 0);
//    float3 d = worldPos - surfel.position;
//
//
//    float3 r      = p_r - p_s;
//float  r_len  = length(r);
//float3 d      = r / r_len;
//
//float cosTheta_s = saturate(dot(n_s, d));
//float cosTheta_r = saturate(dot(n_r, -d));
//
//// Lambertian BRDF approximation for surfel emitter
//    float3 Lo = (I_s / PI) * (cosTheta_s * cosTheta_r) / (r_len * r_len);
//
//    return Lo;
//        
//}

float3 computeRadianceForWorldPos(float3 worldPos, float3 worldNormal)
{
    
  //Get all for now
    //TODO make a bounding box around the surfel to only allow surfel in some range
    //TODO make a raycast toward surfels position to see if it contributes
    //uint3 index = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
    float3 colorE = float3(0, 0, 0);
    uint surfelNum = 0;
    uint surfelStride = 0;

    
    uint3 idx = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);

    uint linearIndex = HashGridIndex(idx, Grid);
    uint surfelListIndexFrom = surfelGridUAV[linearIndex];
    uint surfelListIndexTo = surfelGridUAV[linearIndex + 1];

    

    
    

    for (uint i = surfelListIndexFrom; i < surfelListIndexTo; ++i)
    {

        uint index = surlfeListUAV[i];
        SurfelData surfel = surfelsUAV[index];
        float3 d = worldPos - surfel.position;
        float3 colorContribution ;
        if(surfel.radius <= 0)
        {
            colorContribution = float3(0, 0, 0);
        }
        else
        {
            colorContribution = calculateSurfelsContribution_Experimental(surfel, worldPos, worldNormal);
            //colorContribution = calculateSurfelsContribution_MahalonobisLikeMetric(surfel, worldPos);
        }
       


        //ApplySurfels
        //Color Intensity Contribution

        //A hacky way to estimate relative contribution of a surfel to the pixel
        float colorIntensity = length(colorContribution);

        float intensityThreshold = 0.01f;
        if (colorIntensity > intensityThreshold)
        {
            //Add to the contributions metric
            InterlockedAdd(surfelsUAV[index].contribution.x, 1);
            //Store the last frame used
            uint outO;
            InterlockedExchange(surfelsUAV[index].contribution.y, FrameIndex, outO);

        }
        colorE += colorContribution;
    }
    return colorE;

}

[numthreads(32, 32, 1)]
void main(
    uint3 dispatchThreadId: SV_DispatchThreadID,
    uint groupIndex: SV_GroupIndex,
    uint3 groupThreadID: SV_GroupThreadID,
    uint3 groupdId: SV_GroupID
)
{


    float3 gResolution;
    gDepth.GetDimensions(0, gResolution.x, gResolution.y, gResolution.z);
    if (dispatchThreadId.x >= gResolution.x || dispatchThreadId.y >= gResolution.y)
        return;
    uint2 pixelPos = dispatchThreadId.xy;



    

        float2 uv = float2(dispatchThreadId.xy) / float2(gResolution.x - 1, gResolution.y - 1);
    
    //Reconstruct world-position
    float4 depthRaw = gDepth.SampleLevel(defaultSampler, uv, 0);
    float depthRange = 0.005;
    float3 worldPos = ReconstructWorldPosition(uv, depthRaw.x, invViewProjectionMatrix);
    float3 worldNormal = gNormal.SampleLevel(defaultSampler, uv, 0);
    //Fill Debug Data
    if (pixelPos.x == (gResolution.x / 2) &&  pixelPos.y == (gResolution.y / 2))
    {

        uint3 idx = ComputeGridIndex(worldPos, Grid.gridOrigin, Grid.cellSize);
        debugUAV[0].pointedCell = uint4(idx.xyz, 1);
        
    }


    //Compute the color for this pixel
    outputTexture[pixelPos] = float4(computeRadianceForWorldPos(worldPos, worldNormal), 1);




}



    








