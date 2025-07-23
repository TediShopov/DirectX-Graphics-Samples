//A header find holding all relevant functions
//for screen space reflections

struct SSRCameraData
{
    matrix cameraViewMatrix;
	matrix cameraProjMatrix;
	matrix cameraWorldMatrix;
	matrix inverseViewMatrix;
	matrix inverseProjMatrix;
    float4 cameraPosition;
};
struct SSRParameters
{
	int useSSR ;
    float maxLengthInWorldUnits;
    int maxSteps;
    float thicknessInUnits;
    float resolution;
	int width;
	int height;
};


float3 getReflectionVector(float3 camPos,float3 worldPosition, float3 worldNormal)
{
    float3 viewDir = normalize(camPos - worldPosition);
    float3 reflectionVector = reflect(-viewDir, worldNormal);
    return normalize(reflectionVector);
	
}
float3 rayMarchReflection(float3 camPos, float3 worldPosition, float3 worldNormal, int i)
{
    return
	worldPosition + getReflectionVector(camPos,worldPosition, worldNormal) * i;

	
}

float2 extractNearFarPlanes(float4x4 projMatrix)
{
    // Extract the c and d components of the projection matrix
    float c = projMatrix._33; // Third row, third column
    float d = projMatrix._43; // Fourth row, third column

    // Compute near and far planes
    float nearPlane = d / (c - 1.0);
    float farPlane = d / (c + 1.0);

    return float2(nearPlane, farPlane);
}
float depthValueToLinearDepth(float nonLinearDepth,SSRCameraData data)
{
	// We are only interested in the depth here
    float4 ndcCoords = float4(0, 0, nonLinearDepth, 1.0f);
	// Unproject the vector into (homogenous) view-space vector
    //float4 viewCoords = mul(inverseProjMatrix, ndcCoords);
    float4 viewCoords = mul(ndcCoords,data.inverseProjMatrix);
	// Divide by w, which results in actual view-space z value
    float linearDepth = viewCoords.z / viewCoords.w;
    return linearDepth;
}
float4 clipSpacePositionFromWorld(float3 worldPosition,SSRCameraData cameraData)
{
    float4 clipSpacePos = mul(float4(worldPosition, 1.0f), cameraData.cameraViewMatrix);
    clipSpacePos = mul(clipSpacePos, cameraData.cameraProjMatrix);
    if(clipSpacePos.w != 0)
		clipSpacePos.xyz /= clipSpacePos.w;
    return clipSpacePos;
}
float4 CSFromW(float3 worldPosition,SSRCameraData cameraData)
{
    float4 clipSpacePos = mul(float4(worldPosition, 1.0f), cameraData.cameraViewMatrix);
    clipSpacePos = mul(clipSpacePos, cameraData.cameraProjMatrix);
    return clipSpacePos;
}
//float3 worldSpacePositionFromNDC(float3 ndcPos, SSRCameraData cameraData)
//{
//
//
//    float4 viewPos = mul(float4(ndcPos,1), cameraData.inverseProjMatrix);
//    viewPos /= viewPos.w; //perspectie divide
//    float3 worldPosition = mul(viewPos, cameraData.inverseViewMatrix);
//    return worldPosition.xyz;
//}
float3 worldSpacePositionFromNDC(float3 ndcPos, SSRCameraData cameraData)
{


    float4 viewPos = mul(float4(ndcPos,1), cameraData.inverseProjMatrix);
    if(viewPos.w != 0)
        viewPos /= viewPos.w; //perspectie divide
    float3 worldPosition = mul(viewPos, cameraData.inverseViewMatrix);
    return worldPosition.xyz;
}


float2 getTexFromNDC(float2 ndcCoordinates)
{
    float2 texCoords = 0.5 * ndcCoordinates.xy + 0.5;
    texCoords.y = 1 - texCoords.y;
    return texCoords;
	
}
float2 getTexFromScreen(float2 screen,float width, float height)
{
    float2 tex = screen;
    tex.x /= width;
    tex.y /= height;
    //Screen origin is at bottom-left, texture is at top-left
    tex.y = 1-tex.y;
    return tex;
	
}

