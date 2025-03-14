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
import Scene.HitInfo;
#include "ASVGFCommon.slang"
#include "ASVGFUtils.slang"

cbuffer PerImageCB
{
    Texture2D                   gColor;
    Texture2D                   gAlbedo;
    Texture2D                   gEmission;
    Texture2D                   gGradientSample;
    Texture2D                   gLinearZTexture;
    Texture2D                   gNormalsTexture;
    Texture2D<PackedHitInfo>    gVisibilityBuffer;
	Texture2D<PackedHitInfo>    gPrevVisibilityBuffer;
    Texture2D                   gGradLuminanceDiff;
};

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
    float4 gradColor =  ((iPos.x % 3) == 0 && (iPos.y % 3) == 0) ? gGradLuminanceDiff[iPos / 3] : float4(1.0, 0.0, 0.0, 1.0);

    //return float4(abs(gGradLuminanceDiff[iPos / 3].g) / gGradLuminanceDiff[iPos / 3].r ,0.0, 0.0, 1.0);
    return float4(gColor[iPos]);
    //HitInfo currentHitInfo         = HitInfo(gVisibilityBuffer[iPos]);
    //float   currentMeshID          = float(getMeshInstanceID(currentHitInfo));
    //float   currentHitInfoValid    = float(currentHitInfo.isValid() ? 1 : 0);
    //return float4(currentMeshID, currentHitInfoValid, 0, 0);
	
}
