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
#include "ASVGFCommon.slang"

cbuffer PerImageCB
{
	Texture2D gGradientLuminance;
    Texture2D gGradientVariance;
    int gStepSize;
    int gGradientDownsample;
};

static float sigmaLuminance    = 0.0;
static int2 ipos               = int2(0, 0);
static float centerLuminance   = 0.0;
static float2 centerDepth      = float2(0.0, 0.0);
static float4 sumColor         = float4(0.0, 0.0, 0.0, 0.0);
static float sumLuminance      = 0.0;
static float sumVariance       = 0.0;
static float sumWeight         = 0.0;

struct PS_OUT
{
    float4 gradientLuminanceAtrous  : SV_TARGET0;
	float4 gradientVarianceAtrous  : SV_TARGET1;
};

const float gaussianKernel[3][3] = {
	{ 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 },
	{ 1.0 / 8.0,  1.0 / 4.0, 1.0 / 8.0  },
	{ 1.0 / 16.0, 1.0 / 8.0, 1.0 / 16.0 }
};

float computeSigmaLuminance(float center, int2 pos)
{
	const int r = 1;

	float sum = center * gaussianKernel[0][0];

	for(int yy = -r; yy <= r; yy++) {
		for(int xx = -r; xx <= r; xx++) {
			if(xx != 0 || yy != 0) {
				int2 p = pos + int2(xx, yy);
				float v = gGradientVariance[p].y;
				float w = gaussianKernel[xx + 1][yy + 1];
				sum += v * w;
			}
		}
	}

	return sqrt(max(sum, 0.0));
}

void tap(int2 offset, float kernelWeight)
{
	int2 pos = ipos + offset; 

	float4 color1_p   = gGradientLuminance[pos]; 
	float4 color2_p   = gGradientVariance[pos]; 
	float currentDepth  = color2_p.z;
	float currentLuminance  = color2_p.x;

	float weightLuminance = abs(currentLuminance - centerLuminance) / (sigmaLuminance + 1e-10); 
	float weightDepth = abs(currentDepth - centerDepth.x) / (centerDepth.y * length(float2(offset) * gStepSize * gGradientDownsample) + 1e-2); 

	float weight = exp(-weightLuminance * weightLuminance - weightDepth) * kernelWeight; 

	sumColor     += color1_p * weight; 
	sumLuminance += currentLuminance * weight;
	sumVariance  += weight * weight * color2_p.y; 
	sumWeight    += weight; 
}

void box3()
{
	const int r = 1;
	for(int yy = -r; yy <= r; yy++) {
		for(int xx = -r; xx <= r; xx++) {
			if(xx != 0 || yy != 0) {
				tap(int2(xx, yy) * gStepSize, 1.0);
			}
		}
	}
}

PS_OUT main(FullScreenPassVsOut vsOut)
{
    PS_OUT psOut;

    ipos = int2(vsOut.posH.xy);

    float4 centerGradient1  = gGradientLuminance[ipos];
    float4 centerGradient2  = gGradientVariance[ipos];

    centerDepth = centerGradient2.ba;
    centerLuminance = centerGradient2.r;
    sigmaLuminance  = computeSigmaLuminance(centerGradient2.g, ipos) * 3.0;

    sumColor       = centerGradient1;
    sumLuminance   = centerGradient2.r;
    sumVariance    = centerGradient2.g;
    sumWeight      = 1.0;

    if(centerDepth.x > 0) { /* only filter foreground pixels */
        box3();
	}

    sumColor     /= sumWeight;
	sumLuminance /= sumWeight;
	sumVariance  /= sumWeight * sumWeight;

	psOut.gradientLuminanceAtrous   = sumColor;
	psOut.gradientVarianceAtrous    = float4(sumLuminance, sumVariance, centerDepth);

    return psOut;
}
