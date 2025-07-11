#pragma once
#include <d3d12.h>
//#include "SerializableMesh.h"
//#include "MeshInstance.h"
#include "Geometry/SphereMesh.h"
#include "Geometry/DiscMesh.h"
#include "Geometry/Transform.h"


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
using namespace Math;

//namespace TestRenderer
class TestRenderer
{
public:
	//--- DEFINES ---

	enum eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };

#define RENDER_OBJECT_INSTANCE_PARAMS GraphicsContext& gfxContext, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter


	//--- DATA ---
#pragma region ExposedProperties
	static UINT frameIndex;

	static SurfelGI* SurfelIllumination;
	static HashGridVisualization* GridVisualization;
	static MSMEVisualization* GridMSMEVisualization;
	static SurfelGIOnlyVisualization* SurfelGIVisualization;

	static SphereMesh* m_Sphere;
	static DiscMesh* m_Disc;
	static Transform m_Transform;

	//-- DIRECTIONAL LIGHT PROPERTIES
	static Math::Vector3 m_SunDirection;
	static ShadowCamera m_SunShadow;
	static ExpVar m_AmbientIntensity;
	static ExpVar m_SunLightIntensity;
#pragma endregion


	//--- INITTIALIZATOIN ---
	static void Startup(Math::Camera& camera, HWND hwnd);

	//--- CLEANUP ---
	static void Cleanup(void);

	//--- RENDERING ---
	static void RenderScene(
		GraphicsContext& gfxContext,
		const Math::Camera& camera,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		bool skipDiffusePass = false,
		bool skipShadowMap = false);


	//-- UI SPECIFIC --
	static void RenderImGuiUI(GraphicsContext& gfx);
protected:

	//--- INITTIALIZATOIN ---
	static void InitTriangleModel();
	static void InitQuadModel();
	static void InitSphereModel();



	//--- RENDERING PASSES ---
	static void RenderLightShadows(GraphicsContext& gfxContext, const Camera& camera);
	static void RenderScreenSpaceTriangle(GraphicsContext& Context);
	static void RenderSphereObject(RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderObjects(RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderSurfels(RENDER_OBJECT_INSTANCE_PARAMS);
	static void RenderRelevantSurfels(RENDER_OBJECT_INSTANCE_PARAMS);


	static void RenderFullScreenQuad(GraphicsContext& gfxContext);

	//--- UTILITY ---
	static const ModelH3D& GetModel();

	static XMVECTOR GetRotationQuaternionFromUpToDirection(FXMVECTOR targetDirection);

	static void GetRelevantSurfels(UINT& from, UINT& to);

	static ModelH3D* m_Model;
};
