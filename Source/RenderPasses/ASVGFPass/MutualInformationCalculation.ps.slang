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
    RWTexture2D<float>      gLuminanceSumTexture;
    Texture2D               gPrevLuminanceSumTexture;
    int                     gNumFramesInMICalc;
    float2                  gScreenDimension;
    int                     gFrameNum;
    RWTexture2D<float4>     gColorTest;
};

//Calculates the Luminance bucket id where the luminance would go into
//Assuming luminance would always be withing 0.0 and 1.0
int getLuminanceBuckedID(float luminance, int luminanceBcketCount)
{
    return int(luminance * float(luminanceBcketCount));
}

float main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);

    float binLuminance[360];
    for(int frameIndex = 0; frameIndex < gNumFramesInMICalc; frameIndex++)
    {
        binLuminance[frameIndex] = 0.0;
    }

    float currentLuminance =    luminance(getIllumination(gSourceColor[iPos].rgb, gAlbedoColor[iPos].rgb, gEmissionColor[iPos].rgb));

    int bufferFrameIdx = gFrameNum % gNumFramesInMICalc;
    int bufferPixelValueIndex = iPos.y * gScreenDimension.x + iPos.x + bufferFrameIdx * gScreenDimension.x * gScreenDimension.y;

    //If frames are less than required to calculate the MI. This happens only for the first gNumFramesInMICalc
    if(gFrameNum < gNumFramesInMICalc)
    {
        gMutualInfBuffer[bufferPixelValueIndex] = currentLuminance;
        gLuminanceSumTexture[iPos] += currentLuminance;
    }

    //1st time the buffer is completely filled up
    if(gFrameNum == (gNumFramesInMICalc - 1))
    {
        for(int frameIndex = 0; frameIndex < gNumFramesInMICalc; frameIndex++)
        {
            int indexInBuffer = iPos.y * gScreenDimension.x + iPos.x + frameIndex * gScreenDimension.x * gScreenDimension.y;
            float bufferPixelLuminance  =   gMutualInfBuffer[indexInBuffer].r;
            gMutualInfBuffer[indexInBuffer].r /= gLuminanceSumTexture[iPos];
            binLuminance[getLuminanceBuckedID(bufferPixelLuminance, gNumFramesInMICalc)] += gMutualInfBuffer[indexInBuffer].r;
        }
    }
    else if(gFrameNum > (gNumFramesInMICalc - 1))
    {
        gLuminanceSumTexture[iPos].r -= (gLuminanceSumTexture[iPos].r * gMutualInfBuffer[bufferPixelValueIndex].r);
        gLuminanceSumTexture[iPos].r += currentLuminance;

        float currentPixelLuminanceSum = gLuminanceSumTexture[iPos].r;
        float prevPixelLuminanceSum = gPrevLuminanceSumTexture[iPos].r;

        //Set the probability of the current frame
        gMutualInfBuffer[bufferPixelValueIndex].r = gMutualInfBuffer[bufferPixelValueIndex].r / currentPixelLuminanceSum;

        //setting the probability of the 2d histogram by recreating the luminance and then dividing by the new total sum to give the new probability
        for(int frameIndex = 0; frameIndex < gNumFramesInMICalc; frameIndex++)
        {
            int indexInBuffer = iPos.y * gScreenDimension.x + iPos.x + frameIndex * gScreenDimension.x * gScreenDimension.y;
            if(indexInBuffer != bufferPixelValueIndex)
            {
                float bufferPixelLuminance  =   (gMutualInfBuffer[indexInBuffer].r * prevPixelLuminanceSum);
                gMutualInfBuffer[indexInBuffer].r = bufferPixelLuminance / currentPixelLuminanceSum;
                binLuminance[getLuminanceBuckedID(bufferPixelLuminance, gNumFramesInMICalc)] += gMutualInfBuffer[indexInBuffer].r;
            }
        }
    }


    //Calculate MI
    float mutualInf = 0.0;
    if(gFrameNum >= (gNumFramesInMICalc - 1))
    {
        for (int binIdx = 0; binIdx < gNumFramesInMICalc; binIdx++)
        {
            //int bufferPixelValueIndex = iPos.y * gScreenDimension.x + iPos.x + bufferFrameIdx * gScreenDimension.x * gScreenDimension.y;
            //float p_x = gMutualInfBuffer[bufferPixelValueIndex];
            float p_y = binLuminance[binIdx];
            //float p_xy = gMutualInfBuffer[bufferPixelValueIndex];
            //mutualInf -= p_x * log(p_x);
            mutualInf -= p_y * log(p_y);
            //mutualInf += p_xy * log(p_xy);
        }
        
        //for (int binIdx0 = 0; binIdx0 < numBins; binIdx0++)
        //{
        //    for (int binIdx1 = 0; binIdx1 < numBins; binIdx1++)
        //    {
        //        float p_xy = histogram2d[binIdx0 * numBins + binIdx1];
        //        if (p_xy > EPSILON_2D)
        //        {
        //            mutualInf += p_xy * log(p_xy);
        //        }
        //    }
        //}
    }
    gColorTest[iPos] = float4(0.0, mutualInf, 0.0, 1.0);
    return mutualInf;
}
