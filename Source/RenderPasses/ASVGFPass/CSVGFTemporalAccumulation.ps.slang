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

float tanh(float x){
    float exp2x = exp(2.0 * x);
    return (exp2x - 1.0) / (exp2x + 1.0);
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
    float   currentLuminance        = tanh(luminance(currentColor.rgb));
    float2  currentMomemnts         = float2(currentLuminance, currentLuminance * currentLuminance);
    HitInfo currentHitInfo          = HitInfo(gVisibilityBuffer[iPos]);
	uint    currentMeshID           = getMeshInstanceID(currentHitInfo);
    float3  currentNormal           = gNormalsTexture[iPos].xyz;

    float4  prevColor               = gPrevColorTexture[iPrevPos].xyzw;
    float4  prevAccumColor          = gPrevAccumColorTexture[iPrevPos].xyzw;
    float3  prevIllumination        = getIllumination(prevColor.xyz, gPrevAlbedoTexture[iPrevPos].rgb, gPrevSpecularAlbedo[iPrevPos].rgb, gEmissionTexture[iPrevPos].rgb);
    float   prevLuminance           = tanh(luminance(prevColor.rgb));
    float2  prevMoments             = gPrevAccumMomentsTexture[iPrevPos].xy;
    float2  prevDepth               = gPrevLinearZTexture[iPrevPos].xy;
    float3  prevNormals             = gPrevNormalsTexture[iPrevPos].xyz;

    float antilagAlpha = 1.0;
    // Quantize
    int currLumIdx = int(currentLuminance * (gLumDimSize - 1));
    int prevLumIdx = int(prevLuminance * (gLumDimSize - 1));
    int currDepthIdx = int(tanh(currentDepth.y) * (gDepthDimSize - 1));
    int DepthDiffIdx = int(tanh(abs(currentDepth.x - prevDepth.x) / (currentDepth.y + 1e-2f)) * (gDepthDimSize - 1));
    // Compute the index into the LUT
    int lutIdx = currLumIdx + prevLumIdx * gLumDimSize + DepthDiffIdx * gLumDimSize * gLumDimSize;
    // Get the antilag value from the LUT
    antilagAlpha = 1.0 - pow(gAntilagLUTBuffer[lutIdx],0.1);
	float alphaColor   = lerp(gTemporalColorAlpha, 1.0, antilagAlpha);
	float alphaMoments = lerp(gTemporalMomentsAlpha, 1.0, antilagAlpha);

    // TODO: Reuse the prevAccumColor around the current pixel before temporal accumulation
    psOut.accumulated.xyzw  =   float4(lerp(prevAccumColor.rgb, currentIllumination.rgb, alphaColor), 1.0);
	psOut.moments.rg        =   lerp(prevMoments.rg, currentMomemnts.rg, alphaMoments);
	psOut.histLen.r         =   1.0;
    psOut.gradDiffRatio.r   =   antilagAlpha;
    gCurrColorTexture[iPos] =   currentColor;
#if IS_DEBUG_PASS
    gColorTest[iPos] = float4(antilagAlpha, 0, 0, 1);
#endif
    return psOut;
}
