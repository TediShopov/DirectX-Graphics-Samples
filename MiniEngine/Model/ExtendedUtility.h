#pragma once

#include "RootSignature.h"
#include <d3d12.h>
#include "BufferManager.h"

#include <initializer_list>
struct GBufferPtrs
{
	ColorBuffer* g_Color;
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
};

