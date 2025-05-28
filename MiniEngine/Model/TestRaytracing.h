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


namespace Math
{
    class Camera;
    class Vector3;
}

namespace TestRaytracing
{
	//ByteAddressBuffer m_TestCB;

	//---   RAY-TRACING RELATED
#pragma region RAY-TRACING DEMO


	// Create resources that depend on the device.
	void CreateDeviceDependentResources(
		Transform transform,
		D3D12_VERTEX_BUFFER_VIEW vertexBV,
		D3D12_INDEX_BUFFER_VIEW indexBV,
		ColorBuffer* outputBuffer
	);

// Update the application state with the new resolution.
	void UpdateCBForSizeChange(UINT width, UINT height);


	D3D12_GPU_DESCRIPTOR_HANDLE GetSrvGpuHandle();
	D3D12_CPU_DESCRIPTOR_HANDLE GetSrvCpuHandle();
	ColorBuffer GetOutputBuffer();

	void DoRaytracing(const Math::Camera& camera );

	void CreateRaytracingInterfaces();
	inline void ThrowIfFailed(HRESULT hr, const wchar_t* msg);

	void SerializeAndCreateRaytracingRootSignature(D3D12_ROOT_SIGNATURE_DESC& desc, ComPtr<ID3D12RootSignature>* rootSig);
	void CreateRootSignatures();

	// Local root signature and shader association
	// This is a root signature that enables a shader to have unique arguments that come from shader tables.
	void CreateLocalRootSignatureSubobjects(CD3DX12_STATE_OBJECT_DESC* raytracingPipeline);

	void CreateRaytracingPipelineStateObject();


	void BuildAccelerationStructures(Transform transform,D3D12_VERTEX_BUFFER_VIEW vertexBV, D3D12_INDEX_BUFFER_VIEW indexBV);

	void BuildShaderTables();

	UINT AllocateDescriptor(D3D12_CPU_DESCRIPTOR_HANDLE* cpuDescriptor, UINT descriptorIndexToUse);



#pragma endregion

};

