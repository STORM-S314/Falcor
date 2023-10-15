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
 **************************************************************************/
import ColorSpace;

#ifndef TILE_OFFSET_SHIFT
    #define TILE_OFFSET_SHIFT 3u
#endif

#ifndef TILE_OFFSET_MASK 
    #define TILE_OFFSET_MASK ((1 << TILE_OFFSET_SHIFT) - 1)
#endif

struct FullScreenPassVsOut
{
    float2 texC       : TEXCOORD;
    float4 posH       : SV_POSITION;
};

float3 getIllumination(float3 a_Color, float3 a_Albedo, float3 a_SpecularAlbedo)
{
    return (a_Color.rgb ) / (max(float3(0.001, 0.001, 0.001), a_Albedo.rgb + a_SpecularAlbedo));
}

bool isPixelWithinScreenDimensions(float2 pixelPos, float2 screenDimensions)
{
    return (all(pixelPos >= int2(0,0)) && all(pixelPos <= (screenDimensions.xy - int2(1,1))));
}

bool isReprojectionValid(float2 pixelPos, float2 screenDimensions, float2 currentDepth, float2 prevDepth, float3 currentNormal, float3 prevNormal)
{
    // check whether reprojected pixel is inside of the screen
    if(!isPixelWithinScreenDimensions(pixelPos, screenDimensions)){return false;}

    // check if deviation of depths is acceptable
    if((abs(prevDepth.x - currentDepth.x) / (currentDepth.y + 1e-2f)) > 10.f) {return false;}

    // check normals for compatibility
    if (dot(currentNormal, prevNormal) < 0.9) {return false;}

    return true;
}

int2 getTextureDims(Texture2D tex, uint mip)
{
    uint w, h;
    tex.GetDimensions(w, h);
    return int2(w,h);
}

bool test_reprojected_normal(float3 n1, float3 n2)
{
	return dot(n1, n2) > 0.95;
}

bool test_inside_screen(int2 p, int2 res)
{
	return all(p >= int2(0,0))
		&& all(p < res);
}

bool test_reprojected_depth(float z1, float z2, float dz)
{
	float z_diff = abs(z1 - z2);
	return z_diff < 2.0 * (dz + 1e-3);
}

int2 get_gradient_tile_pos(uint idx, int gradientDownsample)
{
	/* didn't store a gradient sample in the previous frame, this creates
	   a new sample in the center of the tile */
	if(idx < (1u<<31))
		return int2(gradientDownsample / 2);

	return int2((idx & TILE_OFFSET_MASK), (idx >> TILE_OFFSET_SHIFT) & TILE_OFFSET_MASK);
}

uint get_gradient_idx_from_tile_pos(int2 pos)
{
	return (1 << 31) | (pos.x) | (pos.y << TILE_OFFSET_SHIFT);
}

bool is_gradient_sample(Texture2D tex_gradient, int2 ipos, int gradientDownsample)
{
	int2 ipos_grad = ipos / gradientDownsample;
	uint u = tex_gradient[ipos_grad].r;

	int2 tile_pos = int2((u & TILE_OFFSET_MASK), (u >> TILE_OFFSET_SHIFT) & TILE_OFFSET_MASK);
	return (u >= (1u << 31) && all(ipos_grad * gradientDownsample + tile_pos == ipos));
}

// Workaround for isnan() not working in slang.
bool isNaN(float f)
{
    uint u = asuint(f) & ~0x80000000u; // clear out the sign bit
    return (u > 0x7F800000);           // greater than Inf is NaN
}
