//
// Created by melissaa on 09/04/25.
//

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
        }
    }

    // Build the mesh and all
    ai_assert(BuildMesh());
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

bool LTABCImporter::BuildMesh() const {
    m_Scene->mNumMeshes = m_PieceHeader->PieceCount;
    m_Scene->mMeshes = new aiMesh *[m_PieceHeader->PieceCount];

    auto meshIdx = 0;
    for (auto piece : m_PieceHeader->Pieces) {
        auto lod = piece.LODs[0];
        auto mesh = new aiMesh();

        mesh->mName = piece.Name;
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;

        mesh->mNumFaces = lod.FaceCount;
        mesh->mFaces = new aiFace[lod.FaceCount];

        mesh->mNumVertices = lod.VertexCount;

        mesh->mVertices = new aiVector3D[lod.VertexCount];
        mesh->mNormals = new aiVector3D[lod.VertexCount];

        mesh->mTextureCoords[0] = new aiVector3D[lod.FaceCount];
        mesh->mNumUVComponents[0] = lod.FaceCount;

        for (int i = 0; i < static_cast<int32_t>(lod.FaceCount); ++i) {
            // Vertex indices
            mesh->mFaces[i].mNumIndices = 3;
            mesh->mFaces[i].mIndices = new unsigned int[mesh->mFaces[i].mNumIndices];
            mesh->mFaces[i].mIndices[0] = lod.Faces[i].Vertices[0].VertexIndex;
            mesh->mFaces[i].mIndices[1] = lod.Faces[i].Vertices[1].VertexIndex;
            mesh->mFaces[i].mIndices[2] = lod.Faces[i].Vertices[2].VertexIndex;

            // Texcoords
            mesh->mTextureCoords[0][i] = aiVector3D(lod.Faces[i].Vertices->TexCoord.u, lod.Faces[i].Vertices->TexCoord.v, 0.0f);
        }

        for (int i = 0; i < static_cast<int32_t>(lod.VertexCount); ++i) {
            mesh->mVertices[i] = aiVector3D(lod.Vertices[i].Location.x, lod.Vertices[i].Location.y, lod.Vertices[i].Location.z);
            mesh->mNormals[i] = aiVector3D(lod.Vertices[i].Normal.x, lod.Vertices[i].Normal.y, lod.Vertices[i].Normal.z);
        }

        m_Scene->mMeshes[meshIdx] = mesh;
        meshIdx++;
    }

    m_Scene->mRootNode->mNumMeshes = m_Scene->mNumMeshes;
    m_Scene->mRootNode->mMeshes = new unsigned int[m_Scene->mNumMeshes];
    for (unsigned int i = 0; i < m_Scene->mNumMeshes; ++i) {
        m_Scene->mRootNode->mMeshes[i] = i;
    }
    return true;
}

std::string LTABCImporter::ReadLTString() {
    CheckBuffer();

    const uint16_t len = m_Buffer->GetU2();

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
