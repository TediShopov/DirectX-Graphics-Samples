#pragma once
#include <d3d12.h>
//#include "SerializableMesh.h"
//#include "MeshInstance.h"
#include "Geometry/SphereMesh.h"
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

namespace TestRenderer
{
	void Startup( Math::Camera& camera );
	void Cleanup( void );

	void RenderScene(
		GraphicsContext& gfxContext,
		const Math::Camera& camera,
		const D3D12_VIEWPORT& viewport,
		const D3D12_RECT& scissor,
		bool skipDiffusePass = false,
		bool skipShadowMap = false );

	const ModelH3D& GetModel();
	void InitTriangleModel();
	void InitSphereModel();
	void InitTestRootSignature();

	//std::unordered_map<std::wstring, SphereMesh> m_SphereMeshes;
	//std::vector<SphereMesh*> m_SphereMeshes;
	extern SphereMesh* m_Sphere;
	extern Transform m_Transform;
	//std::vector<MeshInstance*> m_meshes;


    extern Math::Vector3 m_SunDirection;
    extern ShadowCamera m_SunShadow;
    extern ExpVar m_AmbientIntensity;
    extern ExpVar m_SunLightIntensity;

}
