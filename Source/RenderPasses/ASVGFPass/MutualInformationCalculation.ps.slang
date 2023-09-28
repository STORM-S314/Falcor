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
    RWBuffer<float>         gMutualInfBuffer;
    Texture2D<float>        gLuminanceSumTexture;
    float2                  gScreenDimension;
    int                     gFrameNum;
    int                     gTotalPixelsInFrame;
#if IS_DEBUG_PASS
    RWTexture2D<float4>         gColorTest;
#endif
};

//Calculates the Luminance bucket id where the luminance would go into
//Assuming luminance would always be within 0.0 and 1.0
int getLuminanceBuckedID(float luminanceP, int luminanceBucketCount)
{
    return int((luminanceP / (0.299 + 0.587 + 0.114)) * float(luminanceBucketCount - 1));
}

struct PS_OUT
{
    float  sumLuminance : SV_TARGET0;
    float  mutualInfVal : SV_TARGET1;
}

PS_OUT main(FullScreenPassVsOut vsOut)
{
    int2 iPos = int2(vsOut.posH.xy);
    PS_OUT psOut;

    float binLuminance[BIN_COUNT];
    float binFrames[BIN_COUNT]   ;

    for(int frameIndex = 0; frameIndex < BIN_COUNT; frameIndex++)
    {
        binLuminance[frameIndex] = 0.0;
    }

    float currentLuminance =    luminance(getIllumination(gSourceColor[iPos].rgb, gAlbedoColor[iPos].rgb, gEmissionColor[iPos].rgb));

    int bufferFrameIdx = gFrameNum % BIN_COUNT;
    int pixelIndexInFrame = iPos.y * gScreenDimension.x + iPos.x;
    int bufferPixelValueIndex = pixelIndexInFrame + bufferFrameIdx * gTotalPixelsInFrame;

    float totalSumLuminance = gLuminanceSumTexture[iPos];

    //If frames are greater or equal than required to calculate the MI.
    if(gFrameNum >= BIN_COUNT)
    {
        totalSumLuminance -= gMutualInfBuffer[bufferPixelValueIndex];
    }

    totalSumLuminance += currentLuminance;
    gMutualInfBuffer[bufferPixelValueIndex] = currentLuminance;
    float mutualInf = 0.0;

    //1st time the buffer is completely filled up
    if(gFrameNum >= (BIN_COUNT - 1))
    {
        for(int frameIndex = 0; frameIndex < BIN_COUNT; frameIndex++)
        {
            int indexInBuffer = pixelIndexInFrame + frameIndex * gTotalPixelsInFrame;
            
            float bufferPixelLuminance  =   gMutualInfBuffer[indexInBuffer].r;
            float pixelLuminanceProbability  = bufferPixelLuminance / totalSumLuminance;
            binFrames[frameIndex] =  pixelLuminanceProbability;
            binLuminance[getLuminanceBuckedID(bufferPixelLuminance, BIN_COUNT)] += pixelLuminanceProbability;
        }

        for (int binIdx0 = 0; binIdx0 < BIN_COUNT; binIdx0++)
        {
            float p_x = binFrames[binIdx0];
            float p_y = binLuminance[binIdx0];
            if(p_x > 0.0)
            {
                float result = p_x * log(p_x);
                if(isnan(result)){result = 0.0;}
                mutualInf -= result;
            }

            if(p_y > 0.0)
            {
                float result = p_y * log(p_y);
                if(isnan(result)){result = 0.0;}
                mutualInf -= result;
            }

            for (int binIdx1 = 0; binIdx1 < BIN_COUNT; binIdx1++)
            {
                float p_xy = p_x * binLuminance[binIdx1];
                if(p_xy > 0.0)
                {
                    float result = p_xy * log(p_xy);
                    if(isnan(result)){result = 0.0;}
                    mutualInf += result;
                }
            }
        }
    }

#if IS_DEBUG_PASS
    gColorTest[iPos] = float4(0.0, mutualInf, 0.0, 1.0);
#endif

    psOut.sumLuminance  =   totalSumLuminance;
    psOut.mutualInfVal  =   saturate(mutualInf);
    return psOut;
}
