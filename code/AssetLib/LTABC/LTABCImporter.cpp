//
// Created by melissaa on 09/04/25.
//

#include "assimp/Exporter.hpp"
#include "assimp/scene.h"
#ifndef ASSIMP_BUILD_NO_LTABC_IMPORTER
#include "LTABCImporter.h"
#include "assimp/IOSystem.hpp"

namespace Assimp {
static constexpr aiImporterDesc desc = {
    "Lithtech ABC Importer",
    "melissaautumn",
    "melissaautumn",
    "Supports v9 to v13",
    aiImporterFlags_SupportBinaryFlavour,
    9,
    0,
    13,
    0,
    "abc"
};

LTABCImporter::~LTABCImporter() {
    delete m_Buffer;
    delete m_MeshHeader;
    delete m_PieceHeader;
    m_FileSize = 0;
}

bool Assimp::LTABCImporter::CanRead(const std::string &pFile, IOSystem *pIOHandler, bool) const {
    // If we have a stream handler check the file version
    if (pIOHandler) {
        std::unique_ptr<IOStream> pStream(pIOHandler->Open(pFile, "rb"));
        pStream->Seek(12, aiOrigin_SET);

        uint32_t meshVersion = 0;
        pStream->Read(&meshVersion, sizeof(meshVersion), 1);

        // Version not supported!
        if (meshVersion < desc.mMinMajor || meshVersion > desc.mMaxMajor) {
            return false;
        }
    }

    // Otherwise simply check the file format
    return SimpleExtensionCheck(pFile, "abc");
}

void Assimp::LTABCImporter::SetupProperties(const Importer *pImp) {
    BaseImporter::SetupProperties(pImp);
}

const aiImporterDesc *Assimp::LTABCImporter::GetInfo() const {
    return &desc;
}

void Assimp::LTABCImporter::InternReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler) {
    std::unique_ptr<IOStream> pStream(pIOHandler->Open(pFile, "rb"));

    // Check whether we can read from the file
    if (pStream == nullptr) {
        throw DeadlyImportError("Failed to open ABC file ", pFile, ".");
    }

    m_FileSize = pStream->FileSize();
    m_Buffer = new StreamReaderLE(pIOHandler->Open(pFile, "rb"));

    m_Scene = pScene;
    m_Scene->mRootNode = new aiNode("<ABC_Root>");

    m_MeshHeader = new LTABC::Header;

    int64_t nextSectionOffset = 0;
    while (nextSectionOffset != -1) {
        m_Buffer->SetCurrentPos(nextSectionOffset);

        auto sectionName = ReadLTString();
        nextSectionOffset = m_Buffer->GetI4();

        if (sectionName == SECTION_HEADER) {
            auto ioHeader = m_Buffer->Get<LTABC::IOHeader>();
            uint32_t unkV13Value = 0;
            m_MeshVersion = ioHeader.Version;

            if (m_MeshVersion == 13) {
                unkV13Value = m_Buffer->GetU4();
            }

            auto commandString = ReadLTString();
            auto internalRadius = m_Buffer->GetF4();
            auto lodDistanceCount = m_Buffer->GetU4();
            m_Buffer->IncPtr(60); // Skip padding

            // Load-up the header
            m_MeshHeader->LoadHeader(ioHeader, commandString, internalRadius, lodDistanceCount, unkV13Value);
        } else if (sectionName == SECTION_PIECES) {
            ai_assert(ReadPieces());
        } else if (sectionName == SECTION_NODES) {
            ai_assert(ReadNodes());
        }
    }

    // Build the mesh and all
    ai_assert(BuildMesh());

    std::string out = std::string(pFile + ".gltf");
    ::Assimp::Exporter exporter;
    exporter.Export(m_Scene, "gltf2", out);
}

