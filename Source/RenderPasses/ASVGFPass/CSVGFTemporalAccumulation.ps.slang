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
//    float exp2x = exp(2.0 * x);
//    return (exp2x - 1.0) / (exp2x + 1.0);
    return clamp(x, 0.0, 1.0);
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
    int         gLumDimSize;
    int         gDepthDimSize;
    Buffer<float> gAntilagLUTBuffer;
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

    const int2 iPrevPos = int2(float2(iPos) + currentMotion.xy * gScreenDimension.xy + float2(0.5,0.5) - gJitterOffset.xy);

    float4  currentColor            = gColor[iPos].xyzw;
    float2  currentDepth            = gLinearZTexture[iPos].xy;
    float3  currentIllumination     = getIllumination(currentColor.xyz, gAlbedoTexture[iPos].rgb, gSpecularAlbedo[iPos].rgb, gEmissionTexture[iPos].rgb);
    float   currentLuminance        = luminance(currentColor.rgb);
    float2  currentMomemnts         = float2(currentLuminance, currentLuminance * currentLuminance);
    HitInfo currentHitInfo          = HitInfo(gVisibilityBuffer[iPos]);
	uint    currentMeshID           = getMeshInstanceID(currentHitInfo);
    float3  currentNormal           = gNormalsTexture[iPos].xyz;

    float   antilagAlpha            = 0.0;
    bool    isAccept                = false;
    float   histLen                 = 0.0;
    float4  prevAccumColor          = float4(0);
    float2  prevMoments             = float2(0);
    if(currentHitInfo.isValid()){
        float4  prevColor               = gPrevColorTexture[iPrevPos].xyzw;
        prevAccumColor          = gPrevAccumColorTexture[iPrevPos].xyzw;
        prevMoments             = gPrevAccumMomentsTexture[iPrevPos].xy;
        float2  prevDepth               = gPrevLinearZTexture[iPrevPos].xy;
        float3  prevNormals             = gPrevNormalsTexture[iPrevPos].xyz;
        histLen                         = gPrevHistLenTexture[iPrevPos].x;
        isAccept = isReprojectionValid(iPrevPos, gScreenDimension.xy, currentDepth.xy, prevDepth.xy, currentNormal.xyz, prevNormals.xyz);
        isAccept = isAccept && isPixelWithinScreenDimensions(iPrevPos, gScreenDimension.xy);
        HitInfo pHitInfo    = HitInfo(gPrevVisibilityBuffer[iPrevPos]);
        uint pMeshID = getMeshInstanceID(pHitInfo);
        isAccept  = isAccept && pHitInfo.isValid() && (currentMeshID == pMeshID);
        if(isAccept){
            // Quantize
            int currLumIdx = int(nomalize(luminance(currentColor.rgb)) * (gLumDimSize - 1));
            int prevLumIdx = int(nomalize(luminance(prevColor.rgb)) * (gLumDimSize - 1));
            int currDepthIdx = int(nomalize(currentDepth.y) * (gDepthDimSize - 1));
            int DepthDiffIdx = int(nomalize(abs(currentDepth.x - prevDepth.x)) * (gDepthDimSize - 1));
            // Compute the index into the LUT
            int lutIdx = currLumIdx + prevLumIdx * gLumDimSize + DepthDiffIdx * gLumDimSize * gLumDimSize;
            // Get the antilag value from the LUT
            antilagAlpha = gAntilagLUTBuffer[lutIdx];
        }
    }
    if(isAccept){
        // Compute the alpha values for the temporal accumulation
        float alphaColor = max(gTemporalColorAlpha, 1.0 / (histLen + 1.0));
        float alphaMoments = max(gTemporalMomentsAlpha, 1.0 / (histLen + 1.0));
	    alphaColor   = lerp(alphaColor, 1.0, antilagAlpha);
	    alphaMoments = lerp(alphaMoments, 1.0, antilagAlpha);

        // TODO: Reuse the prevAccumColor around the current pixel before temporal accumulation
        psOut.accumulated.rgb   =   lerp(prevAccumColor.rgb, currentIllumination.rgb, alphaColor);
        psOut.accumulated.a     =   min(64, histLen + 1.0);
	    psOut.moments.rg        =   lerp(prevMoments.rg, currentMomemnts.rg, alphaMoments);
	    psOut.histLen.r         =   clamp(1.0 / alphaColor, 0.0, 64.0);
        psOut.gradDiffRatio.r   =   antilagAlpha;
        gCurrColorTexture[iPos] =   currentColor;
    #if IS_DEBUG_PASS
        gColorTest[iPos] = float4(antilagAlpha, 0, 0, 1);
    #endif
    }else{
        psOut.accumulated.xyzw  =   float4(currentIllumination.rgb, 1.0);
		psOut.moments.xy        =   currentMomemnts.xy;
		psOut.histLen.r         =   1.0;
        psOut.gradDiffRatio.r   =   antilagAlpha;
    }
    return psOut;
}