//Uses the fresnel formula adapted from
//https://www.scratchapixel.com/lessons/3d-basic-rendering/introduction-to-shading/reflection-refraction-fresnel.html
float computeReflectionCoefficient(const float3 incidence, const float3 normal, const float reflectivity)
{
    float cosi = clamp(-1, 1, dot(incidence, normal));
    float etai = 1, etat = reflectivity;
    float reflectionCoefficient;
	    
    if (cosi > 0)
    {
        float temp = etai;
        etai = etat;
        etat = temp;
    }
    // Compute sini using Snell's law
    float sint = etai / etat * sqrt(max(0.f, 1 - cosi * cosi));
    // Total internal reflection
    if (sint >= 1) {
        reflectionCoefficient = 1;
    }
    else {
        float cost = sqrt(max(0.f, 1 - sint * sint));
        cosi = abs(cosi);
        float Rs = ((etat * cosi) - (etai * cost)) / ((etat * cosi) + (etai * cost));
        float Rp = ((etai * cosi) - (etat * cost)) / ((etai * cosi) + (etat * cost));
        reflectionCoefficient = (Rs * Rs + Rp * Rp) / 2;
    }
    return reflectionCoefficient;
    // As a consequence of the conservation of energy, the transmittance is given by:
    // kt = 1 - kr;
}



float2 getScreenFromNDC(float2 ndcCoordinates,float width,float height)
{
    //(0.5f * clipSpacePos.xy + clipSpacePos.ww)
    float2 screen = 0.5 * ndcCoordinates.xy + 0.5;
    //screen.y = 1 - screen.y;
    screen.x *= width;
    screen.y *= height;
    return screen;
	
}
float2 getNDCFromScreen(float2 screen,float width,float height)
{
    float2 ndc = screen;
    ndc.x /= width;
    ndc.y /= height;
    //ndc.y = 1 - screen.y;
    ndc.xy= (ndc.xy - 0.5) * 2;
    return ndc;
}
float2 getTexCoordsFromWorld(float3 worldPosition,SSRCameraData cameraData)
{
    float4 clipSpacePos = mul(float4(worldPosition, 1.0f), cameraData.cameraViewMatrix);
    clipSpacePos = mul(clipSpacePos, cameraData.cameraProjMatrix);
    clipSpacePos.xyz /= clipSpacePos.w;
    
    return getTexFromNDC(clipSpacePos.xy);
}
float normalizeDepth(float linearizedDepth, float near, float far)
{
    return linearizedDepth / far;
}


float4 screenSpaceReflectionDebugColors(
float3 worldPosition,
float3 worldNormal,
SSRCameraData ssrCameraData,
SSRParameters ssrParameters
)
{
    const int samples = 200;
    const float thickness = ssrParameters.thicknessInUnits / 200;
    float3 reflection = getReflectionVector(ssrCameraData.cameraPosition, worldPosition, worldNormal);
    float3 endPosition = worldPosition + reflection * ssrParameters.maxLengthInWorldUnits;

    float4 debugColor = float4(1, 1, 1, 1.0f);

    float3 rayClipPosition = clipSpacePositionFromWorld(endPosition, ssrCameraData);

        // Check if the ray is outside the screen space (invalid reflection)
    if (rayClipPosition.x < -1.0f || rayClipPosition.x > 1.0f)
    {
    // Ray has left the horizontal screen bounds
        debugColor = lerp(float4(1.0, 0.0, 0.0, 1.0), debugColor, 0.5);
    }

    if (rayClipPosition.y < -1.0f || rayClipPosition.y > 1.0f)
    {
    // Ray has left the vertical screen bounds
        debugColor = lerp(float4(0.0, 1.0, 0.0, 1.0), debugColor, 0.5);
    }

    if (rayClipPosition.z < 0)
    {
    // Ray is before the near clipping plane
        debugColor = lerp(float4(0.0, 0.0, 1.0, 1.0), debugColor, 0.5);
    }
    if (rayClipPosition.z > 1)
    {
    // Ray is beyond the far clipping plane
        debugColor = lerp(float4(1.0, 1.0, 0.0, 1.0), debugColor, 0.5);
    }

    return debugColor;

}
bool isBetween(float value,float min, float max, float bias)
{
    return (value > (min + bias)) && (value < (max - bias));
	
}

