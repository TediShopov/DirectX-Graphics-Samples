#pragma once

#include "RootSignature.h"
#include <d3d12.h>
#include "BufferManager.h"

#include <initializer_list>

class ExtendedUtility
{
public:

	/// <summary>
	/// Utility mehtod that copies initializer list of source descriptor to a given heap. Must specify the heap type as well.
	/// </summary>
	static void CopyDescriptorsToHeap(
		DescriptorHeap& targetHeap,
		std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
		D3D12_DESCRIPTOR_HEAP_TYPE heapType = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
};

