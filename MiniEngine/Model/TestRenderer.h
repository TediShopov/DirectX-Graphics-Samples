#pragma once
#include <d3d12.h>
//#include "SerializableMesh.h"
//#include "MeshInstance.h"
#include "Geometry/SphereMesh.h"
#include "Geometry/DiscMesh.h"
#include "Geometry/Transform.h"
#include "IParameterBlock.h"


class GraphicsContext;
class ShadowCamera;
class ModelH3D;
class ExpVar;

namespace Math
{
    class Camera;
    class Vector3;
}
class SurfelGI;
class HashGridVisualization;
class MSMEVisualization;
class SurfelGIOnlyVisualization;
class SurfelSpawnChanceVisualization;
class HBIL;
class HBILInterleaved;
class GBufferDownsample;
class GBufferSlice;
using namespace Math;

//namespace TestRenderer
class TestRenderer 
{
public:
	//--- DEFINES ---

	enum eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };

#define RENDER_SCENE_DEFAULT_PARAMS GraphicsContext& gfxContext, const Math::Camera& camera,const D3D12_VIEWPORT& viewport,const D3D12_RECT& scissor,bool skipDiffusePass = false,bool skipShadowMap = false
#define RENDER_SCENE_PARAMS GraphicsContext& gfxContext, const Math::Camera& camera,const D3D12_VIEWPORT& viewport,const D3D12_RECT& scissor,bool skipDiffusePass ,bool skipShadowMap
#define RENDER_OBJECT_INSTANCE_PARAMS GraphicsContext& gfxContext, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter


	//--- DATA ---
#pragma region ExposedProperties
	static UINT frameIndex;

	static SurfelGI* SurfelIllumination;
	static HashGridVisualization* GridVisualization;
	static MSMEVisualization* GridMSMEVisualization;
	static SurfelSpawnChanceVisualization* SurfelSpawnVisualization;
	static SurfelGIOnlyVisualization* SurfelGIVisualization;
	static SurfelGIOnlyVisualization* MaterialBindingDebug;
	static HBIL* m_HBIL;
	static HBILInterleaved* m_HBILInterleaved;
	static GBufferDownsample* m_GBufferDownsample;
	static GBufferSlice* m_GBufferSlice;

	static SphereMesh* m_Sphere;
	static DiscMesh* m_Disc;
	static Transform m_Transform;

	//-- DIRECTIONAL LIGHT PROPERTIES
	static Math::Vector3 m_SunDirection;
	static ShadowCamera m_SunShadow;
	static ExpVar m_AmbientIntensity;
	static ExpVar m_SunLightIntensity;
#pragma endregion

	
	 static Math::Camera lastUsedCamera;

	//--- INITTIALIZATOIN ---
	static void Startup(Math::Camera& camera,std::vector<IParameterBlock*>& parameters, HWND hwnd);
	static  void SetupScene();

	//--- CLEANUP ---
	static void Cleanup(void);

	//--- RENDERING ---
	static void RenderScene(RENDER_SCENE_DEFAULT_PARAMS);

	//-- UI SPECIFIC --
	static void RenderImGuiUI(GraphicsContext& gfx);
protected:

	//--- INITTIALIZATOIN ---
	static void InitTriangleModel();
	static void InitQuadModel();
	static void InitSphereModel();

	static UINT FindClosesSurfelToPosition(Math::Vector3 position);

	//Model Rendering VS Constants
	struct VSConstants
	{
		Matrix4 modelToWorld;
		Matrix4 modelToProjection;
		Matrix4 modelToShadow;
		XMFLOAT3 viewerPos;
	};
	__declspec(align(16)) struct PSConstants
	{
		Vector3 sunDirection;
		Vector3 sunLight;
		Vector3 ambientLight;
		float ShadowTexelSize[4];

		float InvTileDim[4];
		uint32_t TileCount[4];
		uint32_t FirstLightIndex[4];

		uint32_t FrameIndexMod2;
	};
	static PSConstants psConstants;

	//--- RENDERING PASSES ---

	static void RenderLightShadows(GraphicsContext& gfxContext, const Camera& camera);
	static void RenderScreenSpaceTriangle(GraphicsContext& Context);
	static void RenderSphereObject(RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderSpheresAlongRay(Vector4 color,Vector3 rayOrigin, Vector3 rayDirection, int samples ,float offset,RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderSphereAt(Vector4 color,float scale, Vector4 position,RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderSurfelAt(Vector4 color, Vector4 normal, float scale, Vector4 position, RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderOBJObject(RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderOBJObjectCorrectPipeline(RENDER_OBJECT_INSTANCE_PARAMS);

	static void RenderObjects(RENDER_OBJECT_INSTANCE_PARAMS);

	static void RenderSSR(GraphicsContext& gfxContext, const Camera& camera,UINT objectIndex);

	static DescriptorHeap SSRHeap;
	static ColorBuffer colorCopyBuffer;
	static DepthBuffer depthCopyBuffer;

	static void CopyColorAndDepthBuffers(GraphicsContext& gfxContext);

	static void RenderRelevantSurfels(RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderSurfels(RENDER_OBJECT_INSTANCE_PARAMS);

	static void RenderColor(RENDER_SCENE_PARAMS);
	static void RenderDebugOverlay(RENDER_SCENE_PARAMS);


	static VSConstants SetupObjectVSConstants(RENDER_OBJECT_INSTANCE_PARAMS);
	


	static void RenderFullScreenQuad(GraphicsContext& gfxContext);

	//--- UTILITY ---
	static const ModelH3D& GetModel();

	static XMVECTOR GetRotationQuaternionFromUpToDirection(FXMVECTOR targetDirection);

	static void GetRelevantSurfels(UINT& from, UINT& to);

	static ModelH3D* m_Model;
	static ModelH3D* m_ModelExtra;
};
