/***************************************************************************
#	Copyright (c) 2018, Christoph Schied
#	All rights reserved.
#
#	Redistribution and use in source and binary forms, with or without
#	modification, are permitted provided that the following conditions are met:
#		* Redistributions of source code must retain the above copyright
#		  notice, this list of conditions and the following disclaimer.
#		* Redistributions in binary form must reproduce the above copyright
#		  notice, this list of conditions and the following disclaimer in the
#		  documentation and/or other materials provided with the distribution.
#		* Neither the name of the Karlsruhe Institute of Technology nor the
#		  names of its contributors may be used to endorse or promote products
#		  derived from this software without specific prior written permission.
#
#	THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
#	ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
#	WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
#	DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
#	DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
#	(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
#	LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
#	ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
#	(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
#	SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *************************************************************************/
 import Utils.Math.MathHelpers;
 #include "ASVGFCommon.slang"
 #include "ASVGFUtils.slang"

float nomalize(float x){
    return 1.0 - exp(-x);
}
cbuffer PerImageCB
{
	Texture2D   gColor;
    Texture2D   gMotionVectorsTexture;
    Texture2D   gPrevAccumColorTexture;
    Texture2D   gPrevAccumMomentsTexture;
    Texture2D   gPrevHistLenTexture;
    Texture2D   gPrevColorTexture;
    RWTexture2D<float4>   gCurrColorTexture;
    Texture2D   gLinearZTexture;
    Texture2D   gPrevLinearZTexture;
    Texture2D   gNormalsTexture;
    Texture2D   gPrevNormalsTexture;
    Texture2D<PackedHitInfo>   gVisibilityBuffer;
    Texture2D<PackedHitInfo>   gPrevVisibilityBuffer;
    Texture2D   gAlbedoTexture;
	Texture2D   gPrevAlbedoTexture;
    Texture2D   gEmissionTexture;
    Texture2D   gSpecularAlbedo;
    Texture2D   gPrevSpecularAlbedo;
    Texture2D<float4>   gGradientDifferenceTexture;
    int         gGradientDownsample;
    float2      gScreenDimension;
    float2      gJitterOffset;
    float       gTemporalColorAlpha;
    float       gTemporalMomentsAlpha;
    int         gGradientFilterRadius;
    int         gLutDimSize;
    const Buffer<float> gAntilagLUTBuffer;
#if IS_DEBUG_PASS
    RWTexture2D<float4>         gColorTest;
#endif
};

struct PS_OUT
{
    float4  accumulated     :   SV_TARGET0;
    float2  moments         :   SV_TARGET1;
    float   histLen         :   SV_TARGET2;
    float   gradDiffRatio   :   SV_TARGET3;
}

