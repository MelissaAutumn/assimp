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
#include "LTBaseABCImporter.h"
#include "assimp/scene.h"

#include "LT1/LT1ABCImporter.h"
#include "LT2/LT2ABCImporter.h"

namespace Assimp {
static constexpr aiImporterDesc desc = {
    "Lithtech ABC Importer",
    "melissaautumn",
    "melissaautumn",
    "Supports v6 to v12, with experimental v13 support",
    aiImporterFlags_SupportBinaryFlavour,
    9,
    0,
    13,
    0,
    "abc"
};

LTBaseABCImporter::LTBaseABCImporter() {
    m_pLT2ABCImporter = new LT::LT2::LT2ABCImporter();
}
LTBaseABCImporter::~LTBaseABCImporter() = default;

bool LTBaseABCImporter::CanRead(const std::string &pFile, IOSystem *pIOHandler, bool) const {
    // We can't mark things here, so just read the file extension,
    // we'll do the real check in InternReadFile
    return SimpleExtensionCheck(pFile, "abc");
}

void LTBaseABCImporter::SetupProperties(const Importer *pImp) {
    BaseImporter::SetupProperties(pImp);
}

const aiImporterDesc *LTBaseABCImporter::GetInfo() const {
    return &desc;
}

void LTBaseABCImporter::InternReadFile(const std::string &pFile, aiScene *pScene, IOSystem *pIOHandler) {
    if (m_pLT2ABCImporter->CanRead(pFile, pIOHandler, false)) {
        m_pLT2ABCImporter->ReadFile(pFile, pScene, pIOHandler);
        return;
    }


}

} // namespace Assimp
#endif
