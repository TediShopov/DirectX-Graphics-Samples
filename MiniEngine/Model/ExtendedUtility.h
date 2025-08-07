#pragma once

#include "RootSignature.h"
#include <d3d12.h>
#include "BufferManager.h"

#include <initializer_list>
struct GBufferPtrs
{
	ColorBuffer* g_Color;
	ColorBuffer* g_Diffuse;
	ColorBuffer* g_Normal;
	DepthBuffer* g_Depth;

};

class ExtendedUtility
{
public:

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

