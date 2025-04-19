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
#include "assimp/Exporter.hpp"
#ifndef ASSIMP_BUILD_NO_LTABC_IMPORTER
#include "../LTShared.h"
#include "LT1ABCImporter.h"
#include "assimp/IOSystem.hpp"
#include "assimp/scene.h"
#include "assimp/types.h"

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

Assimp::LT::LT1::LT1ABCImporter::~LT1ABCImporter() {
    delete m_Buffer;
    delete m_Profiler;
    delete m_MeshHeader;
    if (m_Geometry) {
        delete m_Geometry->TriangleStartPosition;
        delete m_Geometry->Faces;
        delete m_Geometry->Vertices;
        delete m_Geometry;
    }
    for (const auto ptr : m_Nodes) {
        if (ptr->MDVertexCount) {
            delete ptr->MDVertexList;
        }
        delete ptr;
    }
    for (int i = 0; i < m_AnimationCount; ++i) {
        const auto & ptr = m_Animations[i];
        delete ptr->Keyframes;
        for (int n = 0; n < m_NodeCount; ++n) {
            delete ptr->NodeData[n].NodeTransforms;
            delete ptr->NodeData[n].VertexTransforms;
        }
        delete ptr->NodeData;
        delete ptr;
    }
}
bool Assimp::LT::LT1::LT1ABCImporter::CanRead(const std::string &filename, IOSystem *pIOHandler, bool checkSig) const {
    // If we have a stream handler check the file version
    if (pIOHandler) {
        StreamReaderLE pBuffer(StreamReaderLE(pIOHandler->Open(filename, "rb")));
        pBuffer.SetCurrentPos(12);

        const auto versionStr = ReadLTString(&pBuffer, 28);
        return !versionStr.empty() && versionStr == VERSION_STRING;
    }
    return false;
}
void Assimp::LT::LT1::LT1ABCImporter::ReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler) {
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
            m_MeshHeader->VersionString = ReadLTString(m_Buffer);
            m_MeshHeader->CommandString = ReadLTString(m_Buffer);
        } else if (sectionName == SECTION_GEOMETRY) {
            ltabc_assert(ReadGeometry());

        } else if (sectionName == SECTION_NODES) {
            ltabc_assert(ReadNodes());

        } else if (sectionName == SECTION_ANIMATIONS) {
            ltabc_assert(ReadAnimations());
            break;
        } else if (sectionName == SECTION_ANIM_DIMS) {
            ltabc_assert(ReadAnimationDims());
        } else if (sectionName == SECTION_TRANSFORM_INFO) {
            ltabc_assert(ReadTransformInformation());
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
bool Assimp::LT::LT1::LT1ABCImporter::ReadGeometry() {
    CheckBuffer();

    m_Geometry = new Geometry();
    m_Buffer->CopyAndAdvance(&m_Geometry->BoundsMin, sizeof(LTVector));
    m_Buffer->CopyAndAdvance(&m_Geometry->BoundsMax, sizeof(LTVector));
    m_Geometry->LodCount = m_Buffer->GetU4();

    auto vspCount = m_Geometry->LodCount + 1;
    m_Geometry->TriangleStartPosition = new uint16_t[vspCount];

    auto triStartPosSize = sizeof(uint16_t) * vspCount;
    m_Buffer->CopyAndAdvance(m_Geometry->TriangleStartPosition, triStartPosSize);
    m_Geometry->FaceCount = m_Buffer->GetU4();
    m_Geometry->Faces = new FaceVertex[m_Geometry->FaceCount];

    auto faceSize = sizeof(FaceVertex) * m_Geometry->FaceCount;
    m_Buffer->CopyAndAdvance(m_Geometry->Faces, faceSize);

    m_Geometry->VertexCount = m_Buffer->GetU4();
    m_Geometry->LOD0VertexCount = m_Buffer->GetU4();
    // Let's just load in everything right now
    m_Geometry->Vertices = new Vertex[m_Geometry->VertexCount];
    for (auto i = 0; i < static_cast<int>(m_Geometry->VertexCount); i++) {
        m_Buffer->CopyAndAdvance(&m_Geometry->Vertices[i].Location, sizeof(LTVector));
        m_Buffer->CopyAndAdvance(&m_Geometry->Vertices[i].Normal, sizeof(LTByteVector));
        m_Geometry->Vertices[i].NodeIndex = m_Buffer->GetU1();
        m_Geometry->Vertices[i].VertexReplacements[0] = m_Buffer->GetU2();
        m_Geometry->Vertices[i].VertexReplacements[0] = m_Buffer->GetU2();

        m_NodeCount = std::max(m_NodeCount, static_cast<int>(m_Geometry->Vertices[i].NodeIndex));
    }
    return true;
}
bool Assimp::LT::LT1::LT1ABCImporter::ReadNodes() {
    CheckBuffer();

    m_Nodes = {};
    std::vector<int> childStack = {};

    // We want an array size not index, so add 1.
    m_NodeCount += 1;

    // reverse an estimated amount of nodes
    childStack.reserve(m_NodeCount);
    m_Nodes.reserve(m_NodeCount);

    childStack.push_back(-1);
    while (!childStack.empty()) {
        const auto parentNodeIndex = childStack.back();
        childStack.pop_back();
        auto *node = new Node();

        m_Buffer->CopyAndAdvance(&node->BoundsMin, sizeof(LTVector));
        m_Buffer->CopyAndAdvance(&node->BoundsMax, sizeof(LTVector));
        auto name = ReadLTString(m_Buffer);
        node->Index = m_Buffer->GetU2();
        node->Flags = m_Buffer->GetU1();

        if (node->Flags & FLAG_DEFORMATION) {
            node->Name = "d_" + name;
        } else {
            node->Name = name;
        }

        node->MDVertexCount = m_Buffer->GetU4();
        if (node->MDVertexCount) {
            node->MDVertexList = new uint16_t[node->MDVertexCount];
            m_Buffer->CopyAndAdvance(node->MDVertexList, sizeof(uint16_t) * node->MDVertexCount);
        }
        node->ChildCount = m_Buffer->GetU4();

        if (parentNodeIndex > -1) {
            ai_assert(parentNodeIndex < static_cast<int>(m_Nodes.size()));
            node->Parent = m_Nodes[parentNodeIndex];
        }

        childStack.insert(childStack.end(), node->ChildCount, node->Index);
        m_Nodes.push_back(node);
    }

    printf("Did we match our node count? (Est vs Actual) %d == %lu\n", m_NodeCount, m_Nodes.size());
    m_NodeCount = static_cast<int>(m_Nodes.size());

    return true;
}
bool Assimp::LT::LT1::LT1ABCImporter::ReadAnimations() {
    CheckBuffer();

    m_AnimationCount = static_cast<int>(m_Buffer->GetU4());
    m_Animations = new Animation *[m_AnimationCount];
    for (int i = 0; i < m_AnimationCount; ++i) {
        auto animation = new Animation();
        animation->Name = ReadLTString(m_Buffer);
        animation->Length = m_Buffer->GetU4();
        m_Buffer->CopyAndAdvance(&animation->BoundsMin, sizeof(LTVector));
        m_Buffer->CopyAndAdvance(&animation->BoundsMax, sizeof(LTVector));
        animation->KeyframeCount = m_Buffer->GetU4();
        animation->Keyframes = new Keyframe[animation->KeyframeCount];
        for (auto kf = 0; kf < static_cast<int>(animation->KeyframeCount); kf++) {
            animation->Keyframes[kf] = Keyframe();
            animation->Keyframes[kf].Time = m_Buffer->GetU4();
            m_Buffer->CopyAndAdvance(&animation->Keyframes[kf].BoundsMin, sizeof(LTVector));
            m_Buffer->CopyAndAdvance(&animation->Keyframes[kf].BoundsMax, sizeof(LTVector));
            animation->Keyframes[kf].CommandString = ReadLTString(m_Buffer);
        }

        animation->NodeData = new AnimationNodeData[m_NodeCount];
        for (auto n = 0; n < m_NodeCount; n++) {
            const auto& node = m_Nodes[n];
            animation->NodeData[n].NodeTransforms = new KeyframeTransform[animation->KeyframeCount];
            m_Buffer->CopyAndAdvance(animation->NodeData[n].NodeTransforms, sizeof(KeyframeTransform) * animation->KeyframeCount);
            if (node->MDVertexCount) {
                animation->NodeData[n].VertexTransforms = new VertexTransform[animation->KeyframeCount * node->MDVertexCount];
                m_Buffer->CopyAndAdvance(animation->NodeData[n].VertexTransforms, sizeof(VertexTransform) * animation->KeyframeCount * node->MDVertexCount);

            }
            m_Buffer->CopyAndAdvance(&animation->NodeData[n].Scale, sizeof(LTVector));
            m_Buffer->CopyAndAdvance(&animation->NodeData[n].Origin, sizeof(LTVector));

            if (i == 0) {
                const auto& transform = animation->NodeData[n].NodeTransforms[0];
                auto rot = LTRotation2aiQuaternion(transform.Rotation).Conjugate();
                auto loc = LTVector2aiVector(transform.Location);
                auto scale = LTVector2aiVector(animation->NodeData[n].Scale);
                auto origin = LTVector2aiVector(animation->NodeData[n].Origin);

                // We need to ensure we cancel out scale / origin from parent
                if (node->Parent) {
                    auto np = node->Parent->Index;
                    const auto& parentNodeData = animation->NodeData[np];
                    scale = aiVector3f(1.0f) - (LTVector2aiVector(parentNodeData.Scale) - scale);
                    origin = (LTVector2aiVector(parentNodeData.Origin) - origin);
                }

                auto matrix = aiMatrix4x4(scale, rot, origin + loc);

                node->BindMatrix = node->InvBindMatrix = matrix;
                node->InvBindMatrix.Inverse();
            }
        }
        m_Animations[i] = animation;
    }

    return true;
}
bool Assimp::LT::LT1::LT1ABCImporter::ReadAnimationDims() {
    return false;
}
bool Assimp::LT::LT1::LT1ABCImporter::ReadTransformInformation() {
    return false;
}
bool Assimp::LT::LT1::LT1ABCImporter::BuildMesh() const {

    m_Scene->mNumMeshes = 1;
    m_Scene->mMeshes = new aiMesh *[m_Scene->mNumMeshes];

    struct BoneData {
        Node *LTNode = nullptr;
        aiNode *boneNode{};
    };
    struct VertData {
        aiVector3D verts = {};
        aiVector3D normals = {};
        aiVector3D uvs = {};
        int nodeIndex = 0;
        int faceIndex = 0;
        int faceIndexIndex = 0;
    };

    auto headerRoot = new aiNode("<Header Root>");
    auto pieceRoot = new aiNode("<Piece Root>");
    auto nodeRoot = new aiNode("<Node Root>");

    std::map<int, aiMaterial *> materials;
    std::vector<aiNode *> boneNodes;
    std::vector<BoneData> bones;
    std::vector<int> childStack;

    // Create a place to store any header-related metadata
    headerRoot->mMetaData = aiMetadata::Alloc(2);
    headerRoot->mMetaData->Set(0, "version", static_cast<uint64_t>(m_MeshVersion));
    headerRoot->mMetaData->Set(1, "command_string", aiString(m_MeshHeader->CommandString));

    m_Scene->mRootNode->addChildren(1, &headerRoot);
    m_Scene->mRootNode->addChildren(1, &pieceRoot);
    pieceRoot->mNumMeshes = 1;
    pieceRoot->mMeshes = new unsigned int[1];
    pieceRoot->mParent = m_Scene->mRootNode;

    LTABC_PERF_BEGIN("Building Nodes");
    for (const auto &node : m_Nodes) {
        auto *boneNode = new aiNode(node->Name);
        boneNode->mTransformation = node->BindMatrix;

        // Set our parent, this is also where we'll be undoing the absolute positioning
        if (node->Parent) {
            auto parentNode = boneNodes[node->Parent->Index];
            parentNode->addChildren(1, &boneNode);
            boneNode->mParent = parentNode;

            // FIXME: There is no actual bind_rest pose!
            //auto parentInv = node->Parent->InvBindMatrix;
            //boneNode->mTransformation = parentInv * boneNode->mTransformation;
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

    m_Scene->mRootNode->addChildren(1, &nodeRoot);
    nodeRoot->mParent = m_Scene->mRootNode;

    LTABC_PERF_BEGIN("Building Mesh");
    // 1 for now
    // This needs to be split up per lod
    {
        auto mesh = new aiMesh();
        // auto lodIdx = 0;

        std::string name = "default";

        // If we're a LOD then tweak the name to say so.
        // if (lodIdx > 0) {
        //    char nameBuffer[256];
        //    std::sprintf(nameBuffer, "LOD_%d_%s", lodIdx, "default");
        //    name = nameBuffer;
        //}
        mesh->mName = name;
        mesh->mPrimitiveTypes = aiPrimitiveType_TRIANGLE;



#if 1
        mesh->mNumFaces = m_Geometry->FaceCount;
        mesh->mNumVertices = m_Geometry->VertexCount;
        mesh->mNumUVComponents[0] = 2;

        mesh->mFaces = new aiFace[mesh->mNumFaces];
        mesh->mVertices = new aiVector3D[mesh->mNumVertices];
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];
        mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];

        std::map<int, std::vector<aiVertexWeight>> vertexWeights = {};
        std::map<int, VertData> reIndexedVertData = {};
        std::map<int, int> duplicateVerts = {};
        //auto currentVertCount = 0;


        for (auto i = 0; i < static_cast<int>(mesh->mNumVertices); i++) {
            const auto& vertex = m_Geometry->Vertices[i];
            mesh->mVertices[i] = LTVector2aiVector(vertex.Location);
            mesh->mNormals[i] = LTVector2aiVector(vertex.Normal);

            vertexWeights[vertex.NodeIndex].emplace_back(i, 1.0f);
        }
        for (auto i = 0; i < static_cast<int>(mesh->mNumFaces); i++) {
            const auto &face = m_Geometry->Faces[i];
            mesh->mFaces[i].mNumIndices = 3;
            mesh->mFaces[i].mIndices = new unsigned int[mesh->mFaces[i].mNumIndices];
            mesh->mFaces[i].mIndices[0] = face.VertexIndex.x;
            mesh->mFaces[i].mIndices[1] = face.VertexIndex.y;
            mesh->mFaces[i].mIndices[2] = face.VertexIndex.z;
        }
#else
        mesh->mNumFaces = m_Geometry->FaceCount;
        mesh->mNumVertices = m_Geometry->FaceCount * 3;
        mesh->mNumUVComponents[0] = 2;

        mesh->mFaces = new aiFace[mesh->mNumFaces];
        mesh->mVertices = new aiVector3D[mesh->mNumVertices];
        mesh->mNormals = new aiVector3D[mesh->mNumVertices];
        mesh->mTextureCoords[0] = new aiVector3D[mesh->mNumVertices];

        std::map<int, std::vector<aiVertexWeight>> vertexWeights = {};
        std::map<int, VertData> reIndexedVertData = {};
        std::map<int, int> duplicateVerts = {};
        auto currentVertCount = 0;
        for (auto i = 0; i < static_cast<int>(mesh->mNumFaces); i++) {
            const auto &face = m_Geometry->Faces[i];
            const auto& va = { face.VertexIndex.x, face.VertexIndex.y, face.VertexIndex.z };

            mesh->mFaces[i].mNumIndices = 3;
            mesh->mFaces[i].mIndices = new unsigned int[3];
            mesh->mFaces[i].mIndices[0] = 0;
            mesh->mFaces[i].mIndices[1] = 0;
            mesh->mFaces[i].mIndices[2] = 0;

            auto j = 0;
            for (const auto& vi : va) {
                auto const& vertex = m_Geometry->Vertices[vi];
                if (duplicateVerts.count(vi)) {
                    // Duplicate vertex data
                    auto newVertexIndex = static_cast<int>(reIndexedVertData.size()) + static_cast<int>(m_Geometry->VertexCount) - 1;

                    reIndexedVertData[newVertexIndex] = {
                        aiVector3D(vertex.Location.x, vertex.Location.y, vertex.Location.z),
                        aiVector3D(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z),
                        aiVector3f(face.UV[j].u, face.UV[j].v, 0.0f),
                        vertex.NodeIndex,
                        i,
                        j
                    };
                } else {
                    mesh->mFaces[i].mIndices[j] = vi;
                    mesh->mVertices[vi] = LTVector2aiVector(vertex.Location);
                    mesh->mNormals[vi] = aiVector3f(vertex.Normal.x, vertex.Normal.y, vertex.Normal.z);
                    mesh->mTextureCoords[0][vi] = aiVector3f(face.UV[j].u, face.UV[j].v, 0.0f);
                    vertexWeights[vertex.NodeIndex].emplace_back(vi, 1.0f);

                    duplicateVerts[vi] = i;

                    currentVertCount++;
                }
                j++;
            }
        }
        // Now we can insert any non-unique vertex indexes.
        for (const auto &[idx, vertexData] : reIndexedVertData) {
            mesh->mFaces[vertexData.faceIndex].mIndices[vertexData.faceIndexIndex] = currentVertCount;
            mesh->mVertices[currentVertCount] = vertexData.verts;
            mesh->mNormals[currentVertCount] = vertexData.normals;
            mesh->mTextureCoords[0][currentVertCount] = vertexData.uvs;
            vertexWeights[vertexData.nodeIndex].emplace_back(currentVertCount, 1.0f);

            currentVertCount++;
        }
        ai_assert(currentVertCount == static_cast<int>(mesh->mNumVertices));
#endif

        mesh->mNumBones = bones.size();
        mesh->mBones = new aiBone *[mesh->mNumBones];

        // Create our mesh bones
        for (auto idx = 0; idx < static_cast<int>(bones.size()); idx++) {
            auto bone = new aiBone();
            const auto &boneData = bones[idx];
            const auto offsetMatrix = aiMatrix4x4();

            // Not in the weight list? Create an empty bone instead.
            if (!vertexWeights.count(idx)) {
                bone->mName = boneData.LTNode->Name;
                bone->mWeights = nullptr;
                bone->mOffsetMatrix = offsetMatrix;
                bone->mNumWeights = 0;
                mesh->mBones[idx] = bone;
                continue;
            }

            const auto &weightList = vertexWeights[idx];
            bone->mNode = boneData.boneNode;
            bone->mName = boneData.LTNode->Name;
            bone->mOffsetMatrix = offsetMatrix;
            bone->mNumWeights = weightList.size();
            bone->mWeights = new aiVertexWeight[bone->mNumWeights];
            std::copy(weightList.begin(), weightList.end(), bone->mWeights);
            mesh->mBones[idx] = bone;
        }


        pieceRoot->mMeshes[0] = 0;
        m_Scene->mMeshes[0] = mesh;
    }
    LTABC_PERF_END("Building Mesh");

    LTABC_PERF_BEGIN("Building Animations");
    m_Scene->mNumAnimations = m_AnimationCount;
    m_Scene->mAnimations = new aiAnimation *[m_Scene->mNumAnimations];
    for (auto i = 0; i < static_cast<int>(m_Scene->mNumAnimations); i++) {
        const auto& ltAnim = m_Animations[i];
        const auto& anim = new aiAnimation();

        anim->mName = ltAnim->Name;
        anim->mDuration = ltAnim->Length;
        anim->mTicksPerSecond = 1000;
        anim->mNumChannels = m_NodeCount;
        anim->mChannels = new aiNodeAnim *[anim->mNumChannels];

        for (int n = 0; n < static_cast<int>(anim->mNumChannels); ++n) {
            const auto &channel = new aiNodeAnim();
            const auto &node = bones[n];

            channel->mNodeName = node.boneNode->mName;
            channel->mNumPositionKeys = ltAnim->KeyframeCount;
            channel->mNumRotationKeys = ltAnim->KeyframeCount;
            channel->mPositionKeys = new aiVectorKey[channel->mNumPositionKeys];
            channel->mRotationKeys = new aiQuatKey[channel->mNumRotationKeys];

            // Complete our requirement of needing a scale key
            channel->mNumScalingKeys = 1;
            channel->mScalingKeys = new aiVectorKey[channel->mNumScalingKeys];
            channel->mScalingKeys[0].mTime = 0.0;
            channel->mScalingKeys[0].mValue = aiVector3f(1.0f);

            for (int kf = 0; kf < static_cast<int>(ltAnim->KeyframeCount); ++kf) {
                const auto &transform = ltAnim->NodeData[n].NodeTransforms[kf];
                const auto &ltKey = ltAnim->Keyframes[kf];

                auto pos = LT::LTVector2aiVector(transform.Location);
                auto rot = LT::LTRotation2aiQuaternion(transform.Rotation);

                auto &rotKey = channel->mRotationKeys[kf];
                rotKey.mTime = ltKey.Time;
                rotKey.mInterpolation = aiAnimInterpolation_Linear;
                rotKey.mValue = rot;

                auto &posKey = channel->mPositionKeys[kf];
                posKey.mTime = ltKey.Time;
                posKey.mInterpolation = aiAnimInterpolation_Linear;
                posKey.mValue = pos;

            }
            anim->mChannels[n] = channel;
        }
        m_Scene->mAnimations[i] = anim;
    }
    LTABC_PERF_END("Building Animations");

    return true;
}

#endif