bool LTABCImporter::ReadPieces() {
    CheckBuffer();

    m_PieceHeader = new LTABC::PieceHeader();
    m_PieceHeader->WeightCount = m_Buffer->GetI4();
    m_PieceHeader->PieceCount = m_Buffer->GetI4();

    for (auto i = 0; i < static_cast<int32_t>(m_PieceHeader->PieceCount); ++i) {
        LTABC::Piece piece = {};
        piece.MaterialIndex = m_Buffer->GetU2();
        piece.SpecularPower = m_Buffer->GetF4();
        piece.SpecularScale = m_Buffer->GetF4();
        piece.LODWeight = m_MeshHeader->Version > 9 ? m_Buffer->GetF4() : 0.0f;
        piece.Unknown = m_Buffer->GetU2();
        piece.Name = ReadLTString();

        for (auto l = 0; l < static_cast<int32_t>(m_MeshHeader->LODCount); ++l) {
            LTABC::LOD lod = {};

            lod.FaceCount = m_Buffer->GetU4();
            for (auto f = 0; f < static_cast<int32_t>(lod.FaceCount); ++f) {
                LTABC::Face face = {};
                // Struct is padded to 12 bytes, but this is tightly packed.
                m_Buffer->CopyAndAdvance(&face.Vertices[0], /*sizeof(LTABC::FaceVertex)*/ 10);
                m_Buffer->CopyAndAdvance(&face.Vertices[1], /*sizeof(LTABC::FaceVertex)*/ 10);
                m_Buffer->CopyAndAdvance(&face.Vertices[2], /*sizeof(LTABC::FaceVertex)*/ 10);
                lod.Faces.push_back(face);
            }

            lod.VertexCount = m_Buffer->GetU4();
            for (auto v = 0; v < static_cast<int32_t>(lod.VertexCount); ++v) {
                LTABC::Vertex vertex = {};

                vertex.WeightCount = m_Buffer->GetU2();
                vertex.SubLODVertexIndex = m_Buffer->GetU2();

                for (auto w = 0; w < static_cast<int32_t>(vertex.WeightCount); w++) {
                    LTABC::Weight weight = {};
                    m_Buffer->CopyAndAdvance(&weight, sizeof(LTABC::Weight));
                    vertex.Weights.push_back(weight);
                }

                m_Buffer->CopyAndAdvance(&vertex.Location, sizeof(LTABC::LTVector));
                m_Buffer->CopyAndAdvance(&vertex.Normal, sizeof(LTABC::LTVector));

                lod.Vertices.push_back(vertex);
            }

            piece.LODs.push_back(lod);
        }

        m_PieceHeader->Pieces.push_back(piece);
    }

    return true;
}
bool LTABCImporter::ReadNodes() {
    CheckBuffer();

    // A mesh shouldn't have 0 nodes!
    if (m_MeshHeader->NodeCount == 0) {
        return false;
    }

    // Read in the list of nodes and while we're doing that construct links to parent->child and a few more handy things
    std::vector<int> childStack = {};
    for (auto i = 0; i < static_cast<int>(m_MeshHeader->NodeCount); i++) {
        auto *node = new LTABC::Node();
        LTABC::LTMatrix bindMatrix = {};

        node->Name = ReadLTString();
        node->Index = m_Buffer->GetU2();
        node->Flags = m_Buffer->GetU1();
        m_Buffer->CopyAndAdvance(&bindMatrix, sizeof(LTABC::LTMatrix));
        node->BindMatrix = node->InvBindMatrix = LTABC::LTMatrix2aiMatrix(bindMatrix);
        node->InvBindMatrix.Inverse();
        node->ChildCount = m_Buffer->GetU4();

        if (!childStack.empty()) {
            const auto& parentNode = m_Nodes[childStack.back()];
            node->Parent = parentNode;
            if (parentNode) {
                childStack.pop_back();
            }
        }

        childStack.insert(childStack.end(), node->ChildCount, node->Index);

        m_Nodes.push_back(node);
    }

    return true;
}

