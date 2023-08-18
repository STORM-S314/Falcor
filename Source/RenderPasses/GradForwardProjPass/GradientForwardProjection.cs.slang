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

import Utils.Math.MathHelpers;
import Scene.HitInfo;
#include "GradForwardProjCommon.slang"

cbuffer PerImageCB {
	Texture2D                   gPrevRandomNumberTexture;
	RWTexture2D<uint>           gCurrentRandomNumberTexture;
	Texture2D 	                gLinearZTexture;
    Texture2D 	                gPrevLinearZTexture;
    Texture2D 	                gWNormalTexture;
    Texture2D 	                gPrevWNormalTexture;
    RWTexture2D<PackedHitInfo>  gVisibilityBuffer;
    Texture2D<PackedHitInfo> 	gPrevVisibilityBuffer;
    RWTexture2D<uint>  	        gGradientSamplesTexture;
    Texture2D 	                gPosTexture;
    Texture2D 	                gPrevWPosTexture;
    Texture2D 	                gPositionNormalFwidth;
    float4x4                    gViewMat;
    float4x4                    gProjMat;
    float4x4                    gViewProjMat;
    float4x4                    gPrevViewProjMat;
	uint 		                gFrameNumber;
    float                       gTextureWidth;
    float                       gTextureHeight;
    uint                        gGradientDownsample;
    RWTexture2D<float4>         gColorTestTexture;
};



uint get_gradient_idx_from_tile_pos(int2 pos)
{
	return (1 << 31) | (pos.x) | (pos.y << TILE_OFFSET_SHIFT);
}

void encrypt_tea(inout uint2 arg)
{
    const uint key[] = {0xa341316c, 0xc8013ea4, 0xad90777d, 0x7e95761e};
    uint v0 = arg[0], v1 = arg[1];
    uint sum = 0;
    uint delta = 0x9e3779b9;

    for(int i = 0; i < 16; i++) { // XXX rounds reduced, carefully check if good
    //for(int i = 0; i < 32; i++) {
            sum += delta;
            v0 += ((v1 << 4) + key[0]) ^ (v1 + sum) ^ ((v1 >> 5) + key[1]);
            v1 += ((v0 << 4) + key[2]) ^ (v0 + sum) ^ ((v0 >> 5) + key[3]);
    }
    arg[0] = v0;
    arg[1] = v1;
}

void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    //Get a random index from the previous frame within the stratum (gGradientDownsample X gGradientDownsample) that is selected to be forward projected to the next frame
	// here arg is the position within the stratum
    uint2 ipos = uint2(dispatchThreadId.xy);
    int2 idx_prev;
	{
		uint2 arg = uint2(ipos.x + ipos.y * uint(gTextureWidth), gFrameNumber);
		encrypt_tea(arg);
		arg %= gGradientDownsample;
		idx_prev = int2(ipos * gGradientDownsample + arg);
	}

    precise float4 posWCurrent = mul(gViewProjMat, float4(gPrevWPosTexture[idx_prev].xyz, 1.0));
    
    if(any(posWCurrent.xyz <= -posWCurrent.www)
	|| any(posWCurrent.xyz >= posWCurrent.www))
	{
		return;
	}

    posWCurrent.xyz /= posWCurrent.w;
	posWCurrent.xyz *= 0.5;
	posWCurrent.xyz += 0.5;

    posWCurrent.y = 1.0 - posWCurrent.y;
    const float2 imageDim = float2(gTextureWidth, gTextureHeight);

    int2 idx_curr = int2((posWCurrent.xy * imageDim.xy));

    //check whether reprojected pixel is inside of the screen
    if (any(idx_curr < int2(1,1)) || any(idx_curr > (imageDim - int2(1,1)))){ return; }
    
    // check if deviation of depths is acceptable
    float currentDepth = gLinearZTexture[idx_curr].x;
    float prevDepth = gPrevLinearZTexture[idx_prev].x;
    float fwidthZ = gLinearZTexture[idx_curr].y;
    if (abs(prevDepth - currentDepth) / (fwidthZ + 1e-2f) > 10.f) return;
    
    // check normals for compatibility
    float3 currentNormal = gWNormalTexture[idx_curr].xyz;
    float3 prevNormal = gPrevWNormalTexture[idx_prev].xyz;
    float posNormalFWidth = gPositionNormalFwidth[idx_curr].y;
    if (distance(currentNormal, prevNormal) / (posNormalFWidth + 1e-2) > 16.0) return;
    
    int2 tile_pos_curr = idx_curr / gGradientDownsample;
	uint gradient_idx_curr = get_gradient_idx_from_tile_pos(idx_curr % gGradientDownsample);
    
	/* encode position in previous frame */
	gradient_idx_curr |= (idx_prev.x + idx_prev.y * uint(gTextureWidth)) << (2 * TILE_OFFSET_SHIFT);
    
    uint outputValue = 1u;
	InterlockedCompareExchange(gGradientSamplesTexture[ipos], 0u, gradient_idx_curr, outputValue);

    if(outputValue == 0u)
    {
        gCurrentRandomNumberTexture[idx_curr].x = gPrevRandomNumberTexture[idx_prev].x;
        gVisibilityBuffer[idx_curr].xyzw = gPrevVisibilityBuffer[idx_prev].xyzw;
    }
    
}
