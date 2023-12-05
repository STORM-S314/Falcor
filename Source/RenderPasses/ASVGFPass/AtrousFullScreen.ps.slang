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
    Texture2D   gSpecularAlbedo;
    Texture2D   gIndirectAlbedo;
    float       gPhiColor;
    float       gPhiNormal;
	int 		gStepSize;
	int 		gIsModulateAlbedo;
    int2        gScreenDimension;
    bool        gIsUseMutualInf;
    Texture2D   gGradDifferenceRatio;
#if IS_DEBUG_PASS
    RWTexture2D<float4>         gColorTest;
#endif
};

float computeWeight(
    float depthCenter, float depthP, float phiDepth,
    float3 normalCenter, float3 normalP, float phiNormal,
    float luminanceIllumCenter, float luminanceIllumP, float phiIllum)
{
    const float weightNormal  = pow(saturate(dot(normalCenter, normalP)), phiNormal);
    const float weightZ       = 3.0 * abs(depthCenter - depthP) / phiDepth;
    const float weightLillum  = abs(luminanceIllumCenter - luminanceIllumP) / phiIllum;

    const float weightIllum   = exp((gIsUseMutualInf ? (-weightLillum) : (-weightLillum * weightLillum)) - weightZ) * weightNormal;

    return weightIllum;
}

float computeVarianceCenter(int2 ipos)
{
    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };

    float sum = gColorAndVariance[ipos].a * kernel[0][0];

    const int radius = 1;
    for (int yy = -radius; yy <= radius; yy++)
    {
        for (int xx = -radius; xx <= radius; xx++)
        {
            if(xx != 0 || yy != 0)
            {
                const int2 p = ipos + int2(xx, yy);
                const float k = kernel[abs(xx)][abs(yy)];
                sum += gColorAndVariance[p].a * k;
            }
        }
    }

    return sum;
}

float computeMutualInfCenter(int2 ipos)
{
    const float kernel[2][2] = {
        { 1.0 / 4.0, 1.0 / 8.0  },
        { 1.0 / 8.0, 1.0 / 16.0 }
    };

    float sum = gColorAndVariance[ipos].a * kernel[0][0];

    const int radius = 1;
    for (int yy = -radius; yy <= radius; yy++)
    {
        for (int xx = -radius; xx <= radius; xx++)
        {
            if(xx != 0 || yy != 0)
            {
                const int2 p = ipos + int2(xx, yy);
                const float k = kernel[abs(xx)][abs(yy)];
                sum += gColorAndVariance[p].a * k;
            }
        }
    }

    return sum;
}


float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 ipos = int2(vsOut.posH.xy);

    const float epsVariance      = 1e-10;
    //const float kernelWeights[3] = { 1.0, 2.0 / 3.0, 1.0 / 6.0 };

    const float4 illuminationCenter = gColorAndVariance[ipos].rgba;
    const float lIlluminationCenter = luminance(illuminationCenter.rgb);

    //const float var = computeVarianceCenter(ipos);

    const float2 zCenter = gLinearZTexture[ipos].xy;
    if (zCenter.x < 0.0)
    {
        // not a valid depth => must be envmap => do not filter
        return illuminationCenter;
    }

    const float3 nCenter = gNormalsTexture[ipos].xyz;

    const float phiLIllumination   = gPhiColor * (gIsUseMutualInf ? (1.0 / (epsVariance + illuminationCenter.a)) : sqrt(max(0.0, epsVariance + computeVarianceCenter(ipos))));
    const float phiDepth     = max(zCenter.y, 1e-8);

    float sumWIllumination   = 1.0;
    float4  sumIllumination  = illuminationCenter;

    const float radius = 1;
    for (int yy = -radius; yy <= radius; yy++)
    {
        for (int xx = -radius; xx <= radius; xx++)
        {
            const int2 p     = ipos + int2(xx, yy) * int2(gStepSize, gStepSize);
            const bool inside = isPixelWithinScreenDimensions(p, gScreenDimension);

            const float kernel = 1.0;//gIsUseMutualInf ? kernelWeights[abs(xx)] * kernelWeights[abs(yy)] : 1.0;

            if (inside && (xx != 0 || yy != 0)) // skip center pixel, it is already accumulated
            {
                const float4 illuminationP = gColorAndVariance[p].rgba;
                const float lIlluminationP = luminance(illuminationP.rgb);
                const float zP = gLinearZTexture[p].x;
                const float3 nP = gNormalsTexture[p].xyz;

                // compute the edge-stopping functions
                const float w = computeWeight(
                    zCenter.x, zP, phiDepth * length(float2(xx, yy) * float2(gStepSize, gStepSize) * float2(gStepSize, gStepSize)),
                    nCenter, nP, gPhiNormal,
                    lIlluminationCenter, lIlluminationP, phiLIllumination);

                const float wIllumination = w * kernel;

                // alpha channel contains the variance, therefore the weights need to be squared, see paper for the formula
                sumWIllumination  += wIllumination;
                sumIllumination   += float4(wIllumination.xxx, gIsUseMutualInf ? 0.0 : wIllumination * wIllumination) * illuminationP;
            }
        }
    }

    float4 filteredIllumination = float4(sumIllumination / float4(sumWIllumination.xxx, gIsUseMutualInf ? 1.0 : sumWIllumination * sumWIllumination));

    filteredIllumination  = gIsUseMutualInf ? float4(filteredIllumination.rgb, illuminationCenter.a * sumWIllumination) : filteredIllumination;

    if(gIsModulateAlbedo == 1)
    {
        filteredIllumination.rgba = float4(((filteredIllumination.rgb) * (gAlbedoTexture[ipos].rgb + gSpecularAlbedo[ipos].rgb + gEmissionTexture[ipos].rgb)), 1.0);
	}

    return filteredIllumination;
}