bool LTABCImporter::BuildMesh() const {
    m_Scene->mNumMeshes = m_PieceHeader->PieceCount;
    m_Scene->mMeshes = new aiMesh *[m_PieceHeader->PieceCount];

    struct BoneData {
        LTABC::Node* LTNode;
        aiNode* boneNode{};
    };

    auto pieceRoot = new aiNode("<Piece Root>");
    auto nodeRoot = new aiNode("<Node Root>");

    auto meshIdx = 0;

    std::map<int, aiMaterial*> materials;
    std::vector<aiNode*> boneNodes;
    std::vector<BoneData> bones;
    std::vector<int> childStack;

    for (const auto& node : m_Nodes) {
        auto* boneNode = new aiNode(node->Name);

        boneNode->mTransformation = node->BindMatrix;

        // Set our parent, this is also where we'll be undoing the absolute positioning
        if (node->Parent) {
            auto parentNode = boneNodes[node->Parent->Index];
            parentNode->addChildren(1, &boneNode);
            boneNode->mParent = parentNode;

            auto parentInv = node->Parent->InvBindMatrix;
            boneNode->mTransformation = parentInv * boneNode->mTransformation;
        }

        // If we have no bone nodes, then set our parent to the bone root
        if (boneNodes.empty()) {
            nodeRoot->addChildren(1, &boneNode);
            boneNode->mParent = nodeRoot;
        }


        boneNode->mMetaData = new aiMetadata();
        boneNode->mMetaData->Add("index", static_cast<uint64_t>(node->Index));
        boneNode->mMetaData->Add("flags", static_cast<uint64_t>(node->Flags));
        boneNode->mMetaData->Add("child_count", static_cast<uint64_t>(node->ChildCount));

        auto tmp = BoneData();
        tmp.boneNode = boneNode;
        tmp.LTNode = node;
        bones.push_back(tmp);
        boneNodes.push_back(boneNode);
    }

    for (auto piece : m_PieceHeader->Pieces) {
        auto lod = piece.LODs[0];
        auto mesh = new aiMesh();
        auto pieceNode = new aiNode(piece.Name);

        mesh->mName = piece.Name;
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

        mesh->mNumFaces = lod.FaceCount;
        mesh->mFaces = new aiFace[lod.FaceCount];

        mesh->mNumVertices = lod.FaceCount * 3;

        mesh->mVertices = new aiVector3D[mesh->mNumVertices];
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];

        mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];
        mesh->mNumUVComponents[0] = 2;

        mesh->mMaterialIndex = piece.MaterialIndex;
        if (!materials.count(piece.MaterialIndex)) {
            auto shading = aiShadingMode_Phong;  // Should be aiShadingMode_Gouraud, but I don't know if shininess keys will stick without it.
            auto mat = new aiMaterial();
            mat->AddProperty(&piece.SpecularScale, 1, AI_MATKEY_SHININESS);
            mat->AddProperty(&piece.SpecularPower, 1, AI_MATKEY_SHININESS_STRENGTH);
            mat->AddProperty(&shading, 1, AI_MATKEY_SHADING_MODEL);
            materials[piece.MaterialIndex] = mat;
        }

        struct VertData {
            aiVector3D verts = {};
            aiVector3D normals = {};
            aiVector3D uvs = {};
        };

        std::map<int, VertData> duplicateVertData = {};
        std::map<int, std::vector<aiVertexWeight>> pieceWeights = {};

        auto currentIndex = 0;

        // Loop through the list of faces, and then face vertices [Vertex Index, UV] to do a first pass on any
        // currently unique vertex indexes, and store any subsequent non-unique vertex indexes for a second pass below.
        for (auto i = 0; i < static_cast<int>(lod.FaceCount); i++) {
            mesh->mFaces[i].mNumIndices = 3;
            mesh->mFaces[i].mIndices = new unsigned int[3];

            for (auto v = 0; v < 3; v++) {
                const auto &face = lod.Faces[i].Vertices[v];
                const auto &vertex = lod.Vertices[face.VertexIndex];

                if (duplicateVertData.count(face.VertexIndex)) {
                    // Duplicate vertex data
                    auto newVertexIndex = static_cast<int>(lod.VertexCount) - 1 + static_cast<int>(duplicateVertData.size()) - 1;

                    duplicateVertData[newVertexIndex] = {
                        aiVector3D(vertex.Location.x, vertex.Location.y, vertex.Location.z),
                        aiVector3D(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z),
                        aiVector3D(face.TexCoord.u, face.TexCoord.v, 0.0f),
                    };
                    mesh->mFaces[i].mIndices[v] = newVertexIndex;
                } else {
                    mesh->mFaces[i].mIndices[v] = currentIndex;
                    mesh->mVertices[currentIndex] = aiVector3D(vertex.Location.x, vertex.Location.y, vertex.Location.z);
                    mesh->mNormals[currentIndex] = aiVector3D(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
                    mesh->mTextureCoords[0][currentIndex] = aiVector3D(face.TexCoord.u, face.TexCoord.v, 0.0f);
                    currentIndex++;
                }

                for (auto w = 0; w < vertex.WeightCount; w++) {
                    int nodeIndex = static_cast<int>(vertex.Weights[w].NodeIndex);
                    pieceWeights[nodeIndex].emplace_back(mesh->mFaces[i].mIndices[v], vertex.Weights[w].Bias);
                }
            }
        }

        // Now we can insert any non-unique vertex indexes.
        for (const auto& [idx, vertexData] : duplicateVertData) {
            mesh->mVertices[currentIndex] = vertexData.verts;
            mesh->mNormals[currentIndex] = vertexData.normals;
            mesh->mTextureCoords[0][currentIndex] = vertexData.uvs;
            currentIndex++;
        }

        mesh->mNumBones = bones.size();
        mesh->mBones = new aiBone *[mesh->mNumBones];

        for (auto idx = 0; idx < static_cast<int>(bones.size()); idx++) {
            auto bone = new aiBone();
            const auto &boneData = bones[idx];

            // Not in the weight list? Create an empty bone instead.
            if (!pieceWeights.count(idx)) {
                bone->mName = boneData.LTNode->Name;
                bone->mWeights = nullptr;
                bone->mOffsetMatrix = boneData.LTNode->InvBindMatrix;
                bone->mNumWeights = 0;
                mesh->mBones[idx] = bone;
                continue;
            }

            const auto& weightList = pieceWeights[idx];
            bone->mNode = boneData.boneNode;
            bone->mName = boneData.LTNode->Name;
            bone->mOffsetMatrix = boneData.LTNode->InvBindMatrix;
            bone->mNumWeights = weightList.size();
            bone->mWeights = new aiVertexWeight[bone->mNumWeights];
            std::copy(weightList.begin(), weightList.end(), bone->mWeights);
            mesh->mBones[idx] = bone;
        }


        pieceNode->mMeshes = new unsigned int[1];
        pieceNode->mMeshes[0] = meshIdx;
        pieceNode->mNumMeshes = 1;
        pieceRoot->addChildren(1, &pieceNode);
        pieceNode->mParent = pieceRoot;

        m_Scene->mMeshes[meshIdx] = mesh;
        meshIdx++;
    }

    m_Scene->mRootNode->addChildren(1, &pieceRoot);
    m_Scene->mRootNode->addChildren(1, &nodeRoot);
    pieceRoot->mParent = m_Scene->mRootNode;
    nodeRoot->mParent = m_Scene->mRootNode;

    // Add the materials we've collected to the scene
    m_Scene->mMaterials = new aiMaterial *[materials.size()];
    m_Scene->mNumMaterials = materials.size();
    for (const auto& [idx, mat] : materials) {
        ai_assert(idx < static_cast<int>(m_Scene->mNumMaterials));
        m_Scene->mMaterials[idx] = mat;
    }

    return true;
}

std::string LTABCImporter::ReadLTString() {
    CheckBuffer();

    const uint16_t len = m_Buffer->GetU2();

    // Sanity check
    ai_assert(len < 1024);

    // Don't even try to read an empty string...
    if (len == 0) {
        return { "" };
    }

    char string[len + 1];
    m_Buffer->CopyAndAdvance(string, len);
    string[len] = '\0';
    return { string };
}

} // namespace Assimp
#endif
