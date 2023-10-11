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
    Texture2D               gSourceColor;
    Texture2D               gAlbedoColor;
    Texture2D               gEmissionColor;
    Texture2D               gLinearZTexture;
    Texture2D               gNormalsTexture;
    Texture2D               gPosNormalFWidth;
    Texture2D<PackedHitInfo>   gVisibilityBuffer;
    float2                  gScreenDimension;
    Texture2D<float4>       gMutualInfResult;
    int                     gMinHistoryCount;
    Texture2D<float4>       gColorAndVariance;
#if IS_DEBUG_PASS
    RWTexture2D<float4>         gColorTest;
#endif
};

static const int SPATIAL_BIN_COUNT  = (SPATIAL_RADIUS * 2 + 1) * (SPATIAL_RADIUS * 2 + 1); //r = 1 bincount = 9, r = 2 bincount = 25  

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
    float4 mutualInfResultBuf = gMutualInfResult[iPos].rgba;
    float mutualInfResult = mutualInfResultBuf.g;

    if(mutualInfResultBuf.b < float(gMinHistoryCount))
    {
        HitInfo centerHitInfoP  = HitInfo(gVisibilityBuffer[iPos]);
        float2 centerDepthP     = gLinearZTexture[iPos].xy;
        float3 centerNormalP    = gNormalsTexture[iPos].xyz;
        uint    centerMeshID    = getMeshInstanceID(centerHitInfoP);
        float fWidthNormal = gPosNormalFWidth[iPos].y;

        if(!centerHitInfoP.isValid())
        {
            return float4(gColorAndVariance[iPos].rgb, mutualInfResult);
        }

        float binPixel[SPATIAL_BIN_COUNT];
        float binLuminance[SPATIAL_BIN_COUNT];
        for(int frameIndex = 0; frameIndex < SPATIAL_BIN_COUNT; frameIndex++)
        {
            binLuminance[frameIndex] = 0.0;
            binPixel[frameIndex] = 0.0;
        }

        int acceptedPixelCount = 0;
        float sumLuminance = 0.0;

        for(int yy = -SPATIAL_RADIUS; yy <= SPATIAL_RADIUS; yy++) {
			for(int xx = -SPATIAL_RADIUS; xx <= SPATIAL_RADIUS; xx++)
            {
                int2 currentP = iPos + int2(xx,yy);
                bool accept  = true;

                if(all(currentP != iPos))
                {
                    HitInfo hitInfoP    = HitInfo(gVisibilityBuffer[currentP]);
                    float2 depthP       = gLinearZTexture[currentP].xy;
                    float3 normalP      = gNormalsTexture[currentP].xyz;

                    //check if inside screen
                    accept = isPixelWithinScreenDimensions(currentP, gScreenDimension.xy);

                    //vis buffer check
                    accept = accept && hitInfoP.isValid();
                    uint    meshIDP    = getMeshInstanceID(hitInfoP);
                    accept = accept && (meshIDP == centerMeshID);

                    // check if deviation of depths is acceptable
                    accept = accept && ((abs(depthP.x - centerDepthP.x) / (centerDepthP.y + 1e-2f)) < 10.f);

                    // check normals for compatibility
                    accept = accept && ((distance(centerNormalP, normalP) / (fWidthNormal + 1e-2)) < 16.0);
                }

                if(accept)
                {
                    float luminanceP = luminance(gColorAndVariance[currentP].rgb);
                    binLuminance[getLuminanceBuckedID(luminanceP, SPATIAL_BIN_COUNT)] += luminanceP;
                    binPixel[acceptedPixelCount]    =   luminanceP;
                    sumLuminance += luminanceP;

                    acceptedPixelCount++;
                }
            }
        }

        for(int pixelIndex = 0; pixelIndex < SPATIAL_BIN_COUNT; pixelIndex++)
        {
            binLuminance[pixelIndex ] /= sumLuminance;
            binPixel[pixelIndex]    /=   sumLuminance;
        }

        //Calculate mutual information
        mutualInfResult = 0.0;
        for (int binIdx = 0; binIdx < SPATIAL_BIN_COUNT; binIdx++)
        {
            float p_x = binPixel[binIdx];
            float p_y = binLuminance[binIdx];
            if(p_x > 0.0)
            {
                float result = p_x * log(p_x);
                if(isnan(result)){result = 0.0;}
                mutualInfResult -= result;
            }

            if(p_y > 0.0)
            {
                float result = p_y * log(p_y);
                if(isnan(result)){result = 0.0;}
                mutualInfResult -= result;
            }

            for (int binIdx1 = 0; binIdx1 < SPATIAL_BIN_COUNT; binIdx1++)
            {
                float p_xy = p_x * binLuminance[binIdx1];
                if(p_xy > 0.0)
                {
                    float result = p_xy * log(p_xy);
                    if(isnan(result)){result = 0.0;}
                    mutualInfResult += result;
                }
            }
        }
        //If mutual inf is 0, then either sumLuminance == 0 or no info about neighboring pixels
        //In both cases, we need to sample less of the color from neighboring pixels and use more from this current pixel
        //we can do exp(sumLuminance) for all cases 
        //or (1.0 - mutualInfResult) for when mutualInfResult == 0.0
        mutualInfResult = exp(-mutualInfResult);
    }
#if IS_DEBUG_PASS
    //gColorTest[iPos] = (mutualInfResultBuf.b < float(gMinHistoryCount)) ? float4(1.0, 0.0, 0.0, 1.0) : float4(0.0, 1.0, 0.0, 1.0);
#endif

    return float4(gColorAndVariance[iPos].rgb, saturate(mutualInfResult));
}
