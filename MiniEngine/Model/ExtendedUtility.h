#pragma once

#include "RootSignature.h"
#include <d3d12.h>
#include "BufferManager.h"
#include "Math/Vector.h"
#include "Math/Matrix4.h"

#include "Camera.h"

#include <initializer_list>
struct GBufferPtrs
{
	ColorBuffer* g_Color;
	ColorBuffer* g_Diffuse;
	ColorBuffer* g_Normal;
	DepthBuffer* g_Depth;

};

using namespace Math;
class ExtendedUtility
{
public:
	
	static Matrix4 CreatePerspectiveFovLH(float fovY, float aspect, float nearZ, float farZ) 
	{

		float yScale = 1.0f / tanf(fovY * 0.5f);
		float xScale = yScale / aspect;
		float zRange = farZ - nearZ;
		const float lhPerspectiveData[16] =
		{

			xScale, 0.0f,    0.0f,                     0.0f,
			0.0f,   yScale,  0.0f,                     0.0f,
			0.0f,   0.0f,    farZ / zRange,            1.0f,
			0.0f,   0.0f,    -nearZ * farZ / zRange,   0.0f
		};

		//return Matrix4(lhPerspectiveData);
		return Matrix4();
	}
	static Matrix4 ExtendedUtility::GetLHViewMatrix(const Camera& camera)
	{
		//Get the camera position up, right and forward vectors
		Vector3 position = camera.GetPosition();
		Vector3 forward = camera.GetForwardVec();
		Vector3 right = camera.GetRightVec();
		Vector3 up = camera.GetUpVec();

		const float lhViewMatrixData[16] = {
			(float)right.GetX(), (float)up.GetX(), (float)forward.GetX(), 0.0f,
			(float)right.GetY(), (float)up.GetY(), (float)forward.GetY(), 0.0f,
			(float)right.GetZ(), (float)up.GetZ(), (float)forward.GetZ(), 0.0f,
			-(float)Dot(right, position), -(float)Dot(up, position), -(float)Dot(forward, position), 1.0f
		};

		// Construct left-handed view matrix
		Matrix4 viewMatrix = Matrix4(lhViewMatrixData);
		return viewMatrix;

	}



	static void ExtendedUtility::CopyDescriptorsToHeap(
		ID3D12DescriptorHeap* targetHeap,
		std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType,
		UINT& currentOffset,
		UINT descriptorSize);

	/// <summary>
	/// Utility mehtod that copies initializer list of source descriptor to a given heap. Must specify the heap type as well.
	/// </summary>
	static void CopyDescriptorsToHeap(
		DescriptorHeap& targetHeap,
		std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	static void CopyDescriptorsToHeap(DescriptorHeap& targetHeap,
		std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);


	// Read one vertex's attribute of type T
	 template<typename T>
	static T ReadAttribute(
		uint8_t* base,
		size_t stride,
		size_t index,
		size_t offset
	)
	{
		return *reinterpret_cast<T*>(base + index * stride + offset);
	}
	template<typename T>
	static T ReadAttribute(
		uint8_t* base,
		size_t index,
		size_t offset
	)
	{
		return *reinterpret_cast<T*>(base + index * sizeof(T) + offset);
	}

	static XMFLOAT4 PadToXMFLOAT4(XMFLOAT3 a, float w = 1.0f) {
		return XMFLOAT4(a.x, a.y, a.z, w);

	}


};

