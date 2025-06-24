#pragma once

#include "GraphicsCore.h"
#include "BufferManager.h"
//#include "Camera.h"
#include "CommandContext.h"
#include "Geometry/Transform.h"
#include "CompiledShaders/Raytracing.h"


#include <dxcapi.h>
#include <vector>
#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"
#include "DirectXRaytracingHelper.h"
#include "UniformGrid.h"

namespace Math
{
    class Camera;
    class Vector3;
}

class ModelH3D;
class DescriptorHeapStack;

namespace TestRaytracing
{
	//Structs of Constant Buffer Data
	__declspec(align(16))	struct		SunDirectionalLight
	{
		Vector4 sunDirection;
		Vector4 sunColor;
		Vector4 ambientColor;
	};
	__declspec(align(16))	struct		PerInstanceCB {

		UINT materialIndex;
		UINT b;
		UINT c;
		UINT d;

		Color diffuse;
		Color specular;
		Color ambient;
		Color emissive;
		Color transparent; // light passing through a transparent surface is multiplied by this filter color
		float opacity;
		float shininess; // specular exponent
		float specularStrength; // multiplier on top of specular color
		float padding;
	};
	__declspec(align(16))	struct		RayGet3DBuffer {

		XMMATRIX invViewProject;
		XMMATRIX viewToWorld;
		Viewport viewport;
		Viewport stencil;
	};
	__declspec(align(16)) struct AdditionalVertexData
	{
		XMFLOAT4 uv;
		XMFLOAT4 normal;
		XMFLOAT4 tangent;
		XMFLOAT4 bitanget;
	};

	extern std::unique_ptr<DescriptorHeapStack> g_pRaytracingDescriptorHeap;
	extern SunDirectionalLight directionalLightData;

	//--- RENDERING PASS	---
	void DoRaytracing(const Math::Camera& camera, DescriptorHeap surfelUAVHeap, UniformGrid grid);

	//---	INITIALIZATION FUNCITONS ---
	void CreateDeviceDependentResources(
		Transform transform,
		ModelH3D& model,
		ColorBuffer* outputBuffer,
		DescriptorHeap surfelUAVHeap
	);

	void CreateOutputAndPerInstanceTexturesHeap(ColorBuffer* outputBuffer, DescriptorHeap surfelUAVHeap, ModelH3D& model);
	void CreatePerInstanceCBs(ModelH3D& model, UINT numMeshes);

	void CreateRaytracingInterfaces();

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
	void CreateRootSignatures();

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);

	void CreateRaytracingPipelineStateObject();

	void BuildAccelerationStructures(Transform transform, ModelH3D& model, UINT numMeshes);

	void BuildShaderTables();

	//---	CREATE RAYTRACING OUTPUT UAV ---

	void CreateOutputTextureUAV(ColorBuffer* color);
	ColorBuffer GetOutputBuffer();


	//--- UTILITIES AND EVENTS ---

	// Update the application state with the new resolution.
	void UpdateCBForSizeChange(UINT width, UINT height);
	inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg);
};

