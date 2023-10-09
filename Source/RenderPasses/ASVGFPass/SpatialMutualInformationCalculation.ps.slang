/***************************************************************************
 # Copyright (c) 2015-23, NVIDIA CORPORATION. All rights reserved.
 #
 # Redistribution and use in source and binary forms, with or without
 # modification, are permitted provided that the following conditions
 # are met:
 #  * Redistributions of source code must retain the above copyright
 #    notice, this list of conditions and the following disclaimer.
 #  * Redistributions in binary form must reproduce the above copyright
 #    notice, this list of conditions and the following disclaimer in the
 #    documentation and/or other materials provided with the distribution.
 #  * Neither the name of NVIDIA CORPORATION nor the names of its
 #    contributors may be used to endorse or promote products derived
 #    from this software without specific prior written permission.
 #
 # THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS "AS IS" AND ANY
 # EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 # IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 # PURPOSE ARE DISCLAIMED.  IN NO EVENT SHALL THE COPYRIGHT OWNER OR
 # CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL,
 # EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO,
 # PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 # PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY
 # OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 # (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 # OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 **************************************************************************/
#include "ASVGFCommon.slang"

cbuffer PerImageCB {
    Texture2D               gSourceColor;
    Texture2D               gAlbedoColor;
    Texture2D               gEmissionColor;
    float2                  gScreenDimension;
    Texture2D<float4>       gMutualInfResult;
    int                     gMinHistoryCount;
#if IS_DEBUG_PASS
    RWTexture2D<float4>         gColorTest;
#endif
};


float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
    float4 mutualInfResultBuf = gMutualInfResult[iPos].rgba;
    float mutualInfResult = mutualInfResultBuf.g;

    if(mutualInfResultBuf.b < gMinHistoryCount)
    {
        
    }

    return float4(mutualInfResultBuf.r, mutualInfResult, mutualInfResultBuf.b, 0.0);
}
