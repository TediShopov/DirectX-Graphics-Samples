#include "ExtendedUtility.h"

void ExtendedUtility::CopyDescriptorsToHeap(
    ID3D12DescriptorHeap* targetHeap,
    std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors,
    D3D12_DESCRIPTOR_HEAP_TYPE heapType, UINT& currentOffset,
    UINT descriptorSize)
{
    // Prepare descriptor count
    UINT count = static_cast<UINT>(srcDescriptors.size());

    // Get the destination handle by computing offset from start of heap
    D3D12_CPU_DESCRIPTOR_HANDLE destHandle = targetHeap->GetCPUDescriptorHandleForHeapStart();
    destHandle.ptr += currentOffset * descriptorSize;

    // Track source handles and counts
    std::vector<UINT> srcCounts(count, 1);
    std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles(srcDescriptors);

    // Perform descriptor copy
    Graphics::g_Device->CopyDescriptors(
        1, &destHandle, &count,
        count, srcHandles.data(), srcCounts.data(),
        heapType
    );

    // Advance current offset for next allocation
    currentOffset += count;
}

/// <summary>
/// Utility mehtod that copies initializer list of source descriptor to a given heap. Must specify the heap type as well.
/// </summary>

 void ExtendedUtility::CopyDescriptorsToHeap(DescriptorHeap& targetHeap, std::initializer_list<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{

	// Allocate space in the heap
	DescriptorHandle destHandle = targetHeap.Alloc((uint32_t)srcDescriptors.size());

	// Prepare descriptor arrays
	uint32_t count = (uint32_t)srcDescriptors.size();
	std::vector<UINT> srcCounts(count, 1);
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles(srcDescriptors);

	Graphics::g_Device->CopyDescriptors(
		1, &destHandle, &count,
		count, srcHandles.data(), srcCounts.data(),
		heapType
	);
}
 void ExtendedUtility::CopyDescriptorsToHeap(DescriptorHeap& targetHeap, std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcDescriptors, D3D12_DESCRIPTOR_HEAP_TYPE heapType)
{
	// Allocate space in the heap
	DescriptorHandle destHandle = targetHeap.Alloc((uint32_t)srcDescriptors.size());

	// Prepare descriptor arrays
	uint32_t count = (uint32_t)srcDescriptors.size();
	std::vector<UINT> srcCounts(count, 1);
	std::vector<D3D12_CPU_DESCRIPTOR_HANDLE> srcHandles(srcDescriptors);

	Graphics::g_Device->CopyDescriptors(
		1, &destHandle, &count,
		count, srcHandles.data(), srcCounts.data(),
		heapType
	);
}
