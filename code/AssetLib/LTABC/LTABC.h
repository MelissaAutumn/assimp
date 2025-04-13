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
#ifndef LTABC_H
#define LTABC_H

#include <assimp/types.h>

#include <string>
#include <vector>

namespace Assimp {
namespace LTABC {

constexpr auto SECTION_HEADER = "Header";
constexpr auto SECTION_PIECES = "Pieces";
constexpr auto SECTION_NODES = "Nodes";
constexpr auto SECTION_CHILD_MODELS = "ChildModels";
constexpr auto SECTION_ANIMATIONS = "Animations";
constexpr auto SECTION_SOCKETS = "Sockets";
constexpr auto SECTION_ANIM_BINDINGS = "AnimBindings";

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
    std::vector<float> LODDistances;
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

    std::string Name;
    uint16_t Index;
    uint8_t Flags; // 1 = Removable, 2 = Rotation Only (Animations)
    aiMatrix4x4 BindMatrix;
    aiMatrix4x4 InvBindMatrix;
    uint32_t ChildCount;
    Node *Parent;
};

struct WeightSet {
    std::string Name;
    uint32_t NodeCount;
    std::vector<float> NodeWeights;
};

struct ChildModel {
    std::string Name;
    uint32_t BuildNumber;
    std::vector<Transform> Transforms; // len == Node Count
};

struct KeyFrame {
    uint32_t Time;
    std::string Command;
};

struct Animation {
    LTVector Extents;
    std::string Name;
    uint32_t UnkInt; // v10+
    uint32_t InterpolationTime; // v11+
    uint32_t KeyFrameCount;
    std::vector<KeyFrame> KeyFrames; // len == KeyFrame Count
    std::vector<Transform> Transforms; // len == Node Count
};

struct Socket {
    uint32_t NodeIndex;
    std::string Name;
    LTRotation Rotation;
    LTVector Location;
};

struct AnimBinding {
    std::string Name;
    LTVector Extents;
    LTVector Origin;
};

} // namespace LTABC
} // namespace Assimp
#endif // LTABC_H
