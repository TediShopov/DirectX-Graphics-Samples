#include "ModelOBJ.h"
#include "fstream"
#include "sstream"
#include "iostream"

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


	std::ifstream file(filename);
    if (!file) {
        return false;
    }

    m_pPositions.clear();
    m_pIndices.clear();

    //m_pVertices.clear();

    std::string line;
    while (std::getline(file, line)) {
        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "v") {
            XMFLOAT3 pos;
            ss >> pos.x >> pos.y >> pos.z;
            this->m_pPositions.push_back(pos);
            XMFLOAT2 tex = { 0,0 }; XMFLOAT3 normal = { 0,0, 0};
            this->m_pVertices.push_back({ pos,tex,normal });
        }
//        else if (type == "vt") {
//            XMFLOAT2 uv;
//            ss >> uv.x >> uv.y;
//            uv.y = 1.0f - uv.y; // Flip V
//            this->m_pTexCoords.push_back(uv);
//        }
//        else if (type == "vn") {
//            XMFLOAT3 norm;
//            ss >> norm.x >> norm.y >> norm.z;
//            this->m_pNormals.push_back(norm);
//        }
        else if (type == "f") {

            //If VT or VN are 0 just pad them with mock values



            std::string vStr;
			ss >> vStr;
            int pIdx = 0;
            int pIdxOne = 0;
            int pIdxTwo = 0;
			//sscanf(vStr.c_str(), "%d %d %d", &pIdx, &pIdxOne, &pIdxTwo);
			sscanf(vStr.c_str(), "%d", &pIdx);
			ss >> vStr;
			sscanf(vStr.c_str(), "%d", &pIdxOne);
			ss >> vStr;
			sscanf(vStr.c_str(), "%d", &pIdxTwo);
            m_pIndices.push_back(pIdx-1);
            m_pIndices.push_back(pIdxOne-1);
            m_pIndices.push_back(pIdxTwo - 1 );
        }
    }
    m_VertexBuffer.Create(L"Vertex Buffer", m_pVertices.size(), sizeof(VertexType),m_pVertices.data());
    m_IndexBuffer.Create(L"Index Buffer", m_pIndices.size(), sizeof(UINT),m_pIndices.data());

    m_VertexBufferView = m_VertexBuffer.VertexBufferView(0, sizeof(VertexType)*m_pVertices.size(), sizeof(VertexType));
    m_IndexBufferView = m_IndexBuffer.IndexBufferView(0);

    return true;


}
