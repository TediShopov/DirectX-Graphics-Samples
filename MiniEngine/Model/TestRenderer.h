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

namespace TestRenderer
{
	//--- DEFINES ---

	enum eObjectFilter { kOpaque = 0x1, kCutout = 0x2, kTransparent = 0x4, kAll = 0xF, kNone = 0x0 };

	#define RENDER_OBJECT_INSTANCE_PARAMS GraphicsContext& gfxContext, const Matrix4& ViewProjMat, const Vector3& viewerPos, eObjectFilter Filter

	
	//--- DATA ---
#pragma region ExposedProperties
	extern UINT frameIndex;

	extern SurfelGI* SurfelIllumination;
	extern HashGridVisualization* GridVisualization;
	extern MSMEVisualization* GridMSMEVisualization;
	extern SurfelGIOnlyVisualization* SurfelGIVisualization;

	extern SphereMesh* m_Sphere;
	extern DiscMesh* m_Disc;
	extern Transform m_Transform;

	//-- DIRECTIONAL LIGHT PROPERTIES
    extern Math::Vector3 m_SunDirection;
    extern ShadowCamera m_SunShadow;
    extern ExpVar m_AmbientIntensity;
    extern ExpVar m_SunLightIntensity;
#pragma endregion


	//--- INITTIALIZATOIN ---
	void Startup( Math::Camera& camera , HWND hwnd);
	void InitTriangleModel();
	void InitQuadModel();
	void InitSphereModel();

	//--- CLEANUP ---
	void Cleanup( void );

	//--- RENDERING ---
	void RenderScene(
		GraphicsContext& gfxContext,
		const Math::Camera& camera,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		bool skipDiffusePass = false,
		bool skipShadowMap = false );

	void RenderLightShadows(GraphicsContext& gfxContext, const Camera& camera);
	void RenderScreenSpaceTriangle(GraphicsContext& Context);
	void RenderSphereObject(RENDER_OBJECT_INSTANCE_PARAMS);
	void RenderObjects(RENDER_OBJECT_INSTANCE_PARAMS);
	void RenderSurfels(RENDER_OBJECT_INSTANCE_PARAMS);


	//-- UI SPECIFIC --
	void RenderImGuiUI(GraphicsContext& gfx);


	//--- UTILITY ---
	const ModelH3D& GetModel();


}
