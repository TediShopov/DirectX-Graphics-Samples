// Sphere Mesh
// Generates a cube sphere.
#include "DiscMesh.h"


DiscMesh::DiscMesh( int segments)
{
	m_Segments = segments;
	initBuffers();
}

DiscMesh::~DiscMesh()
{
}

void DiscMesh::initBuffers()
{
	FillVertexIndexCollections();

	CreateBufferViews();
}

void DiscMesh::FillVertexIndexCollections()
{
	const float radius = 0.5f;
	const XMFLOAT3 normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

	UINT vertexCount = m_Segments * 3; // Triangle list: 1 triangle per segment
	UINT indexCount = vertexCount;


	//Initialize default values for vectors
	for (size_t i = 0; i < vertexCount; i++)
	{
		VertexType defType;
		m_Vertices.push_back(defType);
		m_Indices.push_back(i);
	}


	const float angleStep = XM_2PI / m_Segments;

	for (int i = 0; i < m_Segments; ++i)
	{
		float angle0 = i * angleStep;
		float angle1 = ((i + 1) % m_Segments) * angleStep;

		XMFLOAT3 p0 = XMFLOAT3(0.0f, 0.0f, 0.0f); // center
		XMFLOAT3 p1 = XMFLOAT3(radius * cosf(angle0), 0.0f, radius * sinf(angle0));
		XMFLOAT3 p2 = XMFLOAT3(radius * cosf(angle1), 0.0f, radius * sinf(angle1));

		XMFLOAT2 uv0 = XMFLOAT2(0.5f, 0.5f);
		XMFLOAT2 uv1 = XMFLOAT2((cosf(angle0) * 0.5f + 0.5f), (sinf(angle0) * 0.5f + 0.5f));
		XMFLOAT2 uv2 = XMFLOAT2((cosf(angle1) * 0.5f + 0.5f), (sinf(angle1) * 0.5f + 0.5f));

		int baseIndex = i * 3;
		m_Vertices[baseIndex + 0] = { p0, uv0, normal };
		m_Vertices[baseIndex + 1] = { p1, uv1, normal };
		m_Vertices[baseIndex + 2] = { p2, uv2, normal };

		m_Indices[baseIndex + 0] = baseIndex + 0;
		m_Indices[baseIndex + 1] = baseIndex + 1;
		m_Indices[baseIndex + 2] = baseIndex + 2;
	}
}

void DiscMesh::CreateBufferViews()
{
	size_t vertexStride = sizeof(VertexType);
	size_t vertexDataSize = sizeof(VertexType) * m_Vertices.size();
	size_t indexDataSize = sizeof(UINT32) * m_Indices.size();

	// 2. Allocate upload buffer (vertex + index)
	size_t totalSize = vertexDataSize + indexDataSize;
	void* uploadMem = _aligned_malloc(totalSize, 16);
	assert(uploadMem);

	void* vertexData = uploadMem;
	void* indexData = static_cast<uint8_t*>(uploadMem) + vertexDataSize;

	memcpy(vertexData, m_Vertices.data(), vertexDataSize);
	memcpy(indexData, m_Indices.data(), indexDataSize);

	//--- Upload buffer to GPU
	m_GeometryBuffer.Create(L"Disc", totalSize, 1, uploadMem);
	//--- Create buffer views
	m_VertexBufferView
		= m_GeometryBuffer.VertexBufferView(0, vertexDataSize, vertexStride);
	m_IndexBufferView
		= m_GeometryBuffer.IndexBufferView(vertexDataSize, indexDataSize, true);
}
