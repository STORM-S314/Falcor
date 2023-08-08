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

cbuffer PerImageCB {
	Texture2D           gPrevRandomNumberTexture;
	Texture2D           gCurrentRandomNumberTexture : SV_TARGET1;
	Texture2D 	        gMotionVectors;
	Texture2D 	        gLinearZAndNormalTexture;
    Texture2D 	        gPrevLinearZAndNormalTexture;
    Texture2D 	        gVisibilityBuffer : SV_TARGET2;
    Texture2D 	        gPrevVisibilityBuffer;
    RWTexture2D<uint>  	gGradientSamplesTexture : SV_TARGET3;
	uint 		        gFrameNumber;
    uint                gTextureWidth;
    uint                gTextureHeight;
    uint                gGradientDownsample;
};

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

[numthreads(16, 16, 1)]
void main(uint3 dispatchThreadId : SV_DispatchThreadID)
{
    //Get a random index from the previous frame within the stratum (gGradientDownsample X gGradientDownsample) that is selected to be forward projected to the next frame
	// here arg is the position within the stratum
    uint2 ipos = uint2(dispatchThreadId.xy);
    int2 idx_prev;
	{
		uint2 arg = uint2(ipos.x + ipos.y * gTextureWidth, gFrameNumber);
		encrypt_tea(arg);
		arg %= gGradientDownsample;
		idx_prev = int2(ipos * gGradientDownsample + arg);
	}

    uint outputValue = 1;
	InterlockedCompareExchange(gGradientSamplesTexture[ipos], 0u, 1u, outputValue);

    if(outputValue == 0u)
    {
        
    }

}
