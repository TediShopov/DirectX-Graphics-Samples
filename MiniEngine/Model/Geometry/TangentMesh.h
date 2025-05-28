//#pragma once
//#include <directxmath.h>
////#include <assimp/Importer.hpp>
//
//#include "BaseMesh.h"
//#include <iostream>
//class TangentMesh :
//    public BaseMesh
//{
//private:
//	struct VertexType_Tangent 
//	{
//		VertexType vertex;
//		DirectX::XMFLOAT3 tangent;
//		XMFLOAT3 bitangent;
//	};
//public:
//
//	BaseMesh* meshToTransform;
//	ID3D12DeviceContext* context;
//	VertexType_Tangent* tangentVertices;
//	ID3D12Buffer* tangentVerticesBuffer;
//
//
//	TangentMesh(ID3D12Device* device,ID3D12DeviceContext* context,BaseMesh* mesh)
//		:
//		BaseMesh(device, context),
//	context(context), meshToTransform(mesh)
//	{
//		initBuffers(device);
//	}
//
//	void sendData(ID3D12DeviceContext* deviceContext, D3D_PRIMITIVE_TOPOLOGY top) override
//	{
//		unsigned int stride;
//		unsigned int offset;
//
//		// Set vertex buffer stride and offset.
//		stride = sizeof(VertexType_Tangent);
//		offset = 0;
//
//		deviceContext->IASetVertexBuffers(0, 1, &this->vertexBuffer, &stride, &offset);
//		deviceContext->IASetIndexBuffer(meshToTransform->indexBuffer, DXGI_FORMAT_R32_UINT, 0);
//		deviceContext->IASetPrimitiveTopology(top);
//	}
//
//
//	virtual void initBuffers(ID3D12Device* device) 
//	{
//		
//		int a = 3;
//
//		vertexCount = meshToTransform->getVertexCount();
//		this->indexBuffer = meshToTransform->indexBuffer;
//		this->indexCount = meshToTransform->getIndexCount();
//		VertexType* vertices = new VertexType[meshToTransform->getVertexCount()];
//		VertexType_Tangent* tangentVertices = new VertexType_Tangent[meshToTransform->getVertexCount()];
//
//
//		//Create staging buffer of vertices to read from
//		auto stagingBuffer= createStagingVertexBuffer(device, vertices);
//
//		//Copy the vertex data to the vertex buffer 
//		//TODO might be unnecessary
//		fillVertexData(vertices, vertexBuffer);
//
//		this->vertices = meshToTransform->vertices;
//		this->indices = meshToTransform->indices;
//
//		//calulate tangents
//		calculateTangents(vertices, tangentVertices);
//
//		//Create buffer with filled tangents
//		  tangentVerticesBuffer=createVertexTangentBuffer(device, tangentVertices);
//
//		//Replace buffer
//		this->vertexBuffer = tangentVerticesBuffer;
//
//		
//	}
//
//	//Return tangent as first and bitangent as second
//	std::pair<XMFLOAT3, XMFLOAT3> calculateTriangleTangents(VertexType vertex1, VertexType vertex2, VertexType vertex3)
//	{
//		XMFLOAT3 edge1, edge2;
//		XMFLOAT2 deltaUV1, deltaUV2;
//		XMVECTOR pos1, pos2, pos3, uv1, uv2, uv3;
//		std::pair<XMFLOAT3, XMFLOAT3> tangents;
//
//		pos1 = XMLoadFloat3(&vertex1.position);
//		pos2 = XMLoadFloat3(&vertex2.position);
//		pos3 = XMLoadFloat3(&vertex3.position);
//
//		uv1 = XMLoadFloat2(&vertex1.texture);
//		uv2 = XMLoadFloat2(&vertex2.texture);
//		uv3 = XMLoadFloat2(&vertex3.texture);
//
//
//		XMStoreFloat3(&edge1, pos2 - pos1);
//		XMStoreFloat3(&edge2, pos3 - pos1);
//
//		XMStoreFloat2(&deltaUV1, uv2 - uv1);
//		XMStoreFloat2(&deltaUV2, uv3 - uv1);
//
//		float f = 1.0f / (deltaUV1.x * deltaUV2.y - deltaUV2.x * deltaUV1.y);
//
//		tangents.first.x = f * (deltaUV2.y * edge1.x - deltaUV1.y * edge2.x);
//		tangents.first.y = f * (deltaUV2.y * edge1.y - deltaUV1.y * edge2.y);
//		tangents.first.z = f * (deltaUV2.y * edge1.z - deltaUV1.y * edge2.z);
//
//		tangents.second.x = f * (-deltaUV2.x * edge1.x + deltaUV1.x * edge2.x);
//		tangents.second.y = f * (-deltaUV2.x * edge1.y + deltaUV1.x * edge2.y);
//		tangents.second.z = f * (-deltaUV2.x * edge1.z + deltaUV1.x * edge2.z);
//
//		return tangents;
//	}
//
//	/// https://learnopengl.com/Advanced-Lighting/Normal-Mapping
//	void calculateTangents(VertexType* vertices, VertexType_Tangent* outTangentVertices) 
//	{
//		
//
//		for (int i = 0; i < meshToTransform->getVertexCount(); i+=3)
//		{
//			//Calculate the tangents
//			auto tangents = calculateTriangleTangents(vertices[i], vertices[i + 1], vertices[i + 2]);
//
//			//Copty the vertex information 
//			outTangentVertices[i].vertex = vertices[i];
//			outTangentVertices[i+1].vertex = vertices[i+1];
//			outTangentVertices[i+2].vertex = vertices[i+2];
//
//			//Add tangents
//			outTangentVertices[i].tangent =tangents.first;
//			outTangentVertices[i + 1].tangent = tangents.first;
//			outTangentVertices[i + 2].tangent = tangents.first;
//
//			//Add bitangents
//			outTangentVertices[i].bitangent = tangents.second;
//			outTangentVertices[i + 1].bitangent = tangents.second;
//			outTangentVertices[i + 2].bitangent = tangents.second;
//		}
//	}
//
//	void fillVertexData(VertexType* vertex, ID3D12Buffer* vertexBuffer) 
//	{
//		if(vertexBuffer == nullptr)
//			return;
//		
//		D3D12_MAPPED_SUBRESOURCE ms;
//		context->Map(vertexBuffer, NULL, D3D12_MAP_READ, NULL, &ms);
//		auto gpuAccess = (VertexType*)ms.pData;
//		memcpy(vertex, gpuAccess, sizeof(VertexType) * meshToTransform->getVertexCount());
//		context->Unmap(vertexBuffer, NULL);
//	}
//
//	//Creating a staging buffer to copy all the vertex data to
//	ID3D12Buffer* createStagingVertexBuffer(ID3D12Device* device,VertexType* vertices)
//	{
//		//Crea
//		D3D12_BUFFER_DESC vertexBufferDesc;
//		vertexBufferDesc.Usage = D3D12_USAGE_STAGING;
//		vertexBufferDesc.ByteWidth = sizeof(VertexType) * meshToTransform->getVertexCount();
//		vertexBufferDesc.BindFlags = 0;
//		vertexBufferDesc.CPUAccessFlags = D3D12_CPU_ACCESS_WRITE | D3D12_CPU_ACCESS_READ;//Change for staging buffer
//		vertexBufferDesc.MiscFlags = 0;
//		vertexBufferDesc.StructureByteStride = sizeof(VertexType);
//	
//		// Give the subresource structure a pointer to the vertex data.
//		D3D12_SUBRESOURCE_DATA vertexData;
//		vertexData.pSysMem = meshToTransform->vertices.data();
//		vertexData.SysMemPitch = 0;
//		vertexData.SysMemSlicePitch = 0;
//
//
//		auto hres=device->CreateBuffer(&vertexBufferDesc, &vertexData, &vertexBuffer);
//		if (hres != 0) // HRESUTL IS NOT E_OK
//		{
//			printf("Creating staging buffer failed");
//		}
//
//		context->CopyResource(vertexBuffer, meshToTransform->vertexBuffer);
//		
//		return vertexBuffer;
//	}
//
//	ID3D12Buffer* createVertexTangentBuffer(ID3D12Device* device, VertexType_Tangent* vertexSrc)
//	{
//		ID3D12Buffer* dst;
//		D3D12_BUFFER_DESC tanVertBufferDesc;
//		// Set up the description of the static vertex buffer.
//		tanVertBufferDesc.Usage = D3D12_USAGE_DEFAULT;
//		tanVertBufferDesc.ByteWidth = sizeof(VertexType_Tangent) * vertexCount;
//		tanVertBufferDesc.BindFlags = D3D12_BIND_VERTEX_BUFFER;
//		tanVertBufferDesc.CPUAccessFlags = 0;
//		tanVertBufferDesc.MiscFlags = 0;
//		tanVertBufferDesc.StructureByteStride = 0;
//
//		// Give the subresource structure a pointer to the vertex data.
//		D3D12_SUBRESOURCE_DATA vertexData;
//		vertexData.pSysMem = vertexSrc;
//		vertexData.SysMemPitch = 0;
//		vertexData.SysMemSlicePitch = 0;
//		// Now create the vertex buffer.
//		device->CreateBuffer(&tanVertBufferDesc, &vertexData, &dst);
//		return dst;
//	}
//
//
//    
//};
//
