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
 import ColorSpace;
 #include "ASVGFCommon.slang"
 #include "ASVGFUtils.slang"

cbuffer PerImageCB
{
	Texture2D   gColor;
    Texture2D   gMotionVectorsTexture;
    Texture2D   gPrevAccumColorTexture;
    Texture2D   gPrevAccumMomentsTexture;
    Texture2D   gPrevHistLenTexture;
    Texture2D   gLinearZTexture;
    Texture2D   gPrevLinearZTexture;
    Texture2D   gNormalsTexture;
    Texture2D   gPrevNormalsTexture;
    Texture2D<PackedHitInfo>   gVisibilityBuffer;
    Texture2D<PackedHitInfo>   gPrevVisibilityBuffer;
    Texture2D   gEmissionTexture;
	Texture2D   gPrevEmissionTexture;
    Texture2D   gAlbedoTexture;
	Texture2D   gPrevAlbedoTexture;
    Texture2D   gPosNormalFWidth;
    Texture2D   gGradientDifferenceTexture;
    int         gGradientDownsample;
    float2      gScreenDimension;
    float2      gJitterOffset;
    float       gTemporalAlpha;
    float       gGradientFilterRadius;
};

struct PS_OUT
{
    float4  accumulated : SV_TARGET0;
    float2  moments     : SV_TARGET1;
    float   histLen     : SV_TARGET2;
}

PS_OUT main(FullScreenPassVsOut vsOut) 
{
    PS_OUT psOut;

    int2 iPos = int2(vsOut.posH.xy);
    float2 currentMotion = gMotionVectorsTexture[iPos].rg + gJitterOffset.xy;

    const int2 iPrevPos = int2(float2(iPos) + currentMotion.xy * gScreenDimension.xy + float2(0.5,0.5));
    const float2 prevPos = floor(vsOut.posH.xy) + currentMotion.xy * gScreenDimension.xy;

    float4  currentColor            = gColor[iPos].xyzw;
    float2  currentDepth            = gLinearZTexture[iPos].xy;
    float3  currentIllumination     = getIllumination(currentColor.xyz, gEmissionTexture[iPos].xyz, gAlbedoTexture[iPos].xyz);
    float   currentLuminance        = luminance(currentIllumination.rgb);
    float2  currentMomemnts         = float2(currentLuminance, currentLuminance * currentLuminance);
	uint    currentMeshID           = getMeshInstanceID(HitInfo(gVisibilityBuffer[iPos]));
    float3  currentNormal           = gNormalsTexture[iPos].xyz;
    float   currentPosNormalFWidth  = gPosNormalFWidth[iPos].y;

    float4 weightedColorPrev    = float4(0);
	float2 weightedMomentsPrev  = float2(0);
	float weightedSum           = 0.0;
	float histLen               = 0.0;

    float x = frac(prevPos.x);
    float y = frac(prevPos.y);
    const int2 offset[4] = { int2(0, 0), int2(1, 0), int2(0, 1), int2(1, 1) };

    // bilinear weights
    const float bilinearWeights[4] = {  (1 - x) * (1 - y),
                                        x       * (1 - y),
                                        (1 - x) * y,
                                        x       * y  };

    for (int sampleIdx = 0; sampleIdx < 4; sampleIdx++)
    {
        const int2 offsetLoc    = int2(prevPos) + offset[sampleIdx];
        float2 prevDepth        = gPrevLinearZTexture[offsetLoc].xy;
        float3 prevNormals      = gPrevNormalsTexture[offsetLoc].xyz;

        bool isAccept = isReprojectionValid(iPrevPos, gScreenDimension.xy, currentDepth.xy, prevDepth.xy, currentNormal.xyz, prevNormals.xyz, currentPosNormalFWidth);

        isAccept = isAccept && isPixelWithinScreenDimensions(offsetLoc, gScreenDimension.xy);

        uint pMeshID = getMeshInstanceID(HitInfo(gPrevVisibilityBuffer[offsetLoc]));

        isAccept = isAccept && (currentMeshID == pMeshID);

        if(isAccept)
        {
            float currentBilinearWeight = bilinearWeights[sampleIdx];
            weightedColorPrev   += gPrevAccumColorTexture[offsetLoc].xyzw * currentBilinearWeight;
            weightedMomentsPrev += gPrevAccumMomentsTexture[offsetLoc].xy * currentBilinearWeight;
            histLen             += gPrevHistLenTexture[offsetLoc].x * currentBilinearWeight;
            weightedSum         += currentBilinearWeight;
        }
    }

    //if Antilag alpha

    float antilagAlpha = 0.0;

	const int r = gGradientFilterRadius;

	for(int yy = -r; yy <= r; yy++) {
		for(int xx = -r; xx <= r; xx++) {
			float4 gradientDifference = gGradientDifferenceTexture[iPos / gGradientDownsample + int2(xx, yy)];
			float differenceProportion = clamp(abs(gradientDifference.r > 1e-4 ? abs(gradientDifference.g) / gradientDifference.r : 0.0), 0.0, 200.0);
			antilagAlpha = max(antilagAlpha, differenceProportion);
		}
	}

    clamp(antilagAlpha, 0.0, 1.0);
	if(isNaN(antilagAlpha))
    {
		antilagAlpha = 1.0;
    }

    //endif Antilag alpha

    if(weightedSum > 0.01) { /* found sufficiently reliable history information */
		weightedColorPrev   /= weightedSum;
		weightedMomentsPrev /= weightedSum;
		histLen             /= weightedSum;

        float alphaColor   = max(gTemporalAlpha, 1.0 / (histLen + 1.0)); //histLen max used is 9 to 9.99
	    float alphaMoments = max(0.6, 1.0 / (histLen + 1.0));   //histLen max used is 1.66

	    alphaColor   = lerp(alphaColor,   1.0, antilagAlpha);
	    alphaMoments = lerp(alphaMoments, 1.0, antilagAlpha);

        psOut.accumulated.xyz   =   lerp(weightedColorPrev.rgb, currentIllumination.xyz, alphaColor);
        psOut.accumulated.a     =   min(64, histLen + 1.0);
		psOut.moments.xy        =   lerp(weightedMomentsPrev.xy, currentMomemnts.xy, alphaMoments);
		psOut.histLen           =   clamp(1.0 / alphaColor, 0.0, 64.0);
    }
    else {
		psOut.accumulated.xyzw  =   float4(currentIllumination.xyz, 1.0);
		psOut.moments.xy        =   currentMomemnts.xy;
		psOut.histLen           =   1.0;
    }

    return psOut;
}
