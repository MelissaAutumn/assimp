/*
Open Asset Import Library (assimp)
----------------------------------------------------------------------

Copyright (c) 2006-2025, assimp team

All rights reserved.

Redistribution and use of this software in source and binary forms,
with or without modification, are permitted provided that the
following conditions are met:

* Redistributions of source code must retain the above
copyright notice, this list of conditions and the
following disclaimer.

* Redistributions in binary form must reproduce the above
copyright notice, this list of conditions and the
following disclaimer in the documentation and/or other
materials provided with the distribution.

* Neither the name of the assimp team, nor the names of its
contributors may be used to endorse or promote products
derived from this software without specific prior
written permission of the assimp team.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
"AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT
LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE,
DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.

----------------------------------------------------------------------
*/
#ifndef ASSIMP_BUILD_NO_LTABC_IMPORTER
#include "LT2ABCImporter.h"
#include "assimp/Exporter.hpp"
#include "assimp/IOSystem.hpp"
#include "assimp/Profiler.h"
#include "assimp/scene.h"

#define LTABC_TESTING
#define LTABC_RESERVE_VECTORS
#define LTABC_PROFILE

#ifdef LTABC_PROFILE
#define LTABC_PERF_BEGIN(name) m_Profiler->BeginRegion(name)
#define LTABC_PERF_END(name) m_Profiler->EndRegion(name)
#else
#define LTABC_PERF_BEGIN(name)
#define LTABC_PERF_END(name)
#endif

#ifdef _DEBUG
#define ltabc_assert(expr) ai_assert(expr)
#else
#define ltabc_assert(expr) expr
#endif

