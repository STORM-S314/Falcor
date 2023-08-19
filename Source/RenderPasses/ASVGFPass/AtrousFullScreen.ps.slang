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

cbuffer PerImageCB
{
	Texture2D   gColorAndVariance;
    Texture2D   gLinearZTexture;
    Texture2D   gNormalsTexture;
    Texture2D   gAlbedoTexture;
    Texture2D   gEmissionTexture;
	int 		gStepSize;
	int 		gIsModulateAlbedo;
};

const float gaussian_kernel[3][3] = {
	{ 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 },
	{ 1.0 / 8.0,  1.0 / 4.0, 1.0 / 8.0  },
	{ 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 }
};

static int2 ipos                = int2(0, 0);
static float centerLuminance    = 0.0;
static float sigmaLuminance     = 0.0;
static float2 centerZ           = float2(0.0);
static float3 centerNormal      = float3(0.0);

static float3 sumColor          = float3(0.0);
static float sumVariance        = 0.0;
static float sumWeight          = 1.0;

float compute_sigma_luminance(float centerVariance, int2 ipos)
{
	const int r = 1;

	float sum = centerVariance * gaussian_kernel[0][0];

	for(int yy = -r; yy <= r; yy++) {
		for(int xx = -r; xx <= r; xx++) {
			if(xx != 0 || yy != 0) {
				int2 p = ipos + int2(xx, yy);
				float v = gColorAndVariance[p].a;
				float w = gaussian_kernel[xx + 1][yy + 1];
				sum += v * w;
			}
		}
	}

	return sqrt(max(sum, 0.0));
}

void tap(int2 offset, float kernel_weight)
{
	int2 p = ipos + offset; 

	float4 color_p      = gColorAndVariance[p].xyzw; 
	float3 normal_p     = gNormalsTexture[p].xyz; 
	float z_p           = gLinearZTexture[p].x;
	float l_p           = luminance(color_p.rgb); 

	float w_l = abs(l_p - centerLuminance) / (sigmaLuminance + 1e-10); 
	float w_z = 3.0 * abs(z_p - centerZ.x) / (centerZ.y * length(float2(offset) * gStepSize) + 1e-2); 
	float w_n = pow(max(0, dot(normal_p, centerNormal)), 128.0); 

	float w = exp(-w_l * w_l - w_z) * kernel_weight * w_n; 

	sumColor    += color_p.rgb * w; 
	sumVariance += w * w * color_p.a; 
	sumWeight   += w; 
}

void box3()
{
	const int r = 1;
	for(int yy = -r; yy <= r; yy++) {
		for(int xx = -r; xx <= r; xx++) {
			if(xx != 0 || yy != 0) {
				tap(int2(xx, yy) * int2(gStepSize, gStepSize), 1.0);
			}
		}
	}
}

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 ipos = int2(vsOut.posH.xy);

    centerLuminance = luminance(gColorAndVariance[ipos].rgb);
    sigmaLuminance  = compute_sigma_luminance(gColorAndVariance[ipos].a, ipos);
    centerZ         = gLinearZTexture[ipos].xy;
    centerNormal    = gNormalsTexture[ipos].xyz;
    sumColor        = gColorAndVariance[ipos].rgb;
    sumVariance     = gColorAndVariance[ipos].a;

	if(gLinearZTexture[ipos].x > 0.0) { /* only filter foreground pixels */
        box3();
    }

	sumColor    /= sumWeight;
	sumVariance /= (sumWeight * sumWeight);

	float4 fragColor  = float4(sumColor, sumVariance);

	if(gIsModulateAlbedo == 1) {
		fragColor.rgb = (fragColor.rgb * gAlbedoTexture[ipos].rgb) + gEmissionTexture[ipos].rgb;
	}
    return fragColor;
}