bool isClipped(float3 clipSpacePos)
{
	if (isBetween(clipSpacePos.z, 0, 1, 0.000000000001) == false)
	{
        return true;
        return float3(0, 0, 1);
	}
	if (isBetween(clipSpacePos.x,-1,1,0.0005) == false)
	{
        return true;
        return float3(1, 0, 0);
	        
	}
	if (isBetween(clipSpacePos.y,-1,1,0.0005) == false)
	{
        return true;
        return float3(0, 1, 0);
    }
    return false;
	return float3(0,0,0);
}

//The only difference here is that sample level is used instead of sample
float4 worldSpaceRayMarchCS(
float3 worldPosition,
float3 worldDirection,
SSRCameraData ssrCameraData,
SSRParameters ssrParameters,
Texture2D depthTexture,
Texture2D colorTexture,
SamplerState depthSampler,
float4 skymapColor
)
{
    const float thickness =  ssrParameters.thicknessInUnits;
    const float3 endVector = worldPosition + worldDirection * ssrParameters.maxLengthInWorldUnits;

    float3 newPos = worldPosition;
    [loop]
    for (int i = 1; i <= ssrParameters.maxSteps; ++i)
    {
		//March ray in direction
        float t = (float) i / (float) ssrParameters.maxSteps;
        newPos = lerp(worldPosition, endVector, t);

        float rayDepth =
		clipSpacePositionFromWorld(newPos,ssrCameraData).z;

        rayDepth = depthValueToLinearDepth(rayDepth, ssrCameraData);

        float sampledDepth =
		depthValueToLinearDepth(
        depthTexture.SampleLevel(depthSampler, getTexCoordsFromWorld(newPos,ssrCameraData),0).r,ssrCameraData);

        if (rayDepth > sampledDepth && rayDepth < sampledDepth + thickness)
        {
            return colorTexture
        		.SampleLevel(depthSampler, getTexCoordsFromWorld(newPos,ssrCameraData),0);
        }

        float2 rayScreenPos = getTexCoordsFromWorld(newPos,ssrCameraData);
        if (rayScreenPos.x < 0.0 || rayScreenPos.x > 1.0 ||rayScreenPos.y < 0.0 || rayScreenPos.y > 1.0)
        {
			// Invalid reflection: ray left the screen
            break;
        }

    }
    // Sample reflected color from the cube map
    return skymapColor;

}

float4 worldSpaceRayMarch(
float3 worldPosition,
float3 worldDirection,
SSRCameraData ssrCameraData,
SSRParameters ssrParameters,
Texture2D depthTexture,
Texture2D colorTexture,
SamplerState depthSampler,
float4 skymapColor
)
{
    const float thickness =  ssrParameters.thicknessInUnits/200;
    const float3 endVector = worldPosition + worldDirection * ssrParameters.maxLengthInWorldUnits;

    float3 newPos = worldPosition;
    [loop]
    for (int i = 1; i <= ssrParameters.maxSteps; ++i)
    {
		//March ray in direction
        float t = (float) i / (float) ssrParameters.maxSteps;
        newPos = lerp(worldPosition, endVector, t);

        float rayDepth =
		clipSpacePositionFromWorld(newPos,ssrCameraData).z;

        rayDepth = depthValueToLinearDepth(rayDepth, ssrCameraData);

        float sampledDepth =
		depthValueToLinearDepth(
        depthTexture.Sample(depthSampler, getTexCoordsFromWorld(newPos,ssrCameraData)).r,ssrCameraData);

        if (rayDepth > sampledDepth && rayDepth < sampledDepth + thickness)
        {
            return colorTexture
        		.Sample(depthSampler, getTexCoordsFromWorld(newPos,ssrCameraData));
        }

        float2 rayScreenPos = getTexCoordsFromWorld(newPos,ssrCameraData);
        if (rayScreenPos.x < 0.0 || rayScreenPos.x > 1.0 ||rayScreenPos.y < 0.0 || rayScreenPos.y > 1.0)
        {
			// Invalid reflection: ray left the screen
            break;
        }

    }
    // Sample reflected color from the cube map
    return skymapColor;

}

float4 worldSpaceReflectionsNoSkymap(
float3 worldPosition,
float3 worldNormal,
SSRCameraData ssrCameraData,
SSRParameters ssrParameters,
Texture2D depthTexture,
Texture2D colorTexture,
SamplerState depthSampler,
float4 skymapColor
)
{
    const float thickness =  ssrParameters.thicknessInUnits;
    float3 reflection = getReflectionVector(ssrCameraData.cameraPosition,worldPosition, worldNormal);
    return worldSpaceRayMarch(worldPosition, reflection, ssrCameraData, ssrParameters, depthTexture, colorTexture, depthSampler, skymapColor);
}


float4 screenSpaceReflectionsNoSkymap(
float3 worldPosition,
float3 worldNormal,
SSRCameraData ssrCameraData,
SSRParameters ssrParameters,
Texture2D depthTexture,
Texture2D colorTexture,
SamplerState depthSampler,
float4 reflectionColorSkymap
)
{
    const float thickness =  ssrParameters.thicknessInUnits;
    float3 reflection = getReflectionVector(ssrCameraData.cameraPosition, worldPosition, worldNormal);
    float3 endPosition = worldPosition + reflection * ssrParameters.maxLengthInWorldUnits;

    int width = ssrParameters.width;
    int height = ssrParameters.height;

    bool breaks = false;

    float3 clipSpaceStart = clipSpacePositionFromWorld(worldPosition, ssrCameraData);
    float3 clipSpaceEnd = clipSpacePositionFromWorld(endPosition, ssrCameraData);

    //float4 clipSpacePos = clipSpaceStart;
    float4 clipSpacePos = mul(float4(worldPosition, 1.0f), ssrCameraData.cameraViewMatrix);
    clipSpacePos = mul(clipSpacePos, ssrCameraData.cameraProjMatrix);
    if(clipSpacePos.w <= 0)
        breaks = true;
    if(isBetween(clipSpaceEnd.z, 0, 15, 0.000000000001) == false)
    {
        breaks = true;
	    
    }



    float2 screenSpaceStart = getScreenFromNDC(clipSpaceStart.xy, width,height);
    float2 screenSpaceEnd = getScreenFromNDC(clipSpaceEnd.xy, width,height);

//     // Line rasterization using DDA (or Bresenham’s algorithm)
    float2  screenXYDelta= screenSpaceEnd - screenSpaceStart;
//    //Pick the maximum value that we is going to be used as step.
    //Means that we iterate along the dimension which has the most change
    float steps = max(abs(screenXYDelta.x), abs(screenXYDelta.y) ) * ssrParameters.resolution;
    float2 step = screenXYDelta / (steps);


    float depthDelta = clipSpaceEnd.z - clipSpaceStart.z;
    float depthStep = depthDelta / (steps);

    float2 screenPos = screenSpaceStart;
    float depthCurrent = clipSpaceStart.z;


//    if (steps >= max(width,height))
//        breaks = true;
    float3 unitPositionFrom = normalize(ssrCameraData.cameraPosition - worldPosition);
    //float visibility = 1 - max(dot(unitPositionFrom, reflection), 0);
    float visibility = 1;
    [loop]
    for (int i = 1; i <= steps; i++)      
    {
        screenPos = screenSpaceStart + (step * i);
        depthCurrent = clipSpaceStart.z + (depthStep * i);
        //Perspective Correct Depth
        //depthCurrent = (clipSpaceStart.z * clipSpaceEnd.z) / lerp(clipSpaceEnd.z, clipSpaceStart.z, (float) i / steps);
        float3 clipSpacePos = float3(getNDCFromScreen(screenPos.xy,width,height), depthCurrent);
    	// Invalid reflection: ray left the screen
        if (isClipped(clipSpacePos))
        {
            breaks = true;
        }
        if(breaks == true)
        {
            break;
        }

        float rayDepth      = depthValueToLinearDepth(clipSpacePos.z,ssrCameraData);
        float sampledDepth  = depthValueToLinearDepth(
            depthTexture.Sample(depthSampler, getTexFromScreen(screenPos,width,height)).r,ssrCameraData);

        if (rayDepth > sampledDepth && rayDepth < sampledDepth + ssrParameters.thicknessInUnits)
        {
            float4 reflectionColorTexture =  colorTexture
        		.Sample(depthSampler, getTexFromScreen(screenPos, width, height));
            return lerp(reflectionColorTexture, reflectionColorSkymap, 1-visibility);
        }

    }
    return reflectionColorSkymap;


}

