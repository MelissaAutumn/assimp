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

#include "LTShared.h"

#include <assimp/StreamReader.h>
#include <assimp/ai_assert.h>
#include <assimp/types.h>

namespace Assimp::LT {

// FIXME: This can't be inlined because of stream reader bleh
std::string ReadLTString(StreamReaderLE *pBuffer, uint16_t assertLength) {
    ai_assert(pBuffer);

    const uint16_t len = pBuffer->GetU2();

    if (len > 0 && assertLength > 0 && len != assertLength) {
        return { "" };
    }

    // Sanity check
    ai_assert(len < 1024);

    // Don't even try to read an empty string...
    if (len == 0) {
        return { "" };
    }

    char string[len + 1];
    pBuffer->CopyAndAdvance(string, len);
    string[len] = '\0';
    return { string };
};

std::string ReadLTString(StreamReaderLE *pBuffer) {
    return ReadLTString(pBuffer, 0);
}

aiMatrix4x4 LTMatrix2aiMatrix(LTMatrix ltMat) {
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

/*
aiVector3f LTVector2aiVector(LTVector ltVec) {
    return {
        ltVec.x, ltVec.y, ltVec.z
    };
}
*/

aiQuaternion LTRotation2aiQuaternion(LTRotation ltRot) {
    return {
        ltRot.w, ltRot.x, ltRot.y, ltRot.z
    };
}

} // namespace Assimp::LT

#endif