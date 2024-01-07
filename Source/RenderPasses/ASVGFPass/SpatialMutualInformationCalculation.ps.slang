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
    Texture2D               gColor;
    Texture2D               gAlbedoTexture;
    Texture2D               gEmissionTexture;
    Texture2D               gSpecularAlbedo;
    Texture2D               gLinearZTexture;
    Texture2D               gNormalsTexture;
    Texture2D<PackedHitInfo>   gVisibilityBuffer;
    float2                  gScreenDimension;
    Texture2D<float4>       gTemporalMutualInfResult;
    int                     gMinHistoryCount;
    Texture2D<float4>       gColorAndVariance;
    float                   gGradDiffRatioThreshold;
    Texture2D               gGradDifferenceRatio;
    float                   gSpatialMIThreshold;
#if IS_DEBUG_PASS
    RWTexture2D<float4>     gColorTest;
    int2                    gDebugPixel;
    RWBuffer<float>         gSpatialDebugBuffer;
#endif
};

static const int SPATIAL_PIXEL_COUNT  = (SPATIAL_RADIUS * 2 + 1) * (SPATIAL_RADIUS * 2 + 1); //r = 1 bincount = 9, r = 2 bincount = 25

//Calculates the Luminance bucket id where the luminance would go into
//Assuming luminance would always be within 0.0 and 1.0
int getLuminanceBucketID(float luminanceP, int luminanceBucketCount, float maxLum)
{
    return int((luminanceP / maxLum) * float(luminanceBucketCount - 1));
}

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
    float4 mutualInfResultBuf = gTemporalMutualInfResult[iPos].rgba;
    float mutualInfResult = mutualInfResultBuf.g;
    float maxLum = 0.0;

    if((mutualInfResultBuf.g < gSpatialMIThreshold) ||
    (gGradDifferenceRatio[iPos].r > gGradDiffRatioThreshold) ||
    (mutualInfResultBuf.b < gMinHistoryCount))
    {
        HitInfo centerHitInfoP  = HitInfo(gVisibilityBuffer[iPos]);
        float2 centerDepth     = gLinearZTexture[iPos].xy;
        float3 centerNormal    = gNormalsTexture[iPos].xyz;
        uint    centerMeshID    = getMeshInstanceID(centerHitInfoP);

        if(!centerHitInfoP.isValid())
        {
            return float4(gColorAndVariance[iPos].rgb, mutualInfResult);
        }

        float binsLumPixel[SPATIAL_LUM_BIN_COUNT];
        float binsPixelIndex[SPATIAL_PIXEL_COUNT];
        float acceptedLum[SPATIAL_PIXEL_COUNT];

        for(int pixelIndex = 0; pixelIndex < SPATIAL_PIXEL_COUNT; pixelIndex++)
        {
            binsPixelIndex[pixelIndex] = 0.0;
            if(pixelIndex < SPATIAL_LUM_BIN_COUNT)
            {
                binsLumPixel[pixelIndex] = 0.0;
            }
        }

        int acceptedPixelCount = 0;
        float sumLuminance = 0.0;
        float sumWeights = 0.0;
        float sumPixelIndex = 0.0;

        for(int yy = -SPATIAL_RADIUS; yy <= SPATIAL_RADIUS; yy++) {
			for(int xx = -SPATIAL_RADIUS; xx <= SPATIAL_RADIUS; xx++)
            {
                int2 currentP = iPos + int2(xx,yy);
                bool accepted  = true;

                float currentluminanceValue = luminance(getIllumination(gColor[currentP].rgb, gAlbedoTexture[currentP].rgb, gSpecularAlbedo[currentP].rgb, gEmissionTexture[currentP].rgb));//luminance(gColorAndVariance[currentP].rgb); //
                if(all(currentP != iPos))
                {
                    HitInfo hitInfoP    = HitInfo(gVisibilityBuffer[currentP]);
                    float2 depthP       = gLinearZTexture[currentP].xy;
                    float3 normalP      = gNormalsTexture[currentP].xyz;
                    //float weightDepth = abs(depthP.x - centerDepth.x) / (centerDepth.y * length(float2(xx, yy)) + 1e-2);
                    //float weightNormals = pow(saturate(dot(normalP, centerNormal)), 128.0); 

				    uint meshID_p =  getMeshInstanceID(hitInfoP);

                    float meshIDWeight = ((hitInfoP.isValid() && (centerMeshID == meshID_p)) ? 1.0 : 0.0);

                    accepted = (meshIDWeight > 0.0) && (dot(normalP, centerNormal) > 0.9) && ((abs(depthP.x - centerDepth.x) / (centerDepth.y + 1e-2f)) < 10.f);
                }

                if(accepted)
                {
                    binsPixelIndex[acceptedPixelCount]    = (acceptedPixelCount + 1);
                    sumPixelIndex += binsPixelIndex[acceptedPixelCount];
                    acceptedLum[acceptedPixelCount] = currentluminanceValue;
                    maxLum = max(maxLum, currentluminanceValue);

                    acceptedPixelCount++;
                }
            }
        }

        for(int pixelIndex = 0; pixelIndex < acceptedPixelCount; pixelIndex++)
        {
            int bucketID = getLuminanceBucketID(acceptedLum[pixelIndex], SPATIAL_LUM_BIN_COUNT, maxLum);
            bucketID = min(bucketID, SPATIAL_LUM_BIN_COUNT -1);
            bucketID = max(bucketID, 0);

            binsLumPixel[bucketID] +=   1.0 / acceptedPixelCount;
            binsPixelIndex[pixelIndex]   /=  sumPixelIndex;

            binsLumPixel[bucketID] =   min(binsLumPixel[bucketID], 1.0);
            binsPixelIndex[pixelIndex]   =  min(binsPixelIndex[pixelIndex], 1.0);
        }

        #if IS_DEBUG_PASS
        for(int pixelIndex = 0; pixelIndex < SPATIAL_PIXEL_COUNT; pixelIndex++)
        {
                if(all(gDebugPixel == iPos))
                {
                    gSpatialDebugBuffer[2 + pixelIndex] = binsPixelIndex[pixelIndex];
                    gSpatialDebugBuffer[2 + pixelIndex + SPATIAL_PIXEL_COUNT] = acceptedLum[pixelIndex];

                    if(pixelIndex < SPATIAL_LUM_BIN_COUNT)
                    {
                        gSpatialDebugBuffer[2 + SPATIAL_PIXEL_COUNT + SPATIAL_PIXEL_COUNT + pixelIndex] = binsLumPixel[pixelIndex];
                    }
                    else
                    {
                        gSpatialDebugBuffer[2 + SPATIAL_PIXEL_COUNT + SPATIAL_PIXEL_COUNT + pixelIndex] = 0.0;
                    }
                }
        }
        #endif

        //Calculate mutual information
        mutualInfResult = 0.0;
        for (int binIdx0 = 0; binIdx0 < SPATIAL_LUM_BIN_COUNT; binIdx0++)
        {
            float p_x = binsLumPixel[binIdx0];
            if(p_x > 0.0)
            {
                float result = p_x * log(p_x);
                if(isnan(result)){result = 0.0;}
                mutualInfResult -= result;
            }
        }
        
        for (int binIdx1 = 0; binIdx1 < acceptedPixelCount; binIdx1++)
        {
            float p_y = binsPixelIndex[binIdx1];
            if(p_y > 0.0)
            {
                float result = p_y * log(p_y);
                if(isnan(result)){result = 0.0;}
                mutualInfResult -= result;
            }
        }
        
        for (int binIdx2 = 0; binIdx2 < SPATIAL_LUM_BIN_COUNT; binIdx2++)
        {
            float p_x = binsLumPixel[binIdx2];
            for (int binIdx3 = 0; binIdx3 < acceptedPixelCount; binIdx3++)
            {
                int binIdx = getLuminanceBucketID(acceptedLum[binIdx3], SPATIAL_LUM_BIN_COUNT, maxLum);
                binIdx = min(binIdx, SPATIAL_LUM_BIN_COUNT-1);
                binIdx = max(binIdx, 0);

                float p_xy =  binIdx == binIdx2 ? 1.0/(float)acceptedPixelCount : 0.0;
        
                if(p_xy > 0.0)
                {
                    float result = p_xy * log(p_xy);
                    if(isnan(result)){result = 0.0;}
                    mutualInfResult += result;
                }
            }
        }

        mutualInfResult = exp(-max(0.0, mutualInfResult));

#if IS_DEBUG_PASS

    if(all(gDebugPixel == iPos))
    {
        gColorTest[iPos] = float4(0.0, 1.0, 0.0, 1.0);
        gSpatialDebugBuffer[0] = acceptedPixelCount;
        gSpatialDebugBuffer[1] = mutualInfResult;
    }
    else
    {
        gColorTest[iPos] = float4(mutualInfResult, 0.0, 0.0, 1.0);
    }

#endif
    }
#if IS_DEBUG_PASS
    else
    {
        gColorTest[iPos] = float4(0.0, mutualInfResult, 0.0, 1.0);
    }
#endif

    return float4(gColorAndVariance[iPos].rgb, max(0.0, mutualInfResult));
}
