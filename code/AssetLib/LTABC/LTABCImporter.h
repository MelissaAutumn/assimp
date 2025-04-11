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
/** @file  LTABCImporter.h
 *  @brief Definition of the Lithtech Engine's ABC file format
 */

#pragma once
#ifndef LTABCIMPORTER_H
#define LTABCIMPORTER_H

#include "assimp/StreamReader.h"

#include <assimp/BaseImporter.h>
#include <assimp/ParsingUtils.h>
#include <assimp/anim.h>
#include <assimp/material.h>
#include <assimp/texture.h>
#include <assimp/types.h>

#include <vector>

struct aiNode;

namespace Assimp {

namespace LTABC {


struct LTString {
    short stringLength;
    char *string;
};

struct LTTexCoord {
    float u, v;
};

struct LTVector {
    float x, y, z;
};

struct LTRotation {
    float x, y, z, w;
};

struct LTMatrix {
    LTRotation m[4];
};

struct Transform {
    LTVector Location;
    LTRotation Rotation;
};

inline aiMatrix4x4 LTMatrix2aiMatrix(LTMatrix ltMat) {
    return {
        ltMat.m[0].x,
        ltMat.m[0].y,
        ltMat.m[0].z,
        ltMat.m[0].w,
        ltMat.m[1].x,
        ltMat.m[1].y,
        ltMat.m[1].z,
        ltMat.m[1].w,
        ltMat.m[2].x,
        ltMat.m[2].y,
        ltMat.m[2].z,
        ltMat.m[2].w,
        ltMat.m[3].x,
        ltMat.m[3].y,
        ltMat.m[3].z,
        ltMat.m[3].w,
    };
}


struct IOHeader {
    uint32_t Version;
    uint32_t KeyframeCount;
    uint32_t AnimationCount;
    uint32_t NodeCount;
    uint32_t PieceCount;
    uint32_t ChildModelCount;
    uint32_t FaceCount;
    uint32_t VertexCount;
    uint32_t WeightCount;
    uint32_t LODCount;
    uint32_t SocketCount;
    uint32_t WeightSetCount;
    uint32_t StringCount;
    uint32_t StringLengthTotal;
};

struct Header {
    Header() :
            Version(0),
            KeyframeCount(0),
            AnimationCount(0),
            NodeCount(0),
            PieceCount(0),
            ChildModelCount(0),
            FaceCount(0),
            VertexCount(0),
            WeightCount(0),
            LODCount(0),
            SocketCount(0),
            WeightSetCount(0),
            StringCount(0),
            StringLengthTotal(0),
            UnkV13Value(0),
            InternalRadius(0),
            LODDistanceCount(0) {};

    void LoadHeader(const IOHeader &ioHdr, const std::string &commandString, const float internalRadius, const uint32_t lodDistanceCount, const uint32_t unkV13Value) {
        Version = ioHdr.Version;
        KeyframeCount = ioHdr.KeyframeCount;
        AnimationCount = ioHdr.AnimationCount;
        NodeCount = ioHdr.NodeCount;
        PieceCount = ioHdr.PieceCount;
        ChildModelCount = ioHdr.ChildModelCount;
        FaceCount = ioHdr.FaceCount;
        VertexCount = ioHdr.VertexCount;
        WeightCount = ioHdr.WeightCount;
        LODCount = ioHdr.LODCount;
        SocketCount = ioHdr.SocketCount;
        WeightSetCount = ioHdr.WeightSetCount;
        StringCount = ioHdr.StringCount;
        StringLengthTotal = ioHdr.StringLengthTotal;
        UnkV13Value = unkV13Value;
        CommandString = commandString;
        InternalRadius = internalRadius;
        LODDistanceCount = lodDistanceCount;
    }

