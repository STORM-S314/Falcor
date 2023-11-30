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
#include "ASVGFUtils.slang"

cbuffer PerImageCB {
    Texture2D               gColorAndVariance;
    Texture2D               gColor;
    Buffer<double>          gTimeStepBuffer;
    Texture2D               gAlbedoTexture;
    Texture2D               gEmissionTexture;
    Texture2D               gSpecularAlbedo;
    Texture2D               gLinearZTexture;
    Texture2D               gPrevLinearZTexture;
    Texture2D               gNormalsTexture;
    Texture2D               gPrevNormalsTexture;
    Texture2D<PackedHitInfo>   gVisibilityBuffer;
    Texture2D<PackedHitInfo>   gPrevVisibilityBuffer;
    float                   gGradDiffRatioThreshold;
    Texture2D               gGradDifferenceRatio;
    Texture2D               gMotionVectorsTexture;
    Buffer<float>           gPrevMutualInfBuffer;
    RWBuffer<float>         gMutualInfBuffer;
    Texture2D<float4>       gPrevMutualInfResult;
    float2                  gScreenDimension;
    int                     gTotalPixelsInFrame;
    float                   gSpatialMIThreshold;
#if IS_DEBUG_PASS
    RWTexture2D<float4>     gColorTest;
#endif
};

static const float MAX_LUMINANCE = luminance(float3(1.0, 1.0, 1.0));

//Calculates the Luminance bucket id where the luminance would go into
//Assuming luminance would always be within 0.0 and 1.0
int getLuminanceBuckedID(float luminanceP, int luminanceBucketCount)
{
    return int((luminanceP / MAX_LUMINANCE) * float(luminanceBucketCount - 1));
}

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
    float historyLength = 1.0;
    float mutualInf = 0.0;
    float totalSumLuminance = 0.0;

    float2 motionVec = gMotionVectorsTexture[iPos].rg;
    
    const float2 prevPos = floor(vsOut.posH.xy) + motionVec.xy * gScreenDimension.xy;
    const int2 iPrevPos = int2(prevPos);// + motionVec.xy * gScreenDimension.xy + float2(0.5,0.5));

    float currentLuminance =   luminance(getIllumination(gColor[iPos].rgb, gAlbedoTexture[iPos].rgb, gSpecularAlbedo[iPos].rgb, gEmissionTexture[iPos].rgb)); //luminance(gColorAndVariance[iPos].rgb);

    float x = frac(prevPos.x);
    float y = frac(prevPos.y);
    const int2 offset[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };
    
    // bilinear weights
    const float bilinearWeights[4] = {  (1.0 - x)   * (1.0 - y),
                                        x           * (1.0 - y),
                                        (1.0 - x)   * y,
                                        x           * y  };

    int pixelIndexInFrame = iPos.y * gScreenDimension.x + iPos.x;
    int prevPixelIndexInFrame  = iPrevPos.y * gScreenDimension.x + iPrevPos.x;

    gMutualInfBuffer[pixelIndexInFrame] = currentLuminance;
    totalSumLuminance = currentLuminance;

//Check for validity of prev and surrounding previous pixels with the current
    float3 currentNormal = gNormalsTexture[iPos].xyz;
    float2 currentDepth = gLinearZTexture[iPos].xy;
    HitInfo currentHitInfo          = HitInfo(gVisibilityBuffer[iPos]);
	uint    currentMeshID           = getMeshInstanceID(currentHitInfo);
 
    bool offsetPixelValid[4] = {false, false, false, false};
    for(int offsetIndex = 0; offsetIndex < 4; offsetIndex++)
    {
        int2 offsetPixel =  iPrevPos + offset[offsetIndex];
        float3 prevNormal   = gPrevNormalsTexture[offsetPixel].xyz;
        float2 prevDepth    = gPrevLinearZTexture[offsetPixel].xy;

        bool isPrevPixelAccepted = isReprojectionValid(offsetPixel, gScreenDimension.xy, currentDepth.xy, prevDepth.xy, currentNormal.xyz, prevNormal.xyz);

        HitInfo prevHitInfo             = HitInfo(gPrevVisibilityBuffer[offsetPixel]);
        offsetPixelValid[offsetIndex]   = isPrevPixelAccepted && prevHitInfo.isValid() && (currentMeshID == getMeshInstanceID(prevHitInfo));
    }

//Add history to current buffer based on 4 offsets of prev pixels
    bool isHistorySet = false;
    for(int historyIndex = 1; historyIndex < LUM_FRAME_BIN_COUNT; historyIndex++)
    {
        float weightedSum = 0.0;
        float luminanceSum = 0.0;
        for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
        {
            int2 offsetPixel = iPrevPos + offset[sampleIdx];
            if(offsetPixelValid[sampleIdx] && (gPrevMutualInfResult[offsetPixel].b >= historyIndex))
            {
                luminanceSum += gPrevMutualInfBuffer[(prevPixelIndexInFrame + gTotalPixelsInFrame * (historyIndex - 1)) + offset[sampleIdx].x + offset[sampleIdx].y * gScreenDimension.x] * bilinearWeights[sampleIdx];
                weightedSum += bilinearWeights[sampleIdx];
            }
        }

        if((weightedSum > 0.5) && !isHistorySet)
        {
            float addluminance = luminanceSum / weightedSum;
            gMutualInfBuffer[pixelIndexInFrame + gTotalPixelsInFrame * historyIndex] = addluminance;
            totalSumLuminance += addluminance;
            historyLength = min(int(historyLength + 1.0), LUM_FRAME_BIN_COUNT);
        }
        else
        {
            isHistorySet = true;
        }
    }

    //If history length is enough for temporal mutual information calculation
    if(historyLength >= LUM_FRAME_BIN_COUNT)
    {
        float binsLumGroup[LUM_GROUP_BIN_COUNT];
        float binsLumFrames[LUM_FRAME_BIN_COUNT]   ;

        for(int frameIndex = 0; frameIndex < LUM_GROUP_BIN_COUNT; frameIndex++)
        {
            binsLumGroup[frameIndex] = 0.0;
        }

        for(int frameIndex = 0; frameIndex < LUM_FRAME_BIN_COUNT; frameIndex++)
        {
            int indexInBuffer = pixelIndexInFrame + frameIndex * gTotalPixelsInFrame;
            
            float bufferPixelLuminance  =   gMutualInfBuffer[indexInBuffer].r;
            float pixelLuminanceProbability  = bufferPixelLuminance / totalSumLuminance;
            binsLumFrames[frameIndex] =  pixelLuminanceProbability;
            binsLumGroup[getLuminanceBuckedID(bufferPixelLuminance, LUM_GROUP_BIN_COUNT)] += pixelLuminanceProbability;
        }

        for (int binIdx0 = 0; binIdx0 < LUM_FRAME_BIN_COUNT; binIdx0++)
        {
            float p_x = binsLumFrames[binIdx0];
            
            if(p_x > 0.0)
            {
                float result = p_x * log(p_x);
                if(isnan(result)){result = 0.0;}
                mutualInf -= result;
            }

            if((binIdx0 < LUM_GROUP_BIN_COUNT))
            {
                float p_y = binsLumGroup[binIdx0];
                if(p_y > 0.0)
                {
                    float result = p_y * log(p_y);
                    if(isnan(result)){result = 0.0;}
                    mutualInf -= result;
                }
            }

            for (int binIdx1 = 0; binIdx1 < LUM_GROUP_BIN_COUNT; binIdx1++)
            {
                float p_xy = p_x * binsLumGroup[binIdx1];
                if(p_xy > 0.0)
                {
                    float result = p_xy * log(p_xy);
                    if(isnan(result)){result = 0.0;}
                    mutualInf += result;
                }
            }
        }
    }


    float saturatedMutualInf = max(0.0, mutualInf);
    if(((saturatedMutualInf < gSpatialMIThreshold) && (int(historyLength) == LUM_FRAME_BIN_COUNT))
    || (gGradDifferenceRatio[iPos].r > gGradDiffRatioThreshold))
    {
        historyLength = 1.0;
    }

#if IS_DEBUG_PASS
    //gColorTest[iPos] = float4(0.0, saturatedMutualInf, 0.0, 1.0);
#endif

    float4 mutualInfResult = float4(0.0);
    mutualInfResult.r = totalSumLuminance;
    mutualInfResult.g = saturatedMutualInf;
    mutualInfResult.b = historyLength;

    return mutualInfResult;
}
