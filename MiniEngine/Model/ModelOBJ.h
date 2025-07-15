#pragma once
#include "VectorMath.h"
#include "TextureManager.h"
#include "GpuBuffer.h"
#include "DescriptorHeap.h"
#include "../Core/Math/BoundingBox.h"
#include "Geometry/BaseMesh.h"
#include "ModelH3D.h"
#include "ModelLoader.h"

struct H3DVertex {
    XMFLOAT3 position;
    XMFLOAT2 uv;
    XMFLOAT3 normal;
    XMFLOAT3 tangent;
    XMFLOAT3 bitangent;
};


class ModelOBJ
{
public:

    ModelOBJ();
    ~ModelOBJ();

    void Clear();


    Mesh* meshPtr = new Mesh();
//    std::vector<XMFLOAT3> m_pPositions;
//    std::vector<XMFLOAT3> m_pNormals;
//    std::vector<XMFLOAT2> m_pTexCoords;
    std::vector<H3DVertex> m_pVertices;
    std::vector<uint16_t> m_pIndices;


    virtual bool Load(const std::wstring& filename);
    //Calculates per face normals from know vertices and faces
    void CalculateNormals();



     Renderer::ModelData CreateModelData();
     void AttemptCreateH3DModel(ModelH3D* modelToFill);
     void InsertIntoH3DModel(ModelH3D* modelToFill);
     
     uint32_t GetVertexStride() const { return m_VertexStride; }
    const D3D12_VERTEX_BUFFER_VIEW& GetVertexBuffer() const { return m_VertexBufferView; }
    const D3D12_INDEX_BUFFER_VIEW& GetIndexBuffer() const { return m_IndexBufferView; }


//    void CreateVertexBufferSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;
//    void CreateIndexBufferSRV(D3D12_CPU_DESCRIPTOR_HANDLE cpuHandle) const;

    // These are used at runtime during rendering
//    ByteAddressBuffer m_VertexBuffer;
//    ByteAddressBuffer m_IndexBuffer;
    ByteAddressBuffer m_GeometryBuffer;

    D3D12_VERTEX_BUFFER_VIEW m_VertexBufferView;
    D3D12_INDEX_BUFFER_VIEW m_IndexBufferView;
    uint32_t m_VertexStride;

};