namespace Assimp::LT::LT2 {

LT2ABCImporter::~LT2ABCImporter() {
    delete m_Buffer;
    delete m_MeshHeader;
    if (m_PieceHeader) {
        for (const auto &ptr : m_PieceHeader->Pieces) {
            for (const auto &lodPtr : ptr.LODs) {
                delete lodPtr.Faces;
            }
        }
    }
    delete m_PieceHeader;
    delete m_Profiler;
    for (const auto ptr : m_Nodes) {
        delete ptr;
    }
    for (const auto ptr : m_WeightSets) {
        delete ptr;
    }
    for (const auto ptr : m_ChildModels) {
        delete ptr;
    }
    for (const auto ptr : m_Animations) {
        for (const auto transformsPtr : ptr->Transforms) {
            delete transformsPtr;
        }
        delete ptr;
    }
    for (const auto ptr : m_Sockets) {
        delete ptr;
    }
    for (const auto ptr : m_AnimationBindings) {
        delete ptr;
    }
    for (const auto ptr : m_ChildModelAnimationBindings) {
        delete ptr;
    }
    m_FileSize = 0;
}

bool LT2ABCImporter::CanRead(const std::string &pFile, IOSystem *pIOHandler, bool) const {
    // If we have a stream handler check the file version
    if (pIOHandler) {
        std::unique_ptr<IOStream> pStream(pIOHandler->Open(pFile, "rb"));
        pStream->Seek(12, aiOrigin_SET);

        uint32_t meshVersion = 0;
        pStream->Read(&meshVersion, sizeof(meshVersion), 1);

        // Version not supported!
        if (meshVersion < 9 || meshVersion > 13) {
            return false;
        }
        return true;
    }
    return false;
}

void LT2ABCImporter::ReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler) {
#ifdef LTABC_PROFILE
    m_Profiler = new Profiling::Profiler();
#endif
    LTABC_PERF_BEGIN("InternReadFile");

    m_Buffer = new StreamReaderLE(pIOHandler->Open(pFile, "rb"));

    // Check whether we can read from the file
    if (m_Buffer == nullptr) {
        throw DeadlyImportError("Failed to open ABC file ", pFile, ".");
    }

    m_Scene = pScene;
    m_Scene->mRootNode = new aiNode("<ABC_Root>");

    m_MeshHeader = new Header;

    int64_t nextSectionOffset = 0;
    while (nextSectionOffset != -1) {
        m_Buffer->SetCurrentPos(nextSectionOffset);

        auto sectionName = ReadLTString(m_Buffer);
        nextSectionOffset = m_Buffer->GetI4();

        LTABC_PERF_BEGIN(sectionName);

        if (sectionName == SECTION_HEADER) {
            auto ioHeader = m_Buffer->Get<IOHeader>();
            uint32_t unkV13Value = 0;
            m_MeshVersion = ioHeader.Version;

            if (m_MeshVersion == 13) {
                unkV13Value = m_Buffer->GetU4();
            }

            auto commandString = ReadLTString(m_Buffer);
            auto internalRadius = m_Buffer->GetF4();
            auto lodDistanceCount = m_Buffer->GetU4();
            m_Buffer->IncPtr(60); // Skip padding

            for (int i = 0; i < static_cast<int>(lodDistanceCount); ++i) {
                m_MeshHeader->LODDistances.push_back(m_Buffer->GetF4());
            }

            // lodDistance shouldn't count main LOD, if it does we need to make some code tweaks.
            ai_assert(lodDistanceCount == ioHeader.LODCount - 1);

            // Load-up the header
            m_MeshHeader->LoadHeader(ioHeader, commandString, internalRadius, lodDistanceCount, unkV13Value);
        } else if (sectionName == SECTION_PIECES) {
            ltabc_assert(ReadPieces());
        } else if (sectionName == SECTION_NODES) {
            ltabc_assert(ReadNodes());
            ltabc_assert(ReadWeightSets());
        } else if (sectionName == SECTION_CHILD_MODELS) {
            ltabc_assert(ReadChildModels());
        } else if (sectionName == SECTION_ANIMATIONS) {
            ltabc_assert(ReadAnimations());
        } else if (sectionName == SECTION_SOCKETS) {
            ltabc_assert(ReadSockets());
        } else if (sectionName == SECTION_ANIM_BINDINGS) {
            ltabc_assert(ReadAnimationBindings());
        }

        LTABC_PERF_END(sectionName);
    }

    LTABC_PERF_BEGIN("BuildMesh");

    // Build the mesh and all
    ltabc_assert(BuildMesh());

    LTABC_PERF_END("BuildMesh");
    LTABC_PERF_END("InternReadFile");
#ifdef LTABC_TESTING
    std::string out = std::string(pFile + ".gltf");
    ::Assimp::Exporter exporter;
    exporter.Export(m_Scene, "gltf2", out);
    exit(0);
#endif
}