PS_OUT main(FullScreenPassVsOut vsOut) 
{
    PS_OUT psOut;

    int2 iPos = int2(vsOut.posH.xy);
    float2 currentMotion = gMotionVectorsTexture[iPos].rg;
    float currentVelocity = length(currentMotion);
    const float2 jitterOffset = float2(0.5, 0.5);
    const int2 iPrevPos = int2(float2(iPos) + currentMotion.xy * gScreenDimension.xy + jitterOffset - gJitterOffset.xy);
    const float2 prevPos = floor(vsOut.posH.xy) + currentMotion.xy * gScreenDimension.xy - gJitterOffset.xy;

    float4  currentColor            = gColor[iPos];
    float2  currentDepth            = gLinearZTexture[iPos].xy;
    float3  currentIllumination     = getIllumination(currentColor.xyz, gAlbedoTexture[iPos].rgb, gSpecularAlbedo[iPos].rgb, gEmissionTexture[iPos].rgb);
    float   currentLuminance        = luminance(currentIllumination);
    float2  currentMomemnts         = float2(currentLuminance, currentLuminance * currentLuminance);
    HitInfo currentHitInfo          = HitInfo(gVisibilityBuffer[iPos]);
	uint    currentMeshID           = getMeshInstanceID(currentHitInfo);
    float3  currentNormal           = gNormalsTexture[iPos].xyz;
    float   currentEmissionLuminance        = luminance(gEmissionTexture[iPos].rgb);
    float   currentAlbedoLuminance           = luminance(gAlbedoTexture[iPos].rgb);
    float   currentSpecularAlbedoLuminance   = luminance(gSpecularAlbedo[iPos].rgb);


    float   antilagAlpha            = 0.0;
    float   histLen                 = 0.0;
    float4  weightedColorPrev       = float4(0);
    float2  weightedMomentsPrev     = float2(0);
    float2  prevDepth               = gPrevLinearZTexture[iPrevPos].xy;
    float3  prevNormal              = gPrevNormalsTexture[iPrevPos].xyz;
    float   weightedSum             = 0.0;


    float x = frac(prevPos.x);
    float y = frac(prevPos.y);
    const int2 offset[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };
    // bilinear weights
    const float bilinearWeights[4] = {  (1.0 - x)   * (1.0 - y),
                                        x           * (1.0 - y),
                                        (1.0 - x)   * y,
                                        x           * y  };

    //if(currentHitInfo.isValid())
    {
        for (int sampleIdx = 0; sampleIdx < 4; sampleIdx ++)
        {
            const int2 offsetLoc    = int2(prevPos) + offset[sampleIdx];
            float2  prevDepth       = gPrevLinearZTexture[offsetLoc].xy;
            float3  prevNormals     = gPrevNormalsTexture[offsetLoc].xyz;

            bool isAccept = isReprojectionValid(offsetLoc, gScreenDimension.xy, currentDepth.xy, prevDepth.xy, currentNormal.xyz, prevNormals.xyz);

            isAccept = isAccept && isPixelWithinScreenDimensions(iPrevPos, gScreenDimension.xy);

            HitInfo pHitInfo    = HitInfo(gPrevVisibilityBuffer[offsetLoc]);
            uint pMeshID        = getMeshInstanceID(pHitInfo);

            isAccept  = isAccept && pHitInfo.isValid() && (currentMeshID == pMeshID);
            if(isAccept){
                float currentBilinearWeight = bilinearWeights[sampleIdx];
                weightedColorPrev   += gPrevAccumColorTexture[offsetLoc].xyzw * currentBilinearWeight;
                weightedMomentsPrev += gPrevAccumMomentsTexture[offsetLoc].xy * currentBilinearWeight;
                histLen             += gPrevHistLenTexture[offsetLoc].x * currentBilinearWeight;
                weightedSum         += currentBilinearWeight;
            }
        }
    }
    const int r = gGradientFilterRadius;
    float4 currentWeightedColor = float4(0,0,0,0);
    float currentWeightedSum = 0.0;
	for(int yy = -r; yy <= r; yy++) {
		for(int xx = -r; xx <= r; xx++) {
            int2 samplePos = iPos + int2(xx, yy);
            float3  currentIllumination = getIllumination(gColor[samplePos].rgb, gAlbedoTexture[samplePos].rgb, gSpecularAlbedo[samplePos].rgb, gEmissionTexture[samplePos].rgb);
            currentWeightedColor += float4(luminance(currentIllumination),0,0,0) * (r - abs(xx) + r - abs(yy));
            currentWeightedSum   += (r - abs(xx) + r - abs(yy));
		}
	}
    currentWeightedColor /= currentWeightedSum;

    if(weightedSum > 0.01)/* found sufficiently reliable history information */
    {
        weightedColorPrev   /= weightedSum;
		weightedMomentsPrev /= weightedSum;
		histLen             /= weightedSum;

        float prevWeightedLuminance    = luminance(weightedColorPrev.rgb);
        float2 prevWeightedMoments      = float2(prevWeightedLuminance, prevWeightedLuminance * prevWeightedLuminance);

        // Quantize
        int lumIdx = int(nomalize(currentLuminance) * (gLutDimSize - 1));
        //int lumIdx = int(nomalize(currentWeightedColor.r) * (gLutDimSize - 1));
        int prevLumIdx = int(nomalize(prevWeightedLuminance) * (gLutDimSize - 1));
        int depthIdx = int(nomalize(currentDepth.x) * (gLutDimSize - 1));
        int prevDepthIdx = int(nomalize(abs(prevDepth.x)) * (gLutDimSize - 1));
        int normalIdx = int(nomalize(dot(currentNormal, prevNormal)) * (gLutDimSize - 1));
        int specularAlbedoIdx = int(nomalize(currentSpecularAlbedoLuminance) * (gLutDimSize - 1));
        int emissionLumIdx = int(nomalize(currentEmissionLuminance) * (gLutDimSize - 1));
        int albedoLumIdx = int(nomalize(currentAlbedoLuminance) * (gLutDimSize - 1));
        int velocityIdx = int(nomalize(currentVelocity) * (gLutDimSize - 1));
        // Compute the index into the LUT
        int lutIdx = 0;
        //lutIdx += albedoLumIdx;
        //lutIdx *= gLutDimSize;
        lutIdx += emissionLumIdx;
        lutIdx *= gLutDimSize;
        lutIdx += specularAlbedoIdx;
        lutIdx *= gLutDimSize;
        //lutIdx += normalIdx;
        //lutIdx *= gLutDimSize;
        //lutIdx += prevDepthIdx;
        //lutIdx *= gLutDimSize;
        //lutIdx += depthIdx;
        //lutIdx *= gLutDimSize;
        //lutIdx += prevLumIdx;
        //lutIdx *= gLutDimSize;
        lutIdx += lumIdx;
        //lutIdx *= gLutDimSize;
        //lutIdx += velocityIdx;
        // Get the antilag value from the LUT
        antilagAlpha        =gAntilagLUTBuffer[lutIdx];        

        // Compute the alpha values for the temporal accumulation
        float alphaColor = max(gTemporalColorAlpha, 1.0 / (histLen + 1.0));
        float alphaMoments = max(gTemporalMomentsAlpha, 1.0 / (histLen + 1.0));

        // Equation 15 as per the paper
        alphaColor   = lerp(alphaColor, 1.0, antilagAlpha);
	    alphaMoments = lerp(alphaMoments, 1.0, antilagAlpha);
        
        // Reuse the prevAccumColor around the current pixel before temporal accumulation
        psOut.accumulated.rgb   =   lerp(weightedColorPrev.rgb, currentIllumination.rgb, alphaColor);
        psOut.accumulated.a     =   min(64, histLen + 1.0);
	    psOut.moments.rg        =   lerp(weightedMomentsPrev.rg, currentMomemnts.rg, alphaMoments);
	    psOut.histLen.r         =   clamp(1.0 / alphaColor, 0.0, 64.0);
        psOut.gradDiffRatio.r   =   antilagAlpha;
        gCurrColorTexture[iPos] =   currentColor;
    #if IS_DEBUG_PASS
        gColorTest[iPos] = float4(alphaColor, 0.0 , 0.0 , 1.0);
        //gColorTest[iPos] = float4(currentWeightedColor.rgb , 1.0);
    #endif
    }else{
        psOut.accumulated.xyzw  =   float4(currentIllumination.rgb, 1.0);
		psOut.moments.xy        =   currentMomemnts.xy;
		psOut.histLen.r         =   1.0;
        psOut.gradDiffRatio.r   =   antilagAlpha;
    }
    return psOut;
}
