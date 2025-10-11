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

// TestRendererContext.h
struct SurfelGIRendererContext {
    // device-independent toggles / state
    bool useInformedSBGI = true;
    bool drawSSR = false;
    bool drawSurfels = false;
    bool enableDebugOverlay = true;
    int  debugOverlayMode = 0;
    UINT frameIndex = 0;

    // resources, PSOs, heaps, buffers…
	std::unique_ptr<SurfelGI> SurfelIllumination;
	std::unique_ptr<HashGridVisualization> GridVisualization;
	std::unique_ptr<MSMEVisualization> GridMSMEVisualization;
	std::unique_ptr<SurfelSpawnChanceVisualization> SurfelSpawnVisualization;
	std::unique_ptr<SurfelGIOnlyVisualization> SurfelGIVisualization;
	std::unique_ptr<SurfelGIOnlyVisualization> MaterialBindingDebug;
	std::unique_ptr<HBIL> m_HBIL;
	std::unique_ptr<HBILInterleaved> m_HBILInterleaved;
	std::unique_ptr<GBufferDownsample> m_GBufferDownsample;
	std::unique_ptr<GBufferSlice> m_GBufferSlice;
    // … the rest of pointers you currently keep as statics …

    // scene assets
    std::unique_ptr<ModelH3D> model;
    std::unique_ptr<SphereMesh> sphere;
    std::unique_ptr<DiscMesh> disc;
    std::unique_ptr<Transform> transform;

    // cached sun/shadow and constants
    ShadowCamera sunShadow;
    ExpVar ambientIntensity  = ExpVar("Ambient Light Intensity", 0.1f);
    ExpVar sunLightIntensity = ExpVar("Sun Light Intensity", 1.0f);
    Vector3 sunDirection = Vector3(0,1,0);

    // PSOs
    GraphicsPSO psoDepth, psoCutoutDepth, psoShadow, psoCutoutShadow;
    GraphicsPSO psoColor, psoColorCutout, psoSphere, psoSSR;

    // convenience
    Matrix4 lastViewProj;
    Camera  lastCamera;
};

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
		uint32_t UseConfidenceWeight;
		float	AOThreshold;
		uint32_t pad;

	};

struct RenderArgs {
    GraphicsContext& gfx;
    const Camera& camera;
    const D3D12_VIEWPORT& viewport;
    const D3D12_RECT& scissor;
    bool skipDiffuse = false;
    bool skipShadow  = false;
};

enum  eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };

struct RenderObjectInstancePassArgs {
    GraphicsContext& gfx;
	const Matrix4& viewProjMat;
	const Vector3& viewerPos;
	eObjectFilter filter;
};


//namespace TestRenderer
class SurfelGIRenderer 
{
public:
	//--- DEFINES ---
//#define RenderObjectInstancePassArgs GraphicsContext& gfxContext, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter

	//--- INITTIALIZATOIN ---
	static void Startup(Math::Camera& camera,std::vector<IParameterBlock*>& parameters, HWND hwnd);
	static void SetSurfelIlluminationAlgorithm(bool isHbilInformedSBGI);

	static  void SetupScene();

	//--- CLEANUP ---
	static void Cleanup(void);

	//--- RENDERING ---
	static void RenderScene(RenderArgs& renderArgs);

	//-- UI SPECIFIC --
	static void RenderImGuiUI(GraphicsContext& gfx);
protected:

	//--- INITTIALIZATOIN ---
	static void InitTriangleModel();
	static void InitQuadModel();
	static void InitSphereModel();

	static UINT FindClosesSurfelToPosition(Math::Vector3 position);

	//Model Rendering VS Constants
	static PSConstants psConstants;

	//--- RENDERING PASSES ---

	static void RenderLightShadows(GraphicsContext& gfxContext, const Camera& camera);
	static void RenderScreenSpaceTriangle(GraphicsContext& Context);
	static void RenderSpheresAlongRay(Vector4 color,Vector3 rayOrigin, Vector3 rayDirection, int samples ,float offset,RenderObjectInstancePassArgs);
	static void RenderSphereAt(Vector4 color,float scale, Vector4 position,RenderObjectInstancePassArgs);
	static void RenderSurfelAt(Vector4 color, Vector4 normal, float scale, Vector4 position, RenderObjectInstancePassArgs);
	static void RenderSphereObject(RenderObjectInstancePassArgs);
	static void RenderOBJObject(RenderObjectInstancePassArgs);
	static void RenderOBJObjectCorrectPipeline(RenderObjectInstancePassArgs);

	static void RenderObjects(RenderObjectInstancePassArgs);

	static void RenderSSR(GraphicsContext& gfxContext, const Camera& camera,UINT objectIndex);

	static DescriptorHeap SSRHeap;
	static ColorBuffer colorCopyBuffer;
	static DepthBuffer depthCopyBuffer;

	static void CopyColorAndDepthBuffers(GraphicsContext& gfxContext);

	static void RenderRelevantSurfels(RenderObjectInstancePassArgs);
	static void RenderSurfels(RenderObjectInstancePassArgs);

	static void RenderColor(RenderArgs& renderArgs);
	static void RenderDebugOverlay(RenderArgs& renderArgs);


	static VSConstants SetupObjectVSConstants(RenderObjectInstancePassArgs);
	


	static void RenderFullScreenQuad(GraphicsContext& gfxContext);

	//--- UTILITY ---
	static const ModelH3D& GetModel();

	static XMVECTOR GetRotationQuaternionFromUpToDirection(FXMVECTOR targetDirection);

	static void GetRelevantSurfels(UINT& from, UINT& to);

	static ModelH3D* m_Model;
	static ModelH3D* m_ModelExtra;
};
