#pragma once

#include "GraphicsCore.h"
#include "BufferManager.h"
//#include "Camera.h"
#include "CommandContext.h"
#include "Geometry/Transform.h"
//#include "CompiledShaders/Raytracing.h"
#include "CompiledShaders/SurfelIrradianceRayTracing.h"


#include <dxcapi.h>
#include <vector>
#include "DXSample.h"
#include "StepTimer.h"
#include "RaytracingHlslCompat.h"
#include "DirectXRaytracingHelper.h"
#include "UniformGrid.h"
#include "SurfelGI.h"


namespace Math
{
    class Camera;
    class Vector3;
}
class ModelH3D;

class DescriptorHeapStack;

namespace SurfelIrradianceAccumulation
{
	extern std::unique_ptr<DescriptorHeapStack> g_pRaytracingDescriptorHeap;
	
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
	//---   RAY-TRACING RELATED
	void CreateDeviceDependentResources(
		Transform transform,
		ModelH3D& model,
		ColorBuffer* outputBuffer,
		DescriptorHeap surfelUAVHeap
	);

	extern SunDirectionalLight directionalLightData;

	void CreateOutputTextureUAV(ColorBuffer* color);
	void DoRaytracing(const Math::Camera& camera,DescriptorHeap surfelUAVHeap, UniformGrid grid,std::vector<SurfelData>& surfels);

// Update the application state with the new resolution.
	void UpdateCBForSizeChange(UINT width, UINT height);




	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle();
	ColorBuffer GetOutputBuffer();


	void CreateRaytracingInterfaces();
	inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg);

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
	void CreateRootSignatures();

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);

	void CreateRaytracingPipelineStateObject();


	void BuildAccelerationStructures(Transform transform,ModelH3D& model, UINT numMeshes);

	void BuildShaderTables();

	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse);




};


