#include "ModelOBJ.h"
#include "fstream"
#include "sstream"
#include "iostream"
#include "algorithm"
#include <cstddef>

ModelOBJ::ModelOBJ() {
}

ModelOBJ::~ModelOBJ()
{
}

void ModelOBJ::Clear()
{
}

bool ModelOBJ::Load(const std::wstring& filename)
{
    ReadOBJData(filename);

    //For each face 
    for (size_t i = 0; i < m_pIndices.size(); i+=3)
    {
		H3DVertex& v0 = m_pVertices[m_pIndices[i]];
		H3DVertex& v1 = m_pVertices[m_pIndices[i+1]];
		H3DVertex& v2 = m_pVertices[m_pIndices[i+2]];
        CalculateTangentBasis(v0, v1, v2);
    }
    for (size_t i = 0; i < m_pVertices.size(); i++)
    {
		H3DVertex& v0 = m_pVertices[m_pIndices[i]];
        XMStoreFloat3(&v0.normal, XMVector3Normalize(XMLoadFloat3(&v0.normal)));
        XMStoreFloat3(&v0.tangent, XMVector3Normalize(XMLoadFloat3(&v0.tangent)));
        XMStoreFloat3(&v0.bitangent, XMVector3Normalize(XMLoadFloat3(&v0.bitangent)));
    }




    ComputeUnifiedBuffer();

    return true;


}

void ModelOBJ::CalculateTangentBasis(H3DVertex& va0,H3DVertex& va1, H3DVertex& va2)
{
    XMVECTOR v0 = XMLoadFloat3(&va0.position);
    XMVECTOR v1 = XMLoadFloat3(&va1.position);
    XMVECTOR v2 = XMLoadFloat3(&va2.position);

    XMVECTOR edge1 = v1 - v0;
    XMVECTOR edge2 = v2 - v0;

    float du1 = va1.uv.x - va0.uv.x;
    float dv1 = va1.uv.y - va0.uv.y;
    float du2 = va2.uv.x - va0.uv.x;
    float dv2 = va2.uv.y - va0.uv.y;

    float r = 1.0f / (du1 * dv2 - dv1 * du2);

    XMVECTOR tangent = r * (dv2 * edge1 - dv1 * edge2);
    XMVECTOR bitangent = r * (-du2 * edge1 + du1 * edge2);
    XMVECTOR normal = XMVector3Normalize(XMVector3Cross(edge1, edge2));

    // Optional: orthonormalize
    tangent = XMVector3Normalize(tangent - normal * XMVector3Dot(normal, tangent));
    bitangent = XMVector3Cross(normal, tangent);

    //Store in all vertices

    XMStoreFloat3(&va0.tangent, tangent + XMLoadFloat3(&va0.tangent));
    XMStoreFloat3(&va0.bitangent, bitangent + XMLoadFloat3(&va0.bitangent));
    XMStoreFloat3(&va0.normal, normal + XMLoadFloat3(&va0.normal));

    XMStoreFloat3(&va1.tangent, tangent + XMLoadFloat3(&va1.tangent));
    XMStoreFloat3(&va1.bitangent, bitangent + XMLoadFloat3(&va1.bitangent));
    XMStoreFloat3(&va1.normal, normal + XMLoadFloat3(&va1.normal));

    XMStoreFloat3(&va2.tangent, tangent + XMLoadFloat3(&va2.tangent));
    XMStoreFloat3(&va2.bitangent, bitangent + XMLoadFloat3(&va2.bitangent));
    XMStoreFloat3(&va2.normal, normal + XMLoadFloat3(&va2.normal));

}

