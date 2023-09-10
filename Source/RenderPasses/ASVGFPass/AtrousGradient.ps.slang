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
	Texture2D   gGradientLuminance;   // gradients :: luminance max, luminance differece, 1.0 or 0.0, 0.0 
    Texture2D   gGradientVariance;    // variance :: total luminance, variance, depth current.x, depth current.y
    float       gPhiColor;
    int         gStepSize;
    int         gGradientDownsample;
    float2      gGradientResDimensions;
    RWTexture2D<float4>         gColorTest;
};

struct PS_OUT
{
    float4 gradientLuminanceAtrous  : SV_TARGET0;
	float4 gradientVarianceAtrous   : SV_TARGET1;
};

float computeWeight(
    float depthCenter, float depthP, float phiDepth,
    float luminanceIllumCenter, float luminanceIllumP, float phiIllum)
{
    const float weightZ       = (phiDepth == 0) ? 0.0f : abs(depthCenter - depthP) / phiDepth;
    const float weightLillum  = abs(luminanceIllumCenter - luminanceIllumP) / phiIllum;

    const float weightIllum   = exp(0.0 - weightLillum * weightLillum - weightZ);

    return weightIllum;
}

float computeVarianceCenter(int2 ipos)
{
    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };
    float sum = gGradientVariance[ipos].g * kernel[0][0];

    const int radius = 1;
    for (int yy = -radius; yy <= radius; yy++)
    {
        for (int xx = -radius; xx <= radius; xx++)
        {
            if(xx != 0 || yy != 0)
            {
                const int2 p = ipos + int2(xx, yy);
                const float k = kernel[abs(xx)][abs(yy)];
                sum += gGradientVariance[p].g * k;
            }
        }
    }

    return sum;
}


PS_OUT main(FullScreenPassVsOut vsOut)
{
    PS_OUT psOut;
    int2 ipos = int2(vsOut.posH.xy);

    const float epsVariance      = 1e-10;
    //const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    const float4 color1 = gGradientLuminance[ipos].rgba;
    const float4 color2 = gGradientVariance[ipos].rgba;

    const float4 centerGradientDifference = color1.rgba;
    const float centerLuminance = color2.r;
    const float centerVariance = color2.g;

    const float computedVarianceCenter = computeVarianceCenter(ipos);

    const float2 zCenter = color2.ba;
    if (zCenter.x < 0)
    {
        // not a valid depth => must be envmap => do not filter
        psOut.gradientLuminanceAtrous   = color1;
        psOut.gradientVarianceAtrous    = color2;
        return psOut;
    }

    const float phiLIllumination    = gPhiColor * sqrt(max(0.0, epsVariance + computedVarianceCenter));
    const float phiDepth            = max(zCenter.y, 1e-8) * gGradientDownsample;

    float   sumWIllumination        = 1.0;
    float4  sumGradientDifference   = centerGradientDifference;
    float   sumVariance             = centerVariance;
    float   sumLuminance            = centerLuminance;

    for (int yy = -1; yy <= 1; yy++)
    {
        for (int xx = -1; xx <= 1; xx++)
        {
            const int2 p        = ipos + int2(xx, yy) * gStepSize;
            const bool inside   = isPixelWithinScreenDimensions(p, gGradientResDimensions);

            const float kernel = 1.0;//kernelWeights[abs(xx)] * kernelWeights[abs(yy)];

            if (inside && (xx != 0 || yy != 0)) // skip center pixel, it is already accumulated
            {
                const float4 currentGradientVarianceP = gGradientVariance[p].rgba;
                const float luminanceP  =   currentGradientVarianceP.r;
                const float zP          =   currentGradientVarianceP.b;
                const float varianceP   =   currentGradientVarianceP.g;
                const float4 currentGradientDifferenceP = gGradientLuminance[p].rgba;

                // compute the edge-stopping functions
                const float w = computeWeight(
                    zCenter.x, zP, phiDepth * length(float2(xx, yy) * float2(gStepSize, gStepSize)),
                    centerLuminance, luminanceP, phiLIllumination);

                const float wIllumination = w * kernel;

                // alpha channel contains the variance, therefore the weights need to be squared, see paper for the formula
                sumWIllumination        +=  wIllumination;
                sumGradientDifference   +=  wIllumination.xxxx * currentGradientDifferenceP;
                sumVariance             +=  wIllumination * wIllumination * varianceP;
                sumLuminance            +=  wIllumination * luminanceP;
            }
        }
    }

    psOut.gradientLuminanceAtrous   = sumGradientDifference / sumWIllumination;
    psOut.gradientVarianceAtrous    = float4(sumLuminance, sumVariance, zCenter.r, zCenter.g) / float4(sumWIllumination.x, sumWIllumination.x * sumWIllumination.x, 1.0, 1.0);

    return psOut;
}
