#pragma once
#include "GpuBuffer.h"
#include <DirectXMath.h>
#include <vector>
#include "BaseMesh.h"


class DiscMesh 
{
public:
	DiscMesh(int segments = 32);
	~DiscMesh();

	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

	std::vector<VertexType> m_Vertices;
	std::vector<uint32_t> m_Indices;
protected:

	void initBuffers();

	void FillVertexIndexCollections();

	void CreateBufferViews();


	int m_Segments;
	ByteAddressBuffer m_GeometryBuffer;
};