bool ModelOBJ::ReadOBJData(const std::wstring& filename)
{
	std::ifstream file(filename);
    if (!file) {
        return false;
    }
    //m_pPositions.clear();
    m_pPositions.clear();
    m_pNormals.clear();
    m_pTexCoords.clear();

    m_pVertices.clear();
    m_pIndices.clear();

	std::map<std::string, uint16_t> uniqueVertexMap; // maps "v/vt/vn" to unique index




    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            XMFLOAT3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            m_pPositions.push_back(pos);
        }
        else if (type == "vt") {
            XMFLOAT2 uv;
            ss >> uv.x >> uv.y;
            uv.y = 1.0f - uv.y; // Flip V
            this->m_pTexCoords.push_back(uv);
        }
        else if (type == "vn") {
            XMFLOAT3 norm;
            ss >> norm.x >> norm.y >> norm.z;
            this->m_pNormals.push_back(norm);
        }
        else if (type == "f") {
            for (int i = 0; i < 3; ++i) {
                std::string vStr;
                ss >> vStr;

                if (uniqueVertexMap.count(vStr) == 0) {
                    int v = 0, vt = 0, vn = 0;
                    sscanf(vStr.c_str(), "%d/%d/%d", &v, &vt, &vn);

                    H3DVertex vertex{};
                    vertex.position = m_pPositions[v - 1];
                    vertex.uv = vt ? m_pTexCoords[vt - 1] : XMFLOAT2(0, 0);
                    vertex.normal = vn ? m_pNormals[vn - 1] : XMFLOAT3(0, 0, 0);
                    vertex.tangent = { 0,0,0 };
                    vertex.bitangent = { 0,0,0 };

                    uint16_t newIndex = static_cast<uint16_t>(m_pVertices.size());
                    uniqueVertexMap[vStr] = newIndex;
                    m_pVertices.push_back(vertex);
                }

                m_pIndices.push_back(uniqueVertexMap[vStr]);
            }
//        else if (type == "f") {
//
//            //If VT or VN are 0 just pad them with mock values
//
//
//
//            std::string vStr;
//			ss >> vStr;
//            int pIdx = 0;
//            int pIdxOne = 0;
//            int pIdxTwo = 0;
//			//sscanf(vStr.c_str(), "%d %d %d", &pIdx, &pIdxOne, &pIdxTwo);
//			sscanf(vStr.c_str(), "%d", &pIdx);
//			ss >> vStr;
//			sscanf(vStr.c_str(), "%d", &pIdxOne);
//			ss >> vStr;
//			sscanf(vStr.c_str(), "%d", &pIdxTwo);
//            m_pIndices.push_back(pIdx-1);
//            m_pIndices.push_back(pIdxOne-1);
//            m_pIndices.push_back(pIdxTwo - 1 );
//
//            //this->m_pPositions.push_back(pos);
//            XMFLOAT2 tex = { 0,0 }; XMFLOAT3 normal = { 0,0, 0};
//            XMFLOAT3 tangent = { 0,0,0 }; XMFLOAT3 bitangent = { 0,0,0 };
//            this->m_pVertices.push_back({ pos,tex,normal, tangent,bitangent});
        }
    }
    return true;
}

void ModelOBJ::ComputeUnifiedBuffer()
{
    size_t vbSize = m_pVertices.size() * sizeof(H3DVertex);
    size_t ibSize = m_pIndices.size() * sizeof(uint16_t);


    std::vector<byte> unifiedBuffer(vbSize + ibSize);
    void* vertexOffsetInGB= unifiedBuffer.data() +0;
    memcpy(vertexOffsetInGB, m_pVertices.data(), vbSize);

    void* indexOffset = unifiedBuffer.data() + vbSize;
    memcpy(indexOffset, m_pIndices.data(), ibSize);

    std::vector<H3DVertex> reconstrueVb(m_pVertices.size());
    std::vector<uint16_t> reconstructedIb(m_pIndices.size());
    memcpy(reconstrueVb.data(), vertexOffsetInGB, vbSize);

    memcpy(reconstructedIb.data(), indexOffset, ibSize);


    CalculateNormals();

    m_GeometryBuffer.Create(L"Geometry Buffer", vbSize+ibSize, 1,unifiedBuffer.data());
    m_VertexBufferView = m_GeometryBuffer.VertexBufferView(0, vbSize, sizeof(H3DVertex));
    m_IndexBufferView = m_GeometryBuffer.IndexBufferView(vbSize,ibSize, false);
}

void ModelOBJ::CalculateNormals()
{

    for (size_t i = 0; i < this->m_pIndices.size(); i+=3)
    {
		uint32_t i0 = m_pIndices[i];
		uint32_t i1 = m_pIndices[i + 1];
		uint32_t i2 = m_pIndices[i + 2];

		XMVECTOR p0 = XMLoadFloat3(&m_pVertices[i0].position);
		XMVECTOR p1 = XMLoadFloat3(&m_pVertices[i1].position);
		XMVECTOR p2 = XMLoadFloat3(&m_pVertices[i2].position);

		XMVECTOR edge1 = p1 - p0;
		XMVECTOR edge2 = p2 - p0;

		XMVECTOR faceNormal = XMVector3Cross(edge1, edge2);
		faceNormal = XMVector3Normalize(faceNormal);

		// Add the face normal to each vertex normal
		XMVECTOR n0 = XMLoadFloat3(&m_pVertices[i0].normal) + faceNormal;
		XMVECTOR n1 = XMLoadFloat3(&m_pVertices[i1].normal) + faceNormal;
		XMVECTOR n2 = XMLoadFloat3(&m_pVertices[i2].normal) + faceNormal;

		XMStoreFloat3(&m_pVertices[i0].normal, n0);
		XMStoreFloat3(&m_pVertices[i1].normal, n1);
		XMStoreFloat3(&m_pVertices[i2].normal, n2);

    }


    for (size_t i = 0; i < this->m_pVertices.size(); i++)
	{
		XMVECTOR normal = XMVector3Normalize(XMLoadFloat3(&m_pVertices[i].normal));
		XMStoreFloat3(&m_pVertices[i].normal, normal);

    }
}


