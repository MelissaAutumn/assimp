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
#ifndef ASSIMP_BUILD_NO_LTABC_IMPORTER

#pragma once
#ifndef LTABCIMPORTER_H
#define LTABCIMPORTER_H

#include "assimp/StreamReader.h"
#include <assimp/BaseImporter.h>
#include <assimp/ParsingUtils.h>
#include <assimp/Profiler.h>

#include <vector>

#include "LT1ABC.h"

struct aiNode;

namespace Assimp {
namespace LT {
class ASSIMP_API LT1ABCImporter : public BaseImporter {
public:
    LT1ABCImporter() :
            m_FileSize(0), m_Buffer(nullptr), m_MeshVersion(0), m_Scene(nullptr), m_MeshHeader(nullptr), m_PieceHeader(nullptr), m_Profiler(nullptr) {};
    ~LT1ABCImporter() override;

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
    bool ReadWeightSets();
    bool ReadChildModels();
    bool ReadAnimations();
    bool ReadSockets();
    bool ReadAnimationBindings();

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
    LT::Header *m_MeshHeader;
    LT::PieceHeader *m_PieceHeader;
    std::vector<LT::Node *> m_Nodes;
    std::vector<LT::WeightSet *> m_WeightSets;
    std::vector<LT::ChildModel *> m_ChildModels;
    std::vector<LT::Animation *> m_Animations;
    std::vector<LT::Socket *> m_Sockets;
    std::vector<LT::AnimBinding *> m_AnimationBindings;
    std::vector<LT::AnimBinding *> m_ChildModelAnimationBindings;
    Profiling::Profiler *m_Profiler;
};
} // namespace LT
} // namespace Assimp

#endif // LTABCIMPORTER_H
#endif