float4 compareWSRayCastingAndSSRayCasting(
float3 worldPosition,
float3 worldNormal,
SSRCameraData ssrCameraData,
SSRParameters ssrParameters,
Texture2D depthTexture,
Texture2D colorTexture,
SamplerState depthSampler,
TextureCube skymap,
SamplerState skymapSampler
)
{
    //Setting up the base parameters
    const int samples = 200;
    const float thickness =  ssrParameters.thicknessInUnits/200;
    float3 worldReflection = getReflectionVector(ssrCameraData.cameraPosition,worldPosition, worldNormal);
    //Getting the world space end
    const float3 worldSpaceEnd = worldPosition + worldReflection * ssrParameters.maxLengthInWorldUnits;


    int width = ssrParameters.width;
    int height = ssrParameters.height;


    //  Clips space Start and end positions 
    float3 clipSpaceStart = clipSpacePositionFromWorld(worldPosition, ssrCameraData);
    float3 clipSpaceEnd = clipSpacePositionFromWorld(worldSpaceEnd, ssrCameraData);

   // clipSpaceEnd.x = clamp(clipSpaceEnd.x, -1.0f, 1.0f);
   // clipSpaceEnd.y = clamp(clipSpaceEnd.y, -1.0f, 1.0f);
   // clipSpaceEnd.z = clamp(clipSpaceEnd.z, 0, 1.0f);


    //The depth of the clip position coordinate is transformed to linear depth
    clipSpaceStart.z = depthValueToLinearDepth(clipSpaceStart.z, ssrCameraData);
    clipSpaceEnd.z = depthValueToLinearDepth(clipSpaceEnd.z, ssrCameraData);


    //Screen space Start and end positions  
    float2 screenSpaceStart = getScreenFromNDC(clipSpaceStart.xy, width,height);
    float2 screenSpaceEnd = getScreenFromNDC(clipSpaceEnd.xy, width,height);

//     // Line rasterization using DDA (or Bresenham’s algorithm)
    float2  screenXYDelta= screenSpaceEnd - screenSpaceStart;
//    //Pick the maximum value that we is going to be used as step.
    //Means that we iterate along the dimension which has the most change
    float steps = max(abs(screenXYDelta.x), abs(screenXYDelta.y) );
    float2 step = screenXYDelta / steps;

    float depthDelta = clipSpaceEnd.z - clipSpaceStart.z;
    float depthStep = depthDelta / steps;





    
    float3 rayLerpedWorldPosition = worldPosition;
    float2 rayLerpedFromScreenPosition = screenSpaceStart;
    float depthCurrent = clipSpaceStart.z;
    [loop]
    for (int i = 1; i <= steps; ++i)
    {
		//March ray in direction
        float t = (float) i / (float) samples;
        rayLerpedWorldPosition = lerp(worldPosition, worldSpaceEnd, t);


        rayLerpedFromScreenPosition = screenSpaceStart + (step * i);
        depthCurrent = clipSpaceStart.z + (depthStep * i);

        float3 clipSpaceFromWorldSpace =
        clipSpacePositionFromWorld(rayLerpedWorldPosition, ssrCameraData);
        clipSpaceFromWorldSpace.z = depthValueToLinearDepth(clipSpaceFromWorldSpace.z,ssrCameraData);
        float3 clipSpaceFromScreenSpace= 
        float3(getNDCFromScreen(rayLerpedFromScreenPosition.xy,width,height), depthCurrent);

        if(clipSpaceFromWorldSpace.x - clipSpaceFromScreenSpace.x > 0.05f)
        {
            float diff = clipSpaceFromWorldSpace.x - clipSpaceFromScreenSpace.x;
            return float4(diff, 0, 0, 0);
        }

        if(clipSpaceFromWorldSpace.y - clipSpaceFromScreenSpace.y > 0.05f)
        {
            float diff = clipSpaceFromWorldSpace.y - clipSpaceFromScreenSpace.y;
            return float4(0, diff, 0, 0);
        }

        if (normalizeDepth(clipSpaceFromWorldSpace.z,0,200) - 
            normalizeDepth(clipSpaceFromScreenSpace.z,0,200) > 0.005f)
        {
            float diff = normalizeDepth(clipSpaceFromWorldSpace.z, 0, 200) -
            normalizeDepth(clipSpaceFromScreenSpace.z, 0, 200);

            return float4(0, 0, 1-diff, 0);
        }

        float sampledDepth =
		depthValueToLinearDepth(
        depthTexture.Sample(depthSampler, getTexCoordsFromWorld(rayLerpedWorldPosition,ssrCameraData)).r,ssrCameraData);


    }
    return 0.0f;

}