void ModelOBJ::AttemptCreateH3DModel(ModelH3D* modelToFill)
{
    size_t vbSize = m_pVertices.size() * sizeof(H3DVertex);
    size_t ibSize = m_pIndices.size() * sizeof(uint16_t);


    //--- HEADER ---
    //Remember to initialize at a later stage
    modelToFill->m_Header.boundingBox;
    modelToFill->m_Header.indexDataByteSize = ibSize;
    modelToFill->m_Header.vertexDataByteSize= vbSize;
    modelToFill->m_Header.vertexDataByteSizeDepth = 0;
    modelToFill->m_Header.materialCount = 1;
    modelToFill->m_Header.meshCount = 1;


    //---MESHES---
    ModelH3D::Mesh h3dMesh;
    h3dMesh.attrib[ModelH3D::attrib_position].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_position].components = 3;
    h3dMesh.attrib[ModelH3D::attrib_position].offset = 0;

    h3dMesh.attrib[ModelH3D::attrib_texcoord0].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_texcoord0].offset = 12;
    h3dMesh.attrib[ModelH3D::attrib_texcoord0].components = 2;
    
    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 20;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;

    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 32;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;

    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 44;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;


    h3dMesh.attribsEnabled = 
        ModelH3D::attrib_position | ModelH3D::attrib_texcoord0 | ModelH3D::attrib_normal | ModelH3D::attrib_mask_tangent| ModelH3D::attrib_bitangent;

    h3dMesh.indexCount = m_pIndices.size();
    //h3dMesh.indexDataByteOffset = vbSize;
    h3dMesh.indexDataByteOffset = 0;

    h3dMesh.vertexCount = m_pVertices.size();
    h3dMesh.vertexDataByteOffset = 0;

    h3dMesh.vertexDataByteOffsetDepth = 0;

    h3dMesh.materialIndex = 0;
    h3dMesh.vertexStride = sizeof(H3DVertex);

    h3dMesh.attribsEnabledDepth = 0;
    h3dMesh.vertexStrideDepth = 0;
    h3dMesh.vertexCountDepth = 0;

    ModelH3D::Mesh* meshArray= new ModelH3D::Mesh[1]{
        h3dMesh
    }
    ;
    //meshArray[0] = *meshPtr;
    modelToFill->m_pMesh = meshArray;


    //---Materials ---
    ModelH3D::Material* material = new ModelH3D::Material;
    material->ambient = Color(0.1f,0.1f,0.1f,1.0f);
    material->diffuse = Color(1.0f,0.1f,0.1f,1.0f);
    material->specular = Color(1.0f,1.0f,1.0,1.0f);

    modelToFill->m_pMaterial = material;


    //--- Unified Geometry Data ---
    //modelToFill->m_pIndexData = (unsigned char*)m_pIndices;
    modelToFill->m_pIndexData = reinterpret_cast<unsigned char*>(m_pIndices.data());
    modelToFill->m_pVertexData = reinterpret_cast<unsigned char*>(m_pVertices.data());
    //memcpy(   modelToFill->m_pIndexData, m_pIndices.data(), ibSize);
    //memcpy(modelToFill->m_pVertexData, m_pVertices.data(), vbSize);
}

bool IsDataDifferent(unsigned char* collectionA,unsigned char* collectionB, UINT size)
{

    //Check for discrepancies
    for (int i = 0; i < size; i++)
    {
        if (collectionA[i] != collectionB[i])
        {
            return true;
        }
    }
    return false;
}

