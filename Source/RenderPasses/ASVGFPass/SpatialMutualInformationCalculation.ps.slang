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
    Texture2D               gLinearZTexture;
    Texture2D               gNormalsTexture;
    Texture2D<PackedHitInfo>   gVisibilityBuffer;
    float2                  gScreenDimension;
    Texture2D<float4>       gTemporalMutualInfResult;
    int                     gMinHistoryCount;
    Texture2D<float4>       gColorAndVariance;
    Texture2D               gGradDifferenceRatio;
#if IS_DEBUG_PASS
    RWTexture2D<float4>         gColorTest;
#endif
};

static const int SPATIAL_PIXEL_BIN_COUNT  = (SPATIAL_RADIUS * 2 + 1) * (SPATIAL_RADIUS * 2 + 1); //r = 1 bincount = 9, r = 2 bincount = 25  

static const float MAX_LUMINANCE = luminance(float3(1.0, 1.0, 1.0));

//Calculates the Luminance bucket id where the luminance would go into
//Assuming luminance would always be within 0.0 and 1.0
int getLuminanceBucketID(float luminanceP, int luminanceBucketCount)
{
    return int((luminanceP / MAX_LUMINANCE) * float(luminanceBucketCount - 1));
}

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
    float4 mutualInfResultBuf = gTemporalMutualInfResult[iPos].rgba;
    float mutualInfResult = mutualInfResultBuf.g;

    if(mutualInfResultBuf.g < 0.1)
    {
        HitInfo centerHitInfoP  = HitInfo(gVisibilityBuffer[iPos]);
        float2 centerDepth     = gLinearZTexture[iPos].xy;
        float3 centerNormal    = gNormalsTexture[iPos].xyz;
        uint    centerMeshID    = getMeshInstanceID(centerHitInfoP);
        uint    centerLuminance    = luminance(gColorAndVariance[iPos].rgb);

        if(!centerHitInfoP.isValid())
        {
            return float4(gColorAndVariance[iPos].rgb, mutualInfResult);
        }

        float binsLumPixel[SPATIAL_PIXEL_BIN_COUNT];
        float binsLumGroup[SPATIAL_GROUP_BIN_COUNT];
        for(int frameIndex = 0; frameIndex < SPATIAL_GROUP_BIN_COUNT; frameIndex++)
        {
            binsLumGroup[frameIndex] = 0.0;
        }

        int acceptedPixelCount = 0;
        float sumLuminance = 0.0;

        for(int yy = -SPATIAL_RADIUS; yy <= SPATIAL_RADIUS; yy++) {
			for(int xx = -SPATIAL_RADIUS; xx <= SPATIAL_RADIUS; xx++)
            {
                int2 currentP = iPos + int2(xx,yy);

                float currentluminanceValue = luminance(gColorAndVariance[currentP].rgb);
                if(all(currentP != iPos))
                {
                    HitInfo hitInfoP    = HitInfo(gVisibilityBuffer[currentP]);
                    float2 depthP       = gLinearZTexture[currentP].xy;
                    float3 normalP      = gNormalsTexture[currentP].xyz;
                    float weightDepth = abs(depthP.x - centerDepth.x) / (centerDepth.y * length(float2(xx, yy)) + 1e-2);
                    float weightNormals = pow(saturate(dot(normalP, centerNormal)), 128.0); 

				    uint meshID_p =  getMeshInstanceID(hitInfoP);

                    float meshIDWeight = ((hitInfoP.isValid() && (centerMeshID == meshID_p)) ? 1.0 : 0.0);
				    float weight = exp(- weightDepth) * weightNormals * meshIDWeight;
                    currentluminanceValue *=  (1.0 + weight) * meshIDWeight;
                }

                binsLumGroup[getLuminanceBucketID(currentluminanceValue, SPATIAL_GROUP_BIN_COUNT)] += currentluminanceValue;
                binsLumPixel[acceptedPixelCount]    =   currentluminanceValue;
                sumLuminance += currentluminanceValue;
                
                acceptedPixelCount++;
            }
        }

        for(int pixelIndex = 0; pixelIndex < SPATIAL_PIXEL_BIN_COUNT; pixelIndex++)
        {
            binsLumPixel[pixelIndex]        /=  sumLuminance;
            if(pixelIndex < SPATIAL_GROUP_BIN_COUNT)
            {
                binsLumGroup[pixelIndex ]   /=  sumLuminance;
            }
        }

        //Calculate mutual information
        mutualInfResult = 0.0;
        for (int binIdx = 0; binIdx < SPATIAL_PIXEL_BIN_COUNT; binIdx++)
        {
            float p_x = binsLumPixel[binIdx];
            
            if(p_x > 0.0)
            {
                float result = p_x * log(p_x);
                if(isnan(result)){result = 0.0;}
                mutualInfResult -= result;
            }

            if((binIdx < SPATIAL_GROUP_BIN_COUNT))
            {
                float p_y = binsLumGroup[binIdx];
                if(p_y > 0.0)
                {
                    float result = p_y * log(p_y);
                    if(isnan(result)){result = 0.0;}
                    mutualInfResult -= result;
                }
            }

            for (int binIdx1 = 0; binIdx1 < SPATIAL_GROUP_BIN_COUNT; binIdx1++)
            {
                float p_xy = p_x * binsLumGroup[binIdx1];
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
        mutualInfResult = exp(-max(0.0, mutualInfResult));
#if IS_DEBUG_PASS
    gColorTest[iPos] = float4(mutualInfResult, 0.0, 0.0, 1.0);
#endif
    }
#if IS_DEBUG_PASS
    else
    {
        //gColorTest[iPos] = float4(0.0, mutualInfResult, 0.0, 1.0);
    }
#endif

    return float4(gColorAndVariance[iPos].rgb, max(0.0, mutualInfResult));
}
