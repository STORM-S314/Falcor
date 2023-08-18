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
	Texture2D                   gCurrentAccumColor;
    Texture2D                   gCurrentAccumMoments;
    Texture2D                   gCurrentAccumHistLen;
    //Texture2D                   gColor;
    Texture2D                   gLinearZTexture;
    Texture2D                   gNormalsTexture;
    Texture2D<PackedHitInfo>    gVisibilityBuffer;
};

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
	float4 psOut;

    float2 accumMoments = gCurrentAccumMoments[iPos].xy;
    float hisLen        = gCurrentAccumHistLen[iPos].x;
    float4 accumColor   = gCurrentAccumColor[iPos];

    const float hisLenThreshold = 4.0f;

    float3 colorSpatial     = accumColor.rgb;
	float2 momentsSpatial   = accumMoments;

    float2 centerDepth = gLinearZTexture[iPos].xy;
	if(centerDepth.x < 0.0) {
		return float4(accumColor.rgb, 0);
	}

    if(hisLen < hisLenThreshold)
    {
		float currentluminance = luminance(accumColor.rgb);
		momentsSpatial += float2(currentluminance, currentluminance * currentluminance);

        float3 centerNormals    =   gNormalsTexture[iPos].xyz;
        uint centerMeshID       =   getMeshInstanceID(HitInfo(gVisibilityBuffer[iPos]));

        float sumWeight = 1.0;
		const int radius = hisLen > 1 ? 2 : 3;
		for(int yy = -radius; yy <= radius; yy++) {
			for(int xx = -radius; xx <= radius; xx++) {
				if(xx != 0 || yy != 0) {
					int2 p = iPos + int2(xx, yy);
					float4 color_p = gCurrentAccumColor[p];

					float depth_p = gLinearZTexture[p].x;
					float3 normals_p = gNormalsTexture[p].xyz;

                    float luminance_p = luminance(color_p.rgb);

					float weightDepth = abs(depth_p - centerDepth.x) / (centerDepth.y * length(float2(xx, yy)) + 1e-2);
                    float weightNormals = pow(max(0, dot(normals_p, centerNormals)), 128.0); 

					uint meshID_p =  getMeshInstanceID(HitInfo(gVisibilityBuffer[p]));

					float weight = exp(- weightDepth) * weightNormals * (centerMeshID == meshID_p ? 1.0 : 0.0);

					if(isnan(weight))
                    {
						weight = 0.0;
                    }

					sumWeight += weight;

					momentsSpatial += float2(luminance_p, luminance_p * luminance_p) * weight;
					colorSpatial.rgb += color_p.rgb * weight;
				}
			}
		}

		momentsSpatial /= sumWeight;
		colorSpatial.rgb /= sumWeight;
		
		psOut = float4(colorSpatial.rgb, (1.0 + 3.0 * (1.0 - hisLen / hisLenThreshold)) * max(0.0, momentsSpatial.y - momentsSpatial.x * momentsSpatial.x));
    }
    else
    {
        float temporalVariance = max(0.0, accumMoments.y - accumMoments.x * accumMoments.x);
		psOut = float4(accumColor.rgb, temporalVariance);
    }

	return psOut;
}