void ModelOBJ::InsertIntoH3DModel(ModelH3D* modelToFill) {

    auto lastMesh = modelToFill->GetMesh(modelToFill->GetMeshCount() - 1);
    UINT oldIndexDataByteSize = modelToFill->m_Header.indexDataByteSize;
    UINT oldVertexDataByteSize = modelToFill->m_Header.vertexDataByteSize;


   


    size_t vbSize = m_pVertices.size() * sizeof(H3DVertex);
    size_t ibSize = m_pIndices.size() * sizeof(uint16_t);

    //--- HEADER ---
    modelToFill->m_Header.indexDataByteSize =modelToFill->m_Header.indexDataByteSize + ibSize;
    modelToFill->m_Header.vertexDataByteSize=modelToFill->m_Header.vertexDataByteSize + vbSize;
    modelToFill->m_Header.meshCount+=1;
    //modelToFill->m_Header.meshCount;


    //---MESHES---
    ModelH3D::Mesh h3dMesh;
    h3dMesh.attrib[ModelH3D::attrib_position].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_position].components = 3;
    h3dMesh.attrib[ModelH3D::attrib_position].offset = 0;

    h3dMesh.attrib[ModelH3D::attrib_texcoord0].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_texcoord0].offset = 12;
    h3dMesh.attrib[ModelH3D::attrib_texcoord0].components = 2;
    
    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 20;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;

    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 32;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;

    h3dMesh.attrib[ModelH3D::attrib_normal].format = ModelH3D::attrib_format_float;
    h3dMesh.attrib[ModelH3D::attrib_normal].offset = 44;
    h3dMesh.attrib[ModelH3D::attrib_normal].components = 3;


    //Get Last Mesh In Array To Help Wiht Byte Offsets

    h3dMesh.attribsEnabled = 
        ModelH3D::attrib_position | ModelH3D::attrib_texcoord0 | ModelH3D::attrib_normal | ModelH3D::attrib_mask_tangent| ModelH3D::attrib_bitangent;

    h3dMesh.indexCount = m_pIndices.size();
    h3dMesh.indexDataByteOffset = lastMesh.indexDataByteOffset + lastMesh.indexCount * sizeof(uint16_t);
    h3dMesh.vertexCount = m_pVertices.size();
    h3dMesh.vertexDataByteOffset = lastMesh.vertexDataByteOffset + lastMesh.vertexCount * sizeof(H3DVertex);
    h3dMesh.materialIndex = 2;
    h3dMesh.vertexStride = sizeof(H3DVertex);

    
    //Create a new mesh buffer and copy the values of old
    ModelH3D::Mesh* newMeshes = new ModelH3D::Mesh[modelToFill->GetMeshCount()];
    memcpy(newMeshes, modelToFill->m_pMesh, (modelToFill->GetMeshCount() - 1) * sizeof(ModelH3D::Mesh));
    newMeshes[modelToFill->GetMeshCount() - 1] = h3dMesh;

    modelToFill->m_pMesh = newMeshes;


    //--- Unified Geometry Data ---
    //modelToFill->m_pIndexData = (unsigned char*)m_pIndices;



    unsigned char* newIndexData = new unsigned char[modelToFill->m_Header.indexDataByteSize];
    unsigned char* newIndexDepthData = new unsigned char[modelToFill->m_Header.indexDataByteSize];
    unsigned char* newVertexData = new unsigned char[modelToFill->m_Header.vertexDataByteSize];

    //COpy old data
    memcpy(newIndexData, modelToFill->m_pIndexData, oldIndexDataByteSize);
    memcpy(newIndexDepthData, modelToFill->m_pIndexDataDepth, oldIndexDataByteSize);
    memcpy(newVertexData, modelToFill->m_pVertexData, oldVertexDataByteSize);
    
    unsigned char* indexDataInsertion = newIndexData + oldIndexDataByteSize;
    unsigned char* indexDataDepthInsertion = newIndexDepthData + oldIndexDataByteSize;
    unsigned char* vertexDataInsertion = newVertexData + oldVertexDataByteSize;


    memcpy(indexDataInsertion, m_pIndices.data(), ibSize);
    memcpy(indexDataDepthInsertion, m_pIndices.data(), ibSize);
    memcpy(vertexDataInsertion, m_pVertices.data(), vbSize);


    bool indexIsDifferent =  IsDataDifferent(modelToFill->m_pIndexData, newIndexData, oldIndexDataByteSize);
    bool vertexDataIsDifferete =  IsDataDifferent(modelToFill->m_pVertexData, newVertexData, oldVertexDataByteSize);
    bool indexDepthDataIsDifferent =  IsDataDifferent(modelToFill->m_pIndexDataDepth, newIndexDepthData, oldIndexDataByteSize);


    modelToFill->m_pIndexData = newIndexData;
    modelToFill->m_pVertexData = newVertexData;
    modelToFill->m_pIndexDataDepth = newIndexDepthData;






//    indexDataInsertion = reinterpret_cast<unsigned char*>(m_pIndices.data());
//    vertexDataInsertion = reinterpret_cast<unsigned char*>(m_pVertices.data());
    //memcpy(   modelToFill->m_pIndexData, m_pIndices.data(), ibSize);
    //memcpy(modelToFill->m_pVertexData, m_pVertices.data(), vbSize);

}
