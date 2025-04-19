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

#ifndef LT1ABC_H
#define LT1ABC_H

#include "AssetLib/LT/LTShared.h"
#include "assimp/types.h"
#include <string>
#include <vector>

namespace Assimp::LT::LT1 {

constexpr auto SECTION_HEADER = "Header";
constexpr auto SECTION_GEOMETRY = "Geometry";
constexpr auto SECTION_NODES = "Nodes";
constexpr auto SECTION_ANIMATIONS = "Animation";
constexpr auto SECTION_ANIM_DIMS = "AnimDims";
constexpr auto SECTION_TRANSFORM_INFO = "TransformInfo";

constexpr auto VERSION_STRING = "MonolithExport Model File v6";

constexpr auto FLAG_NULL = 1;
constexpr auto FLAG_TRIS = 2;
constexpr auto FLAG_DEFORMATION = 4;


struct Header {
    std::string VersionString;
    std::string CommandString;
};

struct Vertex {
    LTVector Location;
    LTByteVector Normals;
    uint8_t NodeIndex; // Only one weight per vertex
    uint16_t VertexReplacements[2]; // Unknown, probably for LOD swaps
} WITH_NO_PADDING;

struct FaceVertex {
    LTTexCoord UV[3];
    LTShortVector VertexIndex;
    LTByteVector Normals;
} WITH_NO_PADDING;

struct Geometry {
    LTVector BoundsMin;
    LTVector BoundsMax;
    uint32_t LodCount;
    uint16_t *TriangleStartPosition; // Length of LodCount + 1
    uint32_t FaceCount;
    FaceVertex *Faces;
    uint32_t VertexCount;
    uint32_t LOD0VertexCount;
    Vertex *Vertices;
};

struct Node {
    LTVector BoundsMin;
    LTVector BoundsMax;
    std::string Name;
    uint16_t Index;
    uint8_t Flags;
    // Vertex animation related
    uint32_t MDVertexCount;
    uint16_t *MDVertexList;
    uint32_t ChildCount;
    // Constructed after read
    Node* Parent;
    // Constructed from first animation's frame
    aiMatrix4x4 BindMatrix;
    aiMatrix4x4 InvBindMatrix;
};

struct Keyframe {
    uint32_t Time;
    LTVector BoundsMin;
    LTVector BoundsMax;
    std::string CommandString;
};

struct KeyframeTransform {
    LTVector Location;
    LTRotation Rotation;
} WITH_NO_PADDING;

struct VertexTransform {
    LTByteVector Location;
} WITH_NO_PADDING;

struct AnimationNodeData {
    KeyframeTransform *NodeTransforms; // [KeyframeCount]
    VertexTransform *VertexTransforms; // [KeyframeCount * Node.MDVertexCount]
    LTVector Scale;
    LTVector Origin;
} WITH_NO_PADDING;

struct Animation {
    std::string Name;
    uint32_t Length;
    LTVector BoundsMin;
    LTVector BoundsMax;
    uint32_t KeyframeCount;
    Keyframe *Keyframes;
    AnimationNodeData *NodeData; // [NodeCount]
};

struct AnimationDims {
    LTVector *Dims;
};

struct TransformInfo {
    uint32_t FlipGeometry;
    uint32_t FlipAnimations;
};

} // namespace Assimp::LT::LT1
#endif // LT1ABC_H

#endif