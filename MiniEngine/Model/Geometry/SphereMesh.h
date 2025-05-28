// Sphere Mesh
// Generated sphere mesh with texture coordinates and normals.
// Uses the cube sphere normalisation method. First a cube is generated,
// then the vertices are normalised creating a sphere.
// Resolution specifies the number of segments in the sphere (top and bottom, matches equator).


#ifndef _SPHEREMESH_H_
#define _SPHEREMESH_H_

#include "GpuBuffer.h"
#include <DirectXMath.h>
#include <vector>

using namespace DirectX;

struct VertexType
{
	XMFLOAT3 position;
	XMFLOAT2 texture;
	XMFLOAT3 normal;
};

using namespace DirectX;

class SphereMesh 
//	: public BaseMesh
{

public:
	ByteAddressBuffer m_GeometryBuffer;
	D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
	D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;

	std::vector<VertexType> m_Vertices;
	std::vector<uint32_t> m_Indices;



	SphereMesh(int resolution = 20);
	~SphereMesh();

protected:
	void initBuffers();
	void initGeometryData(float res);
	int resolution;
};

#endif