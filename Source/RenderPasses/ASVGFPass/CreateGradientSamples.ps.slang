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
import ColorSpace;
#include "ASVGFCommon.slang"
#include "ASVGFUtils.slang"

cbuffer PerImageCB
{
	Texture2D                   gColorTexture;
	Texture2D                   gPrevColorTexture;
	Texture2D                   gAlbedoTexture;
	Texture2D                   gPrevAlbedoTexture;
    Texture2D                   gEmissionTexture;
	Texture2D                   gPrevEmissionTexture;
	Texture2D<uint>             gGradientSamples;
    Texture2D<PackedHitInfo>    gVisibilityBuffer;
	Texture2D                   gLinearZTexture;
	int                         gGradientDownsample;
    int                         gScreenWidth;
    RWTexture2D<float4>         gColorTest;
};

struct PS_OUT
{
    float4 fragGradientLuminance    : SV_TARGET0;
	float4 fragGradientVariance     : SV_TARGET1;
};

PS_OUT main(FullScreenPassVsOut vsOut)
{
	PS_OUT psOut;
    
    int2 posGrad = int2(vsOut.posH.xy);
	uint gradientSample = uint(gGradientSamples[posGrad]);
    
	//The position of the texel in the tile
	int2 posInTile = int2((gradientSample & TILE_OFFSET_MASK), (gradientSample >> TILE_OFFSET_SHIFT) & TILE_OFFSET_MASK);
    
	int2 currentPos = posGrad * gGradientDownsample + posInTile;

    float3 currentIllumination = getIllumination(gColorTexture[currentPos].rgb, gEmissionTexture[currentPos].rgb, gAlbedoTexture[currentPos].rgb);
	float currentLuminance = luminance(currentIllumination);
    
	if(gradientSample >= (1u << 31)) {
	    uint idxPrev = (gradientSample >> (2 * TILE_OFFSET_SHIFT)) & ((1 << (31 - 2 * TILE_OFFSET_SHIFT)) - 1);
    
		int2 prevPos = int2(idxPrev % gScreenWidth, idxPrev / gScreenWidth);

        float3 prevIllumination = getIllumination(gPrevColorTexture[prevPos].rgb, gPrevEmissionTexture[prevPos].rgb, gPrevAlbedoTexture[prevPos].rgb);
		float prevLuminance = luminance(prevIllumination);
    
		psOut.fragGradientLuminance.r = max(currentLuminance, prevLuminance);
		psOut.fragGradientLuminance.g = (currentLuminance - prevLuminance);
		psOut.fragGradientLuminance.b = 1.0;
		psOut.fragGradientLuminance.a = 0.0;
	}
	else {
		psOut.fragGradientLuminance.rgba = float4(0);
	}
    
	float2 moments = float2(currentLuminance, currentLuminance * currentLuminance);
	float sumWeight = 1.0;
	float2 currentDepth = gLinearZTexture[currentPos].rg;

    HitInfo hitInf = HitInfo(gVisibilityBuffer[currentPos]);
    if(hitInf.isValid())
    {
	    uint meshID  = getMeshInstanceID(hitInf);
    
        for(int yy = 0; yy < gGradientDownsample; yy++) {
		    for(int xx = 0; xx < gGradientDownsample; xx++) {
			    int2 indexPos = posGrad * gGradientDownsample + int2(xx, yy);
			    if(!all(currentPos == indexPos)) {
				    float3 indexColor       = gColorTexture[indexPos].rgb;
                    float3 indexEmission    = gEmissionTexture[indexPos].rgb;
                    float3 indexAlbedo      = gAlbedoTexture[indexPos].rgb;

				    uint indexMeshID = getMeshInstanceID(HitInfo(gVisibilityBuffer[indexPos]));

                    float3 indexIllumination = getIllumination(indexColor, indexEmission, indexAlbedo);
				    float indexLuminance = luminance(indexIllumination);
    
				    float indexWeight = indexMeshID == meshID ? 1.0 : 0.0;
    
				    moments += float2(indexLuminance, indexLuminance * indexLuminance) * indexWeight;
				    sumWeight += indexWeight;
			    }
		    }
	    }
    }
	moments /= sumWeight;
    
	float variance = max(0.0, moments[1] - moments[0] * moments[0]);
    
	psOut.fragGradientVariance.r  = moments[0];
	psOut.fragGradientVariance.g  = variance;
	psOut.fragGradientVariance.ba = currentDepth;
    ////gColorTest[currentPos] = float4(psOut.fragGradientLuminance.g, 0.0, 0.0, 1.0);
	return psOut;
}