    uint32_t Version;
    uint32_t KeyframeCount;
    uint32_t AnimationCount;
    uint32_t NodeCount;
    uint32_t PieceCount;
    uint32_t ChildModelCount;
    uint32_t FaceCount;
    uint32_t VertexCount;
    uint32_t WeightCount;
    uint32_t LODCount;
    uint32_t SocketCount;
    uint32_t WeightSetCount;
    uint32_t StringCount;
    uint32_t StringLengthTotal;
    uint32_t UnkV13Value;
    std::string CommandString;
    float InternalRadius;
    uint32_t LODDistanceCount;
};

struct FaceVertex {
    LTTexCoord TexCoord;
    uint16_t VertexIndex;
};

struct Face {
    FaceVertex Vertices[3];
};

struct Weight {
    uint32_t NodeIndex;
    LTVector Location; // This boy, right here!
    float Bias;
};

struct Vertex {
    uint16_t WeightCount;
    uint16_t SubLODVertexIndex;
    std::vector<Weight> Weights; //[WeightCount] <optimize=false>;
    LTVector Location;
    LTVector Normal;
};

struct LOD {
    uint32_t FaceCount;
    std::vector<Face> Faces; //[FaceCount] <optimize=false>;
    uint32_t VertexCount;
    std::vector<Vertex> Vertices; //[VertexCount] <optimize=false>;
};

struct Piece { //(uint32 LODCount) {
    uint16_t MaterialIndex;
    float SpecularPower;
    float SpecularScale;
    float LODWeight;
    uint16_t Unknown;
    std::string Name;
    std::vector<LOD> LODs; //[LODCount] <optimize=false>;
};

struct PieceHeader { // (uint32 LODCount) {
    uint32_t WeightCount;
    uint32_t PieceCount;
    std::vector<Piece> Pieces; //(LODCount)[PieceCount] <optimize=false>;
};

struct IONode {
    uint16_t Index;
    uint8_t Flags;
    LTMatrix BindMatrix;
    uint32_t ChildCount;
};

struct Node {
    void LoadNode(const IONode &ioNode) {
        Index = ioNode.Index;
        Flags = ioNode.Flags;
        BindMatrix = LTMatrix2aiMatrix(ioNode.BindMatrix);
        ChildCount = ioNode.ChildCount;
    }

    // Model node types.
#define MNODE_REMOVABLE		(1<<0)	// This node can be removed.
#define MNODE_ROTATIONONLY	(1<<1)	// Only use rotation info from animation data.

    std::string Name;
    uint16_t Index;
    uint8_t Flags;
    aiMatrix4x4 BindMatrix;
    aiMatrix4x4 InvBindMatrix;
    uint32_t ChildCount;
    Node* Parent;
};



} // namespace LTABC

constexpr auto SECTION_HEADER = "Header";
constexpr auto SECTION_PIECES = "Pieces";
constexpr auto SECTION_NODES = "Nodes";
constexpr auto SECTION_CHILD_MODELS = "ChildModels";
constexpr auto SECTION_ANIMATIONS = "Animations";
constexpr auto SECTION_SOCKETS = "Sockets";
constexpr auto SECTION_ANIM_BINDINGS = "AnimBindings";

class ASSIMP_API LTABCImporter : public BaseImporter {
public:
    LTABCImporter() :
            m_FileSize(0), m_Buffer(nullptr), m_MeshVersion(0), m_Scene(nullptr), m_MeshHeader(nullptr), m_PieceHeader(nullptr) {};
    ~LTABCImporter() override;

    bool CanRead(const std::string &filename, IOSystem *pIOHandler, bool checkSig) const override;
    void SetupProperties(const Importer *pImp) override;
    const aiImporterDesc *GetInfo() const override;

protected:
    void InternReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler) override;

    /**
     * Reads in the `SECTION_PIECES` into m_PieceHeader
     * @return true if success
     */
    bool ReadPieces();
    bool ReadNodes();

    /**
     * Takes various LTABC structs and constructs an assimp mesh
     * @return true if success
     */
    bool BuildMesh() const;

    // Helpers
    std::string ReadLTString();

    /**
     * Checks buffer against itself (for null), and the offset vs filesize.
     * Returns true if you can use buffer else false.
     * @return bool
     */
    void CheckBuffer() const { ai_assert(m_Buffer != nullptr); }

private:
    size_t m_FileSize;
    StreamReaderLE *m_Buffer;
    uint32_t m_MeshVersion;
    aiScene *m_Scene;
    LTABC::Header *m_MeshHeader;
    LTABC::PieceHeader *m_PieceHeader;
    std::vector<LTABC::Node *> m_Nodes;
};

} // namespace Assimp

#endif // LTABCIMPORTER_H
