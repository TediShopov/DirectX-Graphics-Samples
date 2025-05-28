//// BaseMesh.cpp // Base mesh class, for inheriting base mesh functionality.
//
//#include <map>
//
//#include "basemesh.h"
//
//bool BaseMesh::rayTriangleIntersect(XMVECTOR orig, XMVECTOR dir, XMVECTOR v0, XMVECTOR v1, XMVECTOR v2, XMVECTOR& P,
//	float& t, float& u, float& v)
//{
//	// compute plane's normal
//	XMVECTOR v0v1 = v1 - v0;
//	XMVECTOR v0v2 = v2 - v0;
//	// no need to normalize
//	//XMVECTOR N = v0v1.crossProduct(v0v2); // N
//	XMVECTOR N = XMVector3Cross(v0v1, v0v2); // N
//	//
//
//
//
//	// Ray-plane intersection: solving t from the plane equation
//	//float NdotRayDirection = N.dotProduct(dir);
//	float NdotRayDirection = XMVector3Dot(N, dir).m128_f32[0];
//
//	if (fabs(NdotRayDirection) < 1e-8) return false; // Ray is parallel to the triangle
//
//	//float d = N.dotProduct(v0); // Plane constant
//	float d = XMVector3Dot(N, v0).m128_f32[0];// Plane constant
//	//t = (N.dotProduct(v0) - N.dotProduct(orig)) / NdotRayDirection;
//	t = (d - XMVector3Dot(N,orig).m128_f32[0]) / NdotRayDirection;
//
//	// If t is negative, the triangle is behind the ray
//	if (t < 0) return false; 
//
//	// Compute the intersection point P
//	//XMVECTOR P = orig + dir * t;
//	 P = orig + dir * t;
//
//	// Step 2: Calculate area of the triangle
//	//float area = N.length() / 2; // Area of the full triangle
//	float area = XMVector3Length(N).m128_f32[0] / 2; // Area of the full triangle
//
//	// Step 3: Inside-outside test using barycentric coordinates
//	XMVECTOR C;
//
//	// Calculate u (for triangle BCP)
//	XMVECTOR v1p = P - v1;
//	XMVECTOR v1v2 = v2 - v1;
//	//    C = v1v2.crossProduct(v1p);
//	C = XMVector3Cross(v1v2, v1p);
//
//	//u = (C.length() / 2) / area;
//	u = (XMVector3Length((C)).m128_f32[0]/ 2) / area;
//	//if (N.dotProduct(C) < 0) return false; // P is on the wrong side
//	if (XMVector3Dot(N,C).m128_f32[0] < 0) return false; // P is on the wrong side
//
//	// Calculate v (for triangle CAP)
//	XMVECTOR v2p = P - v2;
//	XMVECTOR v2v0 = v0 - v2;
//	//C = v2v0.crossProduct(v2p);
//	C = XMVector3Cross(v2v0,v2p);
//	v = (XMVector3Length((C)).m128_f32[0] / 2) / area;
//	//if (N.dotProduct(C) < 0) return false; // P is on the wrong side
//	if (XMVector3Dot(N,C).m128_f32[0] < 0) return false; // P is on the wrong side
//
//	// Third edge
//	XMVECTOR v0p = P - v0;
//	// XMVECTOR v0v1 = v1 - v0; -> already defined
//	//C = v0v1.crossProduct(v0p);
//	C = XMVector3Cross(v0v1,v0p);
//	if (XMVector3Dot(N,C).m128_f32[0] < 0) return false; // P is on the wrong side
//
//	// The point is inside the triangle
//	return true;
//}
//
//bool BaseMesh::allRayMeshIntersections(XMVECTOR orig, XMVECTOR dir, std::vector<XMVECTOR>* intersections)
//{
//	int intersectionCount = 0;
//	for (int i = 0; i < this->indices.size(); i+=3)
//	{
//		auto A = XMLoadFloat3(&this->vertices[i].position);
//		auto B = XMLoadFloat3(&this->vertices[i+1].position);
//		auto C = XMLoadFloat3(&this->vertices[i+2].position);
//
//		XMVECTOR P;
//		float t, u, v;
//
//
//		bool trueIntersection = rayTriangleIntersect(orig, dir, A, B, C, P, t, u, v);
//		if (trueIntersection)
//		{
//			if(intersections != nullptr)
//			{
//				intersections->push_back(P);
//			}
//			intersectionCount++;
//		}
//	}
//	return intersectionCount>0;
//}
//
//bool BaseMesh::rayMeshIntersect(XMVECTOR orig, XMVECTOR dir, XMVECTOR* closestIntersection)
//{
//	std::vector<XMVECTOR> tempIntersections;
//	bool b = allRayMeshIntersections(orig, dir, &tempIntersections);
//	float minDistance = 999999;
//	for (XMVECTOR i : tempIntersections)
//	{
//		float dist = XMVector3Length(XMVectorSubtract(i, orig)).m128_f32[0];
//		if (dist < minDistance)
//		{
//			minDistance = dist;
//			if(closestIntersection != nullptr)
//				(*closestIntersection) = i;
//		}
//
//	}
//	return b;
//}
//
//BaseMesh::BaseMesh(ID3D11Device* device,ID3D11DeviceContext* deviceContext)
//{
//	this->device = device;
//	this->deviceContext = deviceContext;
//	vertexBuffer = nullptr;
//	indexBuffer = nullptr;
//	vertexCount = 0; 
//	indexCount = 0;
//	vertexStagingBuffer = nullptr;
//	indexStagingBuffer = nullptr;
//
//}
//
//// Release base objects (index, vertex buffers and texture object.
//BaseMesh::~BaseMesh()
//{
//	if (indexBuffer)
//	{
//		indexBuffer->Release();
//		indexBuffer = 0;
//	}
//
//	if (vertexBuffer)
//	{
//		vertexBuffer->Release();
//		vertexBuffer = 0;
//	}
//}
//
//void BaseMesh::changeOrigin(ID3D11Device* device,XMVECTOR origin)
//{
//	if (this->vertices.size() <= 0) return;
//
//	for (int i = 0; i < this->vertices.size(); i++)
//	{
//		this->vertices[i].position.x -= XMVectorGetX(origin);
//		this->vertices[i].position.y -= XMVectorGetY(origin);
//		this->vertices[i].position.z -= XMVectorGetZ(origin);
//
//	}
//
//	this->initBuffers(device);
//}
//
//ID3D11Buffer* BaseMesh::CreateStagingBuffer(ID3D11Device* device, ID3D11Buffer* sourceBuffer,
//	ID3D11Buffer* stagingBufferPtr)
//{
//	D3D11_BUFFER_DESC bufferDesc = {};
//	sourceBuffer->GetDesc(&bufferDesc);
//
//	bufferDesc.Usage = D3D11_USAGE_STAGING;      // Staging usage for CPU access
//	bufferDesc.BindFlags = 0;                    // No binding flags
//	bufferDesc.CPUAccessFlags = D3D11_CPU_ACCESS_READ; // Allow CPU read
//
//	HRESULT hr = device->CreateBuffer(&bufferDesc, nullptr, &stagingBufferPtr);
//	if (FAILED(hr)) {
//		// Handle error (e.g., log or throw an exception)
//		return nullptr;
//	}
//	return stagingBufferPtr;
//}
//
//void BaseMesh::computeBoundingBox()
//{
//
//
//	boundingBoxMin = XMFLOAT3(D3D11_FLOAT32_MAX,D3D11_FLOAT32_MAX,D3D11_FLOAT32_MAX );
//	boundingBoxMax = XMFLOAT3(-999.999f,-999.999f,-999.999f);
//	// Iterate through indices to compute the bounding box
//	for (UINT i = 0; i < indexCount; ++i) {
//		const XMFLOAT3& position = vertices[indices[i]].position;
//
//		boundingBoxMin.x = min(boundingBoxMin.x, position.x);
//		boundingBoxMin.y = min(boundingBoxMin.y, position.y);
//		boundingBoxMin.z = min(boundingBoxMin.z, position.z);
//
//		boundingBoxMax.x = max(boundingBoxMax.x, position.x);
//		boundingBoxMax.y = max(boundingBoxMax.y, position.y);
//		boundingBoxMax.z = max(boundingBoxMax.z, position.z);
//	}
//
//
//
//
//	
//}
//
//int BaseMesh::getIndexCount()
//{
//	return indexCount;
//}
//
//
//// Sends geometry data to the GPU. Default primitive topology is TriangleList.
//// To render alternative topologies this function needs to be overwritten.
//void BaseMesh::sendData(ID3D11DeviceContext* deviceContext, D3D_PRIMITIVE_TOPOLOGY top)
//{
//	unsigned int stride;
//	unsigned int offset;
//	
//	// Set vertex buffer stride and offset.
//	stride = sizeof(VertexType);
//	offset = 0;
//
//	deviceContext->IASetVertexBuffers(0, 1, &vertexBuffer, &stride, &offset);
//	deviceContext->IASetIndexBuffer(indexBuffer, DXGI_FORMAT_R32_UINT, 0);
//	deviceContext->IASetPrimitiveTopology(top);
//}
//
//
//
//
