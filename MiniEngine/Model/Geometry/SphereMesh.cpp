// Sphere Mesh
// Generates a cube sphere.
#include "SphereMesh.h"

// Store shape resolution (default is 20), initialise buffers and load texture.
SphereMesh::SphereMesh(int lresolution)
	//:BaseMesh(device,deviceContext)
{
	resolution = lresolution;
	initBuffers();
}

// Release resources.
SphereMesh::~SphereMesh()
{
	// Run parent deconstructor
	//BaseMesh::~BaseMesh();
}

// Generate sphere. Generates a cube based on resolution provided. Then normalises vertex positions to create sphere.
// Shape has texture coordinates and normals.
void SphereMesh::initBuffers()
{
	// 6 m_Vertices per quad, res*res is face, times 6 for each face
	int vertexCount = ((6 * resolution)*resolution) * 6;
	int indexCount = vertexCount;

	m_Vertices.resize(vertexCount);
	m_Indices.resize(indexCount);

	initGeometryData(resolution);


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


//	std::vector<VertexType> testVertices;
//	testVertices.assign(m_Vertices.size(),VertexType());
//	memcpy(&testVertices, vertexData, vertexDataSize);

	//--- Upload buffer to GPU
	m_GeometryBuffer.Create(L"Sphere", totalSize, 1, uploadMem);
	//--- Create buffer views
	m_VertexBufferView
		= m_GeometryBuffer.VertexBufferView(0, vertexDataSize, vertexStride);
	m_IndexBufferView
		= m_GeometryBuffer.IndexBufferView(vertexDataSize, indexDataSize, true);

	// Optional: store CPU copy (not required if you don't need access)
//    m_pVertexData = reinterpret_cast<uint8_t*>(_aligned_malloc(vertexDataSize, 16));
//    m_pIndexData  = reinterpret_cast<uint8_t*>(_aligned_malloc(indexDataSize, 16));
//    memcpy(m_pVertexData, triangleVerts, vertexDataSize);
//    memcpy(m_pIndexData, m_Indices, indexDataSize);
}

void SphereMesh::initGeometryData(float resolutions)
{
	// Vertex variables
	float yincrement = 2.0f / resolution;
	float xincrement = 2.0f / resolution;
	float ystart = 1.0f;
	float xstart = -1.0f;
	//UV variables
	float txu = 0.0f;
	float txv = 0.0f;
	float txuinc = 1.0f / resolution;	// UV increment
	float txvinc = 1.0f / resolution;
	//Counters
	int v = 0;	// vertex counter
	int i = 0;	// index counter

	//front face

	for (int y = 0; y<resolution; y++)	// for each quad in the y direction
	{
		for (int x = 0; x < resolution; x++)	// for each quad in the x direction
		{
			// Load the vertex array with data.
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, ystart - yincrement, -1.0f);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv + txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, ystart, -1.0f);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart, ystart, -1.0f);  // Top left.	-1.0, 1.0
			m_Vertices[v].texture = XMFLOAT2(txu, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, ystart - yincrement, -1.0f);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//3
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, ystart - yincrement, -1.0f);  // Bottom right.	1.0, -1.0, 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, ystart, -1.0f);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, -1.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			// increment
			xstart += xincrement;
			txu += txuinc;
			//ystart -= yincrement;

		}

		ystart -= yincrement;
		xstart = -1;

		txu = 0;
		txv += txvinc;

	}

	txv = 0;

	//back face
	ystart = 1;
	xstart = 1;
	for (int y = 0; y<resolution; y++)	// for each quad in the y direction
	{
		for (int x = 0; x < resolution; x++)	// for each quad in the x direction
		{
			// Load the vertex array with data.
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, ystart - yincrement, 1.0f);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, 1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart - xincrement, ystart, 1.0f);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, 1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(xstart, ystart, 1.0f);  // Top left.	-1.0, 1.0
			m_Vertices[v].texture = XMFLOAT2(txu, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, 1.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, ystart - yincrement, 1.0f);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, 1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//3
			m_Vertices[v].position = XMFLOAT3(xstart - xincrement, ystart - yincrement, 1.0f);  // Bottom right.	1.0, -1.0, 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, 1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart - xincrement, ystart, 1.0f);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 0.0f, 1.0f);

			m_Indices[i] = i;
			v++;
			i++;

			// increment
			xstart -= xincrement;
			//ystart -= yincrement;
			txu += txuinc;

		}

		ystart -= yincrement;
		xstart = 1;

		txu = 0;
		txv += txvinc;

	}

	txv = 0;

	//right face
	ystart = 1;
	xstart = -1;
	for (int y = 0; y<resolution; y++)	// for each quad in the y direction
	{
		for (int x = 0; x < resolution; x++)	// for each quad in the x direction
		{
			// Load the vertex array with data.
			//0
			m_Vertices[v].position = XMFLOAT3(1.0f, ystart - yincrement, xstart);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(1.0f, ystart, xstart + xincrement);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(1.0f, ystart, xstart);  // Top left.	-1.0, 1.0
			m_Vertices[v].texture = XMFLOAT2(txu, txv);
			m_Vertices[v].normal = XMFLOAT3(1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//0
			m_Vertices[v].position = XMFLOAT3(1.0f, ystart - yincrement, xstart);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//3
			m_Vertices[v].position = XMFLOAT3(1.0f, ystart - yincrement, xstart + xincrement);  // Bottom right.	1.0, -1.0, 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(1.0f, ystart, xstart + xincrement);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			// increment
			xstart += xincrement;
			//ystart -= yincrement;
			txu += txuinc;

		}

		ystart -= yincrement;
		xstart = -1;
		txu = 0;
		txv += txvinc;
	}

	txv = 0;

	//left face
	ystart = 1;
	xstart = 1;
	for (int y = 0; y < resolution; y++)	// for each quad in the y direction
	{
		for (int x = 0; x < resolution; x++)	// for each quad in the x direction
		{
			// Load the vertex array with data.
			//0
			m_Vertices[v].position = XMFLOAT3(-1.0f, ystart - yincrement, xstart);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(-1.0f, ystart, xstart - xincrement);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(-1.0f, ystart, xstart);  // Top left.	-1.0, 1.0
			m_Vertices[v].texture = XMFLOAT2(txu, txv);
			m_Vertices[v].normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//0
			m_Vertices[v].position = XMFLOAT3(-1.0f, ystart - yincrement, xstart);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//3
			m_Vertices[v].position = XMFLOAT3(-1.0f, ystart - yincrement, xstart - xincrement);  // Bottom right.	1.0, -1.0, 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(-1.0f, ystart, xstart - xincrement);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(-1.0f, 0.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			// increment
			xstart -= xincrement;
			//ystart -= yincrement;
			txu += txuinc;
		}

		ystart -= yincrement;
		xstart = 1;
		txu = 0;
		txv += txvinc;
	}
	
	txv = 0;

	//top face
	ystart = 1;
	xstart = -1;

	for (int y = 0; y<resolution; y++)	// for each quad in the y direction
	{
		for (int x = 0; x < resolution; x++)	// for each quad in the x direction
		{
			// Load the vertex array with data.
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, 1.0f, ystart - yincrement);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, 1.0f, ystart);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(xstart, 1.0f, ystart);  // Top left.	-1.0, 1.0
			m_Vertices[v].texture = XMFLOAT2(txu, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, 1.0f, ystart - yincrement);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//3
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, 1.0f, ystart - yincrement);  // Bottom right.	1.0, -1.0, 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, 1.0f, ystart);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, 1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			// increment
			xstart += xincrement;
			//ystart -= yincrement;
			txu += txuinc;
		}

		ystart -= yincrement;
		xstart = -1;
		txu = 0;
		txv += txvinc;
	}

	txv = 0;
	
	//bottom face
	ystart = -1;
	xstart = -1;

	for (int y = 0; y<resolution; y++)	// for each quad in the y direction
	{
		for (int x = 0; x < resolution; x++)	// for each quad in the x direction
		{
			// Load the vertex array with data.
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, -1.0f, ystart + yincrement);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, -1.0f, ystart);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//1
			m_Vertices[v].position = XMFLOAT3(xstart, -1.0f, ystart);  // Top left.	-1.0, 1.0
			m_Vertices[v].texture = XMFLOAT2(txu, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;
			
			//0
			m_Vertices[v].position = XMFLOAT3(xstart, -1.0f, ystart + yincrement);  // Bottom left. -1. -1. 0
			m_Vertices[v].texture = XMFLOAT2(txu, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//3
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, -1.0f, ystart + yincrement);  // Bottom right.	1.0, -1.0, 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv+txvinc);
			m_Vertices[v].normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			//2
			m_Vertices[v].position = XMFLOAT3(xstart + xincrement, -1.0f, ystart);  // Top right.	1.0, 1.0 0.0
			m_Vertices[v].texture = XMFLOAT2(txu+txuinc, txv);
			m_Vertices[v].normal = XMFLOAT3(0.0f, -1.0f, 0.0f);

			m_Indices[i] = i;
			v++;
			i++;

			// increment
			xstart += xincrement;
			//ystart -= yincrement;
			txu += txuinc;
		}

		ystart += yincrement;
		xstart = -1;
		txu = 0;
		txv += txvinc;
	}

	// now loop over every vertex and bend into a sphere (normalise the m_Vertices)
	float x = 0;
	float y = 0;
	float z = 0;
	float dx = 0;
	float dy = 0;
	float dz = 0;

	for (int counter = 0; counter < v; counter++)
	{
		x = m_Vertices[counter].position.x;
		y = m_Vertices[counter].position.y;
		z = m_Vertices[counter].position.z;

		dx = x * sqrtf(1.0f - (y*y / 2.0f) - (z*z / 2.0f) + (y*y*z*z / 3.0f));
		dy = y * sqrtf(1.0f - (z*z / 2.0f) - (x*x / 2.0f) + (z*z*x*x / 3.0f));
		dz = z * sqrtf(1.0f - (x*x / 2.0f) - (y*y / 2.0f) + (x*x*y*y / 3.0f));

		m_Vertices[counter].position.x = dx;
		m_Vertices[counter].position.y = dy;
		m_Vertices[counter].position.z = dz;

		m_Vertices[counter].normal.x = dx;
		m_Vertices[counter].normal.y = dy;
		m_Vertices[counter].normal.z = dz;
	}
}

