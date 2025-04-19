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
/** @file  LT1ABCImporter.h
 *  @brief Definition of the Lithtech Engine's ABC file format
 */
#ifndef ASSIMP_BUILD_NO_LTABC_IMPORTER

#pragma once
#ifndef LT1ABCIMPORTER_H
#define LT1ABCIMPORTER_H

#include "LT1ABC.h"
#include <assimp/BaseImporter.h>
#include <assimp/ParsingUtils.h>
#include <assimp/Profiler.h>
#include <assimp/StreamReader.h>

struct aiNode;

namespace Assimp::LT::LT1 {
class LT1ABCImporter {
public:
    LT1ABCImporter() :
            m_Buffer(nullptr),
            m_Profiler(nullptr),
            m_Scene(nullptr),
            m_MeshHeader(nullptr),
            m_Geometry(nullptr),
            m_NodeCount(0),
            m_AnimationCount(0),
            m_Animations(nullptr),
            m_MeshVersion(0) {};
    ~LT1ABCImporter();

    bool CanRead(const std::string &filename, IOSystem *pIOHandler, bool checkSig) const;
    void ReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler);

protected:
    /**
     * Reads in the `SECTION_PIECES` into m_PieceHeader
     * @return true if success
     */
    bool ReadGeometry();
    bool ReadNodes();
    bool ReadAnimations();
    bool ReadAnimationDims();
    bool ReadTransformInformation();

    /**
     * Takes various LTABC structs and constructs an assimp mesh
     * @return true if success
     */
    bool BuildMesh() const;

    /**
     * Checks buffer against itself (for null), and the offset vs filesize.
     * Returns true if you can use buffer else false.
     * @return bool
     */
    void CheckBuffer() const { ai_assert(m_Buffer != nullptr); }

private:
    StreamReaderLE *m_Buffer;
    Profiling::Profiler *m_Profiler;
    aiScene *m_Scene;

    Header *m_MeshHeader;
    Geometry *m_Geometry;
    std::vector<Node *> m_Nodes; // Vector since we don't know exact node size until we read it!
    int m_NodeCount; // Estimated count until it's all read
    int m_AnimationCount;
    Animation **m_Animations;


    int32_t m_MeshVersion;
};
} // namespace Assimp::LT::LT1

#endif // LT1ABCIMPORTER_H
#endif