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

#ifndef LTSHARED_H
#define LTSHARED_H

#include <cstdint>
#include <string>

#include "assimp/StreamReader.h"
#include "assimp/matrix4x4.h"
#include "assimp/quaternion.h"

#if defined(__GNUC__)
#define WITH_NO_PADDING_SUPPORTED
#define WITH_NO_PADDING __attribute__((packed))
#else
#define WITH_NO_PADDING
#endif

namespace Assimp::LT {

struct LTString {
    short stringLength;
    char *string;
};

std::string ReadLTString(StreamReaderLE *pBuffer, uint16_t assertLength);
std::string ReadLTString(StreamReaderLE *pBuffer);

struct LTTexCoord {
    float u, v;
};

struct LTByteVector {
    int8_t x, y, z;
};

struct LTShortVector {
    uint16_t x, y, z;
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

aiMatrix4x4 LTMatrix2aiMatrix(LTMatrix ltMat);
aiVector3f LTVector2aiVector(LTVector ltVec);
aiQuaternion LTRotation2aiQuaternion(LTRotation ltRot);

} // namespace Assimp::LT
#endif // LTSHARED_H

#endif