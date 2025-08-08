// Constants
#define TILE_COUNT_X 4
#define TILE_COUNT_Y 4
#define SLICE_COUNT  (TILE_COUNT_X * TILE_COUNT_Y)

// Bound resources
Texture2D<float>     gQuarterDepth      : register(t0); // quarter-res depth
Texture2D<float4>    gQuarterRadiance   : register(t1); // quarter-res RGB/HDR
Texture2D<float3>    gQuarterNormal     : register(t2); // quarter-res XY normals 

RWTexture2DArray<float>  gTileDepth     : register(u0); // R16F or R32F
RWTexture2DArray<float4> gTileRadiance  : register(u1); // R11G11B10_FLOAT or RGBA16F
RWTexture2DArray<float3> gTileNormal    : register(u2); // R8G8A8_SNORM

cbuffer SliceParams : register(b0)
{
    uint quarterWidth;
    uint quarterHeight;
    uint tileWidth;
    uint tileHeight;
}

// 8x8 threads per group is safe; adjust to match GPU perf
[numthreads(8, 8, 1)]
void main(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pixel = dispatchThreadID.xy;
    if (pixel.x >= quarterWidth || pixel.y >= quarterHeight)
        return;

    // Which tile (0..3) in X and Y inside the quarter-res image
    uint tileX = pixel.x / tileWidth;
    uint tileY = pixel.y / tileHeight;

    // Which slice index in the Texture2DArray
    uint sliceIndex = tileY * TILE_COUNT_X + tileX;

    // Local pixel coords inside the slice
    uint localX = pixel.x % tileWidth;
    uint localY = pixel.y % tileHeight;

    // Read quarter-res data
    float4 radiance = gQuarterRadiance.Load(int3(pixel, 0));
    float3 normalXY = gQuarterNormal.Load(int3(pixel, 0));
    float  depthVal = gQuarterDepth.Load(int3(pixel, 0));

    // Write to slice
    gTileRadiance[uint3(localX, localY, sliceIndex)] = radiance;
    gTileNormal[uint3(localX, localY, sliceIndex)]   = normalXY;
    gTileDepth[uint3(localX, localY, sliceIndex)]    = depthVal;
}

