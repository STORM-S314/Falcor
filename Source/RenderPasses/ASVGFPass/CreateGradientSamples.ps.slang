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
//import Utils.Math.MathHelpers;
//import Utils.Color.ColorHelpers;

#define TILE_OFFSET_SHIFT 3u
#define TILE_OFFSET_MASK ((1 << TILE_OFFSET_SHIFT) - 1)

import ColorSpace;
import ASVGFShared;


cbuffer PerImageCB
{
	Texture2D tex_color_unfiltered;
	Texture2D tex_gradient_samples;
	Texture2D tex_color_unfiltered_prev;
	Texture2D tex_z;
	Texture2D tex_vbuf;
	int gradientDownsample;
};

struct PS_OUT
{
    float4 frag_color1  : SV_TARGET0;
	float4 frag_color2  : SV_TARGET1;
};

PS_OUT main(FullScreenPassOut vsOut)
{
	PS_OUT psOut;
    
	const float4 posH = vsOut.posH;
    const int2 ipos_grad = posH.xy;
	//ivec2 ipos_grad = ivec2(gl_FragCoord);

	//uint u = texelFetch(tex_gradient_samples, ipos_grad, 0).r;
	uint u = tex_gradient_samples[ipos_grad].x;


	//The position of the texel in the tile
	int2 tile_pos = int2((u & TILE_OFFSET_MASK), (u >> TILE_OFFSET_SHIFT) & TILE_OFFSET_MASK);
    
	int2 ipos = ipos_grad * gradientDownsample + tile_pos;
	//float l_curr = luminance(texelFetch(tex_color_unfiltered,      ipos,      0).rgb);
	float l_curr = luminance(tex_color_unfiltered[ipos].rgb);

	
	if(u >= (1u << 31)) {
		uint idx_prev = (u >> (2 * TILE_OFFSET_SHIFT)) & ((1 << (31 - 2 * TILE_OFFSET_SHIFT)) - 1);

		int w = getTextureDims(tex_color_unfiltered, 0).r;
		int2 ipos_prev = int2(idx_prev % w, idx_prev / w);

		float l_prev = luminance(tex_color_unfiltered_prev[ipos_prev].rgb);

		psOut.frag_color1.r = max(l_curr, l_prev);
		psOut.frag_color1.g = (l_curr - l_prev);
		psOut.frag_color1.b = 1.0;
		psOut.frag_color1.a = 0.0;
	}
	else {
		psOut.frag_color1.rgba = float4(0);
	}

	float2 moments = float2(l_curr, l_curr * l_curr);
	float sum_w = 1.0;
	float2 z_curr = tex_z[ipos].rg;
	uint mesh_id  = asuint(tex_vbuf[ipos].r);
    for(int yy = 0; yy < gradientDownsample; yy++) {
		for(int xx = 0; xx < gradientDownsample; xx++) {
			int2 p = ipos_grad * gradientDownsample + int2(xx, yy);
			if(!all(ipos == p)) {
				float3 rgb = tex_color_unfiltered[p].rgb;
				uint mesh_id_p = asuint(tex_vbuf[p].r);

				float l = luminance(rgb);

				float w = mesh_id_p == mesh_id ? 1.0 : 0.0;

				moments += float2(l, l * l) * w;
				sum_w += w;
			}
		}
	}
	moments /= sum_w;

	float variance = max(0.0, moments[1] - moments[0] * moments[0]);

	psOut.frag_color2.r  = moments[0];
	psOut.frag_color2.g  = variance;
	psOut.frag_color2.ba = z_curr;
	
	return psOut;
}
