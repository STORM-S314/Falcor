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
    Buffer<float>           gTimeStepBuffer;
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
    RWBuffer<float>         gTemporalDebugBuffer;
    int2                    gDebugPixel;
#endif
};

static const float MAX_LUMINANCE = luminance(float3(1.0,1.0,1.0));

//Calculates the Luminance bucket id where the luminance would go into
//Assuming luminance would always be within 0.0 and 1.0
int getLuminanceBucketID(float luminanceP, int luminanceBucketCount, float maxLum)
{
    return int((luminanceP / maxLum) * float(luminanceBucketCount - 1));
}

struct PS_OUT
{
    float4 temporalMIResult         : SV_TARGET0;
	float4 accumulatedColorAndMI    : SV_TARGET1;
};

PS_OUT main(FullScreenPassVsOut vsOut)
{
    int2 iPos = int2(vsOut.posH.xy);
    float historyLength = 1.0;
    float mutualInf = 0.0;
    float totalSumLuminance = 0.0;
    float totalSumTimeStep = 0.0;
    float maxLum = 0.0;

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
    totalSumTimeStep    =   gTimeStepBuffer[0];

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
    for(int historyIndex = 1; historyIndex < FRAME_HISTORY_COUNT; historyIndex++)
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
            totalSumTimeStep += gTimeStepBuffer[historyIndex];
            historyLength = min(int(historyLength + 1.0), FRAME_HISTORY_COUNT);
            maxLum = max(maxLum, addluminance);
        }
        else
        {
            isHistorySet = true;
            gMutualInfBuffer[pixelIndexInFrame + gTotalPixelsInFrame * historyIndex] = 0.0;
        }
    }

    //If history length is enough for temporal mutual information calculation
    float binsFrameTimeStep[FRAME_HISTORY_COUNT];
    float binsFramesLum[LUM_FRAME_BIN_COUNT]   ;

    for(int frameIndex = 0; frameIndex < LUM_FRAME_BIN_COUNT; frameIndex++)
    {
        binsFramesLum[frameIndex] = 0.0;
    }

    for(int frameIndex = 0; frameIndex < (int)historyLength; frameIndex++)
    {
        int indexInBuffer = pixelIndexInFrame + frameIndex * gTotalPixelsInFrame;

        int bucketID = getLuminanceBucketID(gMutualInfBuffer[indexInBuffer].r, LUM_FRAME_BIN_COUNT, maxLum);
        bucketID = min(bucketID, LUM_FRAME_BIN_COUNT -1);
        bucketID = max(bucketID, 0);

        binsFramesLum[bucketID] += 1.0 / historyLength;
        binsFrameTimeStep[frameIndex] =  (gTimeStepBuffer[frameIndex].r) / (totalSumTimeStep );

        binsFramesLum[bucketID] = min(binsFramesLum[bucketID], 1.0);
        binsFrameTimeStep[frameIndex] = min(binsFrameTimeStep[frameIndex], 1.0);

#if IS_DEBUG_PASS
    if(all(gDebugPixel == iPos))
    {
        gTemporalDebugBuffer[2 + frameIndex] = gMutualInfBuffer[indexInBuffer].r;
        gTemporalDebugBuffer[2 + frameIndex + FRAME_HISTORY_COUNT] =  gTimeStepBuffer[frameIndex].r;
        gTemporalDebugBuffer[2 + frameIndex + FRAME_HISTORY_COUNT + FRAME_HISTORY_COUNT] = binsFrameTimeStep[frameIndex];
    }
#endif
    }

#if IS_DEBUG_PASS
if(all(gDebugPixel == iPos))
{
    for(int bucketIndex = 0; bucketIndex < LUM_FRAME_BIN_COUNT; bucketIndex++)
    {
        gTemporalDebugBuffer[2 + FRAME_HISTORY_COUNT + FRAME_HISTORY_COUNT + FRAME_HISTORY_COUNT+ bucketIndex] = binsFramesLum[bucketIndex].r;
    }
}
#endif

    for (int binIdx0 = 0; binIdx0 < (int)historyLength; binIdx0++)
    {
        float p_x = binsFrameTimeStep[binIdx0];
        float result = p_x * log(p_x);
        if(isnan(result)){result = 0.0;}
        mutualInf -= result;
    }

    for (int binIdx1 = 0; binIdx1 < LUM_FRAME_BIN_COUNT; binIdx1++)
    {
        float p_y = binsFramesLum[binIdx1];
        float result = p_y * log(p_y);
        if(isnan(result)){result = 0.0;}
        mutualInf -= result;
    }

    for (int binIdx1 = 0; binIdx1 < LUM_FRAME_BIN_COUNT; binIdx1++)
    {
        for (int binIdx2 = 0; binIdx2 < (int)historyLength; binIdx2++)
        {
            float p_xy = getLuminanceBucketID(gMutualInfBuffer[binIdx2].r, LUM_FRAME_BIN_COUNT, maxLum) == binIdx1 ? 1.0/historyLength : 0.0;
            
            float result = p_xy * log(p_xy);
            if(isnan(result)){result = 0.0;}
            mutualInf += result;
        }
    }

#if IS_DEBUG_PASS
    
    if(all(gDebugPixel == iPos))
    {
        gColorTest[iPos] = float4(1.0, 0.0, 0.0, 1.0);
        gTemporalDebugBuffer[0] = historyLength;
        gTemporalDebugBuffer[1] = mutualInf;
    }
    else
    {
        gColorTest[iPos] = float4(0.0, mutualInf, 0.0, 1.0);
    }
#endif

    PS_OUT psOut;
    psOut.temporalMIResult      =   float4(totalSumLuminance, mutualInf, historyLength, 1.0);
    psOut.accumulatedColorAndMI =   float4(gColorAndVariance[iPos].rgb, mutualInf);

    return psOut;
}
