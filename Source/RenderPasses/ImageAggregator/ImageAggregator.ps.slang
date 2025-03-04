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
struct FullScreenPassVsOut
{
    float2 texC       : TEXCOORD;
    float4 posH       : SV_POSITION;
};

cbuffer PerImageCB
{
    Texture2D           gColor;
    RWBuffer<float>     gAccumulationBuffer;
    int                 gInputImageIndex;
    float2              gImageDimensions;
    uint               gPixelsInFrame;
};

float4 main(FullScreenPassVsOut vsOut) : SV_TARGET0
{
    int2 iPos = int2(vsOut.posH.xy);
    float4 colorResult = float4(0);

    int currentBufferIndex = 3 * ((iPos.y * gImageDimensions.x + iPos.x));
    gAccumulationBuffer[currentBufferIndex + 0] += gColor[iPos].r;
    gAccumulationBuffer[currentBufferIndex + 1] += gColor[iPos].g;
    gAccumulationBuffer[currentBufferIndex + 2] += gColor[iPos].b;

    if(gInputImageIndex == (AGGREGATION_IMAGE_COUNT - 1))
    {
        colorResult.rgb = float3(gAccumulationBuffer[currentBufferIndex + 0], gAccumulationBuffer[currentBufferIndex + 1], gAccumulationBuffer[currentBufferIndex + 2]) / (float(AGGREGATION_IMAGE_COUNT)).xxx;
        gAccumulationBuffer[currentBufferIndex + 0] = 0.0;
        gAccumulationBuffer[currentBufferIndex + 1] = 0.0;
        gAccumulationBuffer[currentBufferIndex + 2] = 0.0;
    }

    return float4(colorResult.rgb, 1.0);
	
}