bool LT2ABCImporter::ReadPieces() {
    CheckBuffer();

    m_PieceHeader = new PieceHeader();
    m_PieceHeader->WeightCount = m_Buffer->GetI4();
    m_PieceHeader->PieceCount = m_Buffer->GetI4();

#ifdef LTABC_RESERVE_VECTORS
    m_PieceHeader->Pieces.reserve(m_PieceHeader->PieceCount);
#endif
    for (auto i = 0; i < static_cast<int32_t>(m_PieceHeader->PieceCount); ++i) {
        Piece piece = {};
        piece.MaterialIndex = m_Buffer->GetU2();
        piece.SpecularPower = m_Buffer->GetF4();
        piece.SpecularScale = m_Buffer->GetF4();
        piece.LODWeight = m_MeshVersion > 9 ? m_Buffer->GetF4() : 0.0f;
        piece.Unknown = m_Buffer->GetU2();
        piece.Name = ReadLTString(m_Buffer);

#ifdef LTABC_RESERVE_VECTORS
        piece.LODs.reserve(m_MeshHeader->LODCount);
#endif
        for (auto l = 0; l < static_cast<int32_t>(m_MeshHeader->LODCount); ++l) {
            LOD lod = {};

            lod.FaceCount = m_Buffer->GetU4();

#ifdef WITH_NO_PADDING_SUPPORTED
            lod.Faces = new Face[lod.FaceCount];
            m_Buffer->CopyAndAdvance(lod.Faces, sizeof(Face) * lod.FaceCount);
#else
            for (auto f = 0; f < static_cast<int32_t>(lod.FaceCount); ++f) {
                Face face = {};
                // Struct is padded to 12 bytes, but this is tightly packed.
                m_Buffer->CopyAndAdvance(&face.Vertices[0], /*sizeof(FaceVertex)*/ 10);
                m_Buffer->CopyAndAdvance(&face.Vertices[1], /*sizeof(FaceVertex)*/ 10);
                m_Buffer->CopyAndAdvance(&face.Vertices[2], /*sizeof(FaceVertex)*/ 10);
                lod.Faces[f] = face;
            }
#endif

            lod.VertexCount = m_Buffer->GetU4();
#ifdef LTABC_RESERVE_VECTORS
            lod.Vertices.reserve(lod.VertexCount);
#endif
            for (auto v = 0; v < static_cast<int32_t>(lod.VertexCount); ++v) {
                Vertex vertex = {};

                vertex.WeightCount = m_Buffer->GetU2();
                vertex.SubLODVertexIndex = m_Buffer->GetU2();

#ifdef LTABC_RESERVE_VECTORS
                vertex.Weights.reserve(vertex.WeightCount);
#endif
                for (auto w = 0; w < static_cast<int32_t>(vertex.WeightCount); w++) {
                    Weight weight = {};
                    m_Buffer->CopyAndAdvance(&weight, sizeof(Weight));
                    vertex.Weights.push_back(weight);
                }

                m_Buffer->CopyAndAdvance(&vertex.Location, sizeof(LT::LTVector));
                m_Buffer->CopyAndAdvance(&vertex.Normal, sizeof(LT::LTVector));

                lod.Vertices.push_back(vertex);
            }

            piece.LODs.push_back(lod);
        }

        m_PieceHeader->Pieces.push_back(piece);
    }

    return true;
}
bool LT2ABCImporter::ReadNodes() {
    CheckBuffer();

    // A mesh shouldn't have 0 nodes!
    if (m_MeshHeader->NodeCount == 0) {
        return false;
    }

    // Read in the list of nodes and while we're doing that construct links to parent->child and a few more handy things
    std::vector<int> childStack = {};
#ifdef LTABC_RESERVE_VECTORS
    childStack.reserve(m_MeshHeader->NodeCount);
#endif
    for (auto i = 0; i < static_cast<int>(m_MeshHeader->NodeCount); i++) {
        auto *node = new Node();
        LT::LTMatrix bindMatrix = {};

        node->Name = ReadLTString(m_Buffer);
        node->Index = m_Buffer->GetU2();
        node->Flags = m_Buffer->GetU1();
        m_Buffer->CopyAndAdvance(&bindMatrix, sizeof(LT::LTMatrix));
        node->BindMatrix = node->InvBindMatrix = LT::LTMatrix2aiMatrix(bindMatrix);
        node->InvBindMatrix.Inverse();
        node->ChildCount = m_Buffer->GetU4();

        if (!childStack.empty()) {
            const auto &parentNode = m_Nodes[childStack.back()];
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
bool LT2ABCImporter::ReadWeightSets() {
    CheckBuffer();

    uint32_t weightSetCount = m_Buffer->GetU4();
#ifdef LTABC_RESERVE_VECTORS
    m_WeightSets.reserve(weightSetCount);
#endif
    for (int i = 0; i < static_cast<int>(weightSetCount); ++i) {
        auto weightSet = new WeightSet();
        weightSet->Name = ReadLTString(m_Buffer);
        weightSet->NodeCount = m_Buffer->GetU4();
        for (int n = 0; n < static_cast<int>(weightSet->NodeCount); ++n) {
            weightSet->NodeWeights.push_back(m_Buffer->GetF4());
        }
        m_WeightSets.push_back(weightSet);
    }

    return true;
}
bool LT2ABCImporter::ReadChildModels() {
    CheckBuffer();

    uint16_t childCount = m_Buffer->GetU2();
#ifdef LTABC_RESERVE_VECTORS
    m_ChildModels.reserve(childCount);
#endif
    for (int i = 0; i < static_cast<int>(childCount); ++i) {
        auto childModel = new ChildModel();
        childModel->Name = ReadLTString(m_Buffer); // Will be blank for "self"
        childModel->BuildNumber = m_Buffer->GetU4();
        for (int n = 0; n < static_cast<int>(m_MeshHeader->NodeCount); ++n) {
            auto trans = LT::Transform();
            m_Buffer->CopyAndAdvance(&trans.Location, sizeof(trans.Location));
            m_Buffer->CopyAndAdvance(&trans.Rotation, sizeof(trans.Rotation));
            childModel->Transforms.push_back(trans);
        }
        m_ChildModels.push_back(childModel);
    }

    return true;
}
bool LT2ABCImporter::ReadAnimations() {
    CheckBuffer();

    uint32_t animCount = m_Buffer->GetU4();
#ifdef LTABC_RESERVE_VECTORS
    m_Animations.reserve(animCount);
#endif
    for (int i = 0; i < static_cast<int>(animCount); ++i) {
        auto animation = new Animation();
        m_Buffer->CopyAndAdvance(&animation->Extents, sizeof(animation->Extents));
        animation->Name = ReadLTString(m_Buffer);
        animation->UnkInt = m_MeshVersion > 9 ? m_Buffer->GetU4() : 0;
        animation->InterpolationTime = m_MeshVersion > 10 ? m_Buffer->GetU4() : 200;
        animation->KeyFrameCount = m_Buffer->GetU4();
        for (int k = 0; k < static_cast<int>(animation->KeyFrameCount); ++k) {
            auto keyframe = KeyFrame();
            keyframe.Time = m_Buffer->GetU4();
            keyframe.Command = ReadLTString(m_Buffer);
            animation->KeyFrames.push_back(keyframe);
        }
        for (int n = 0; n < static_cast<int>(m_MeshHeader->NodeCount); ++n) {
            // A slightly different animation transform structure makes this slightly annoying
            if (m_MeshVersion <= 12) {
                auto transforms = new AnimTransform[animation->KeyFrameCount];
                m_Buffer->CopyAndAdvance(transforms, sizeof(AnimTransform) * animation->KeyFrameCount);
                animation->Transforms.push_back(transforms);
            } else { // v13
                // Skip unk int
                m_Buffer->SetCurrentPos(m_Buffer->GetCurrentPos() + 4);

                auto transforms = new AnimTransformV13[animation->KeyFrameCount];
                m_Buffer->CopyAndAdvance(transforms, sizeof(AnimTransformV13) * animation->KeyFrameCount);
                animation->Transforms.push_back(transforms);
            }
        }
        m_Animations.push_back(animation);
    }

    return true;
}
bool LT2ABCImporter::ReadSockets() {
    CheckBuffer();

    uint32_t socketCount = m_Buffer->GetU4();
#ifdef LTABC_RESERVE_VECTORS
    m_Sockets.reserve(socketCount);
#endif
    for (int i = 0; i < static_cast<int>(socketCount); ++i) {
        auto socket = new Socket();
        socket->NodeIndex = m_Buffer->GetU4();
        socket->Name = ReadLTString(m_Buffer);
        // Yes these are flipped compared to every other transform :shrug:
        m_Buffer->CopyAndAdvance(&socket->Rotation, sizeof(socket->Rotation));
        m_Buffer->CopyAndAdvance(&socket->Location, sizeof(socket->Location));
        m_Sockets.push_back(socket);
    }

    return true;
}
bool LT2ABCImporter::ReadAnimationBindings() {
    CheckBuffer();

    uint32_t animBindingCount = m_Buffer->GetU4();
#ifdef LTABC_RESERVE_VECTORS
    m_AnimationBindings.reserve(animBindingCount);
#endif
    for (int i = 0; i < static_cast<int>(animBindingCount); ++i) {
        auto animBinding = new AnimBinding();
        animBinding->Name = ReadLTString(m_Buffer);
        m_Buffer->CopyAndAdvance(&animBinding->Extents, sizeof(animBinding->Extents));
        m_Buffer->CopyAndAdvance(&animBinding->Origin, sizeof(animBinding->Origin));
        m_AnimationBindings.push_back(animBinding);
    }

    // TODO: Clean this up
    if (m_MeshHeader->ChildModelCount > 1) {
        uint32_t childAnimBindingCount = m_Buffer->GetU4();
#ifdef LTABC_RESERVE_VECTORS
        m_ChildModelAnimationBindings.reserve(childAnimBindingCount);
#endif
        for (int i = 0; i < static_cast<int>(childAnimBindingCount); ++i) {
            auto animBinding = new AnimBinding();
            animBinding->Name = ReadLTString(m_Buffer);
            m_Buffer->CopyAndAdvance(&animBinding->Extents, sizeof(animBinding->Extents));
            m_Buffer->CopyAndAdvance(&animBinding->Origin, sizeof(animBinding->Origin));
            m_ChildModelAnimationBindings.push_back(animBinding);
        }
    }

    return true;
}

bool LT2ABCImporter::BuildMesh() const {
    m_Scene->mNumMeshes = m_PieceHeader->PieceCount * m_MeshHeader->LODCount;
    m_Scene->mMeshes = new aiMesh *[m_Scene->mNumMeshes];

    struct BoneData {
        Node *LTNode = nullptr;
        aiNode *boneNode{};
    };
    struct VertData {
        aiVector3D verts = {};
        aiVector3D normals = {};
        aiVector3D uvs = {};
    };

    auto headerRoot = new aiNode("<Header Root>");
    auto pieceRoot = new aiNode("<Piece Root>");
    auto nodeRoot = new aiNode("<Node Root>");
    auto weightSetRoot = new aiNode("<WeightSet Root>");

    auto meshIdx = 0;

    std::map<int, aiMaterial *> materials;
    std::vector<aiNode *> boneNodes;
    std::vector<BoneData> bones;
    std::vector<int> childStack;

    // Create a place to store any header-related metadata
    headerRoot->mMetaData = aiMetadata::Alloc(3);
    headerRoot->mMetaData->Set(0, "version", static_cast<uint64_t>(m_MeshHeader->Version));
    headerRoot->mMetaData->Set(1, "command_string", aiString(m_MeshHeader->CommandString));
    headerRoot->mMetaData->Set(2, "internal_radius", m_MeshHeader->InternalRadius);

    m_Scene->mRootNode->addChildren(1, &headerRoot);

    LTABC_PERF_BEGIN("Building Nodes");
    for (const auto &node : m_Nodes) {
        auto *boneNode = new aiNode(node->Name);
        boneNode->mTransformation = node->BindMatrix;

        // Set our parent, this is also where we'll be undoing the absolute positioning
        if (node->Parent) {
            auto parentNode = boneNodes[node->Parent->Index];
            parentNode->addChildren(1, &boneNode);
            boneNode->mParent = parentNode;

            // FIXME: Not sure if this is needed
            auto parentInv = node->Parent->InvBindMatrix;
            boneNode->mTransformation = parentInv * boneNode->mTransformation;
        }

        // If we have no bone nodes, then set our parent to the bone root
        if (boneNodes.empty()) {
            nodeRoot->addChildren(1, &boneNode);
            boneNode->mParent = nodeRoot;
        }

        boneNode->mMetaData = aiMetadata::Alloc(3);
        boneNode->mMetaData->Set(0, "index", static_cast<uint64_t>(node->Index));
        boneNode->mMetaData->Set(1, "flags", static_cast<uint64_t>(node->Flags));
        boneNode->mMetaData->Set(2, "child_count", static_cast<uint64_t>(node->ChildCount));

        auto tmp = BoneData();
        tmp.boneNode = boneNode;
        tmp.LTNode = node;
        bones.push_back(tmp);
        boneNodes.push_back(boneNode);
    }
    LTABC_PERF_END("Building Nodes");

    LTABC_PERF_BEGIN("Building Sockets");
    for (const auto &socket : m_Sockets) {
        ai_assert(socket->NodeIndex < boneNodes.size());

        auto socketNode = new aiNode();
        const auto boneNode = boneNodes[socket->NodeIndex];

        socketNode->mName = socket->Name;
        socketNode->mTransformation = aiMatrix4x4(aiVector3d(1.0f), LT::LTRotation2aiQuaternion(socket->Rotation), LT::LTVector2aiVector(socket->Location));

        boneNode->addChildren(1, &socketNode);
        socketNode->mParent = boneNode;
    }
    LTABC_PERF_END("Building Sockets");

    LTABC_PERF_BEGIN("Building WeightSets");
    // Mainly a metadata node, I'm not sure how to otherwise implement this within assimp.
    for (const auto &weightSet : m_WeightSets) {
        ai_assert(weightSet->NodeCount <= boneNodes.size());
        ai_assert(weightSet->Name.size() < 256);

        auto weightSetNode = new aiNode();
        weightSetNode->mName = weightSet->Name;
        weightSetNode->mMetaData = aiMetadata::Alloc(weightSet->NodeCount);
#if 0
        // This is many times faster than adding a child + metadata per weightset...
        std::ostringstream wvbStream;
        for (int i = 0; i < static_cast<int>(weightSet->NodeCount); ++i) {
            if (i == 0) {
                wvbStream << weightSet->NodeWeights[i];
            }
            wvbStream << "," << weightSet->NodeWeights[i];

        }
        weightSetNode->mMetaData->Add("weight_buffers", aiString(wvbStream.str()));
#else
        // Slower but acceptable
        for (int i = 0; i < static_cast<int>(weightSet->NodeCount); ++i) {
            char keyBuffer[11];
            std::sprintf(keyBuffer, "%d", i);
            keyBuffer[10] = '\0';
            weightSetNode->mMetaData->Set(i, keyBuffer, weightSet->NodeWeights[i]);
        }
#endif
        weightSetRoot->addChildren(1, &weightSetNode);
        weightSetNode->mParent = weightSetRoot;
    }
    LTABC_PERF_END("Building WeightSets");

    LTABC_PERF_BEGIN("Building Pieces");
    for (auto piece : m_PieceHeader->Pieces) {
        auto lodIdx = 0;
        for (const auto &lod : piece.LODs) {
            auto mesh = new aiMesh();
            std::string name = piece.Name;

            // If we're a LOD then tweak the name to say so.
            if (lodIdx > 0) {
                char nameBuffer[256];
                std::sprintf(nameBuffer, "LOD_%d_%s", lodIdx, piece.Name.c_str());
                name = nameBuffer;
            }
            auto pieceNode = new aiNode(name);

            mesh->mName = name;
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
                auto shading = aiShadingMode_Gouraud;
                auto mat = new aiMaterial();
                mat->AddProperty(&piece.SpecularScale, 1, "$mat.ltabc.specularscale");
                mat->AddProperty(&piece.SpecularPower, 1, "$mat.ltabc.specularpower");
                mat->AddProperty(&shading, 1, AI_MATKEY_SHADING_MODEL);
                materials[piece.MaterialIndex] = mat;
            }

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
                    auto texCoords = face.TexCoord;

                    // Correct uv coordinates
                    texCoords.u = 1.0f - (-texCoords.u + 1.0f);
                    texCoords.v = 1.0f - (texCoords.v);

                    if (duplicateVertData.count(face.VertexIndex)) {
                        // Duplicate vertex data
                        auto newVertexIndex = static_cast<int>(lod.VertexCount) - 1 + static_cast<int>(duplicateVertData.size()) - 1;

                        duplicateVertData[newVertexIndex] = {
                            aiVector3D(vertex.Location.x, vertex.Location.y, vertex.Location.z),
                            aiVector3D(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z),
                            aiVector3D(texCoords.u, texCoords.v, 0.0f),
                        };
                        mesh->mFaces[i].mIndices[v] = newVertexIndex;
                    } else {
                        mesh->mFaces[i].mIndices[v] = currentIndex;
                        mesh->mVertices[currentIndex] = aiVector3D(vertex.Location.x, vertex.Location.y, vertex.Location.z);
                        mesh->mNormals[currentIndex] = aiVector3D(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
                        mesh->mTextureCoords[0][currentIndex] = aiVector3D(texCoords.u, texCoords.v, 0.0f);
                        currentIndex++;
                    }

                    for (auto w = 0; w < vertex.WeightCount; w++) {
                        int nodeIndex = static_cast<int>(vertex.Weights[w].NodeIndex);
                        pieceWeights[nodeIndex].emplace_back(mesh->mFaces[i].mIndices[v], vertex.Weights[w].Bias);
                    }
                }
            }

            // Now we can insert any non-unique vertex indexes.
            for (const auto &[idx, vertexData] : duplicateVertData) {
                mesh->mVertices[currentIndex] = vertexData.verts;
                mesh->mNormals[currentIndex] = vertexData.normals;
                mesh->mTextureCoords[0][currentIndex] = vertexData.uvs;
                currentIndex++;
            }

            mesh->mNumBones = bones.size();
            mesh->mBones = new aiBone *[mesh->mNumBones];

            // Create our mesh bones
            for (auto idx = 0; idx < static_cast<int>(bones.size()); idx++) {
                auto bone = new aiBone();
                const auto &boneData = bones[idx];
                const auto offsetMatrix = boneData.LTNode->InvBindMatrix;

                // Not in the weight list? Create an empty bone instead.
                if (!pieceWeights.count(idx)) {
                    bone->mName = boneData.LTNode->Name;
                    bone->mWeights = nullptr;
                    bone->mOffsetMatrix = offsetMatrix;
                    bone->mNumWeights = 0;
                    mesh->mBones[idx] = bone;
                    continue;
                }

                const auto &weightList = pieceWeights[idx];
                bone->mNode = boneData.boneNode;
                bone->mName = boneData.LTNode->Name;
                bone->mOffsetMatrix = offsetMatrix;
                bone->mNumWeights = weightList.size();
                bone->mWeights = new aiVertexWeight[bone->mNumWeights];
                std::copy(weightList.begin(), weightList.end(), bone->mWeights);
                mesh->mBones[idx] = bone;
            }

            pieceNode->mMetaData = aiMetadata::Alloc(2);
            pieceNode->mMetaData->Set(0, "lod_index", lodIdx);

            // First lod doesn't contain a distance, but we should add one for consistency.
            if (lodIdx == 0) {
                pieceNode->mMetaData->Set(1, "lod_distance", 0.0f);
            } else {
                pieceNode->mMetaData->Set(1, "lod_distance", m_MeshHeader->LODDistances[lodIdx - 1]);
            }

            pieceNode->mMeshes = new unsigned int[1];
            pieceNode->mMeshes[0] = meshIdx;
            pieceNode->mNumMeshes = 1;
            pieceRoot->addChildren(1, &pieceNode);
            pieceNode->mParent = pieceRoot;

            m_Scene->mMeshes[meshIdx] = mesh;
            meshIdx++;
            lodIdx++;
        }
    }
    LTABC_PERF_END("Building Pieces");

    auto debugNode = new aiNode("<DEBUG>");
    m_Scene->mRootNode->addChildren(1, &debugNode);

    LTABC_PERF_BEGIN("Building Animations");
    // Add the materials we've collected to the scene
    m_Scene->mNumAnimations = m_Animations.size();
    m_Scene->mAnimations = new aiAnimation *[m_Scene->mNumAnimations];
    for (int i = 0; i < static_cast<int>(m_Scene->mNumAnimations); ++i) {
        const auto &ltAnim = m_Animations[i];
        const auto &anim = new aiAnimation();

        ai_assert(!ltAnim->KeyFrames.empty());

        anim->mName = ltAnim->Name;
        anim->mDuration = ltAnim->KeyFrames[ltAnim->KeyFrames.size() - 1].Time;
        anim->mTicksPerSecond = 1000; // LTAnim->Time is in milliseconds
        anim->mNumChannels = ltAnim->Transforms.size();
        anim->mChannels = new aiNodeAnim *[anim->mNumChannels];

        for (int n = 0; n < static_cast<int>(anim->mNumChannels); ++n) {
            const auto &channel = new aiNodeAnim();
            const auto &node = bones[n];
            auto hasPosKey = !(node.LTNode->Flags & 2);

            channel->mNodeName = node.boneNode->mName;
            channel->mNumPositionKeys = hasPosKey ? ltAnim->KeyFrameCount : 0;
            channel->mNumRotationKeys = ltAnim->KeyFrameCount;
            channel->mPositionKeys = hasPosKey ? new aiVectorKey[channel->mNumPositionKeys] : nullptr;
            channel->mRotationKeys = new aiQuatKey[channel->mNumRotationKeys];

            // Complete our requirement of needing a scale key
            channel->mNumScalingKeys = 1;
            channel->mScalingKeys = new aiVectorKey[channel->mNumScalingKeys];
            channel->mScalingKeys[0].mTime = 0.0;
            channel->mScalingKeys[0].mValue = aiVector3f(1.0f);

            for (int kf = 0; kf < static_cast<int>(ltAnim->KeyFrameCount); ++kf) {
                const auto &transform = ltAnim->Transforms[n][kf];
                const auto &ltKey = ltAnim->KeyFrames[kf];
                auto pos = hasPosKey ? LT::LTVector2aiVector(transform.transform.Location) : aiVector3f();
                auto &rotKey = channel->mRotationKeys[kf];
                auto rot = LT::LTRotation2aiQuaternion(transform.transform.Rotation);

                rotKey.mTime = ltKey.Time;
                rotKey.mInterpolation = aiAnimInterpolation_Linear;
                rotKey.mValue = rot;

                if (hasPosKey) {
                    auto &posKey = channel->mPositionKeys[kf];
                    posKey.mTime = ltKey.Time;
                    posKey.mInterpolation = aiAnimInterpolation_Linear;
                    posKey.mValue = pos;
                }
            }
            anim->mChannels[n] = channel;
        }
        m_Scene->mAnimations[i] = anim;
    }
    LTABC_PERF_END("Building Animations");

    LTABC_PERF_BEGIN("Building Materials");
    // Add the materials we've collected to the scene
    m_Scene->mMaterials = new aiMaterial *[materials.size()];
    m_Scene->mNumMaterials = materials.size();
    for (const auto &[idx, mat] : materials) {
        ai_assert(idx < static_cast<int>(m_Scene->mNumMaterials));
        m_Scene->mMaterials[idx] = mat;
    }
    LTABC_PERF_END("Building Materials");

    LTABC_PERF_BEGIN("Adding RootNodes");
    m_Scene->mRootNode->addChildren(1, &weightSetRoot);
    m_Scene->mRootNode->addChildren(1, &pieceRoot);
    m_Scene->mRootNode->addChildren(1, &nodeRoot);
    pieceRoot->mParent = m_Scene->mRootNode;
    nodeRoot->mParent = m_Scene->mRootNode;
    weightSetRoot->mParent = m_Scene->mRootNode;
    LTABC_PERF_END("Adding RootNodes");

    return true;
}

} // namespace Assimp::LT::LT2
#endif
