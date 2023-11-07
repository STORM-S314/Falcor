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
#include "MultipleIlluminationModulator.h"

// Shader source files
const char kModulateIllumination[] = "RenderPasses/MultipleIlluminationModulator/MultipleIlluminationModulator.ps.slang";

// Input buffer names
const char kInputIlluminationTexture1[] = "Illumination1";
const char kInputIlluminationTexture2[] = "Illumination2";
const char kInputAlbedoTexture[] = "Albedo";
const char kInputEmissionTexture[] = "Emission";
const char kInputSpecularAlbedoTexture[] = "SpecularAlbedo";

// output
const char kOutputModulatedImage[] = "ModulatedImage";

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, MultipleIlluminationModulator>();
}

MultipleIlluminationModulator::MultipleIlluminationModulator(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
}

Properties MultipleIlluminationModulator::getProperties() const
{
    return {};
}

void MultipleIlluminationModulator::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int screenWidth = compileData.defaultTexDims.x;
    int screenHeight = compileData.defaultTexDims.y;

    Fbo::Desc formatModulatedImageDesc;
    formatModulatedImageDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    mpModulatedImage = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatModulatedImageDesc);

    mpPrgIlluminationModulate = FullScreenPass::create(mpDevice, kModulateIllumination);
}

RenderPassReflection MultipleIlluminationModulator::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    reflector.addInput(kInputIlluminationTexture1, "Illumination1");
    reflector.addInput(kInputIlluminationTexture2, "Illumination2");
    reflector.addInput(kInputAlbedoTexture, "Albedo");
    reflector.addInput(kInputEmissionTexture, "Emission");
    reflector.addInput(kInputSpecularAlbedoTexture, "SpecularAlbedo");

    /// Output image i.e. reconstructed image, (marked as output in the GraphEditor)
    reflector.addOutput(kOutputModulatedImage, "Modulated image").format(ResourceFormat::RGBA32Float);

    return reflector;
}

void MultipleIlluminationModulator::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");

    ref<Texture> pInputIlluminationTexture1 = renderData.getTexture(kInputIlluminationTexture1);
    ref<Texture> pInputIlluminationTexture2 = renderData.getTexture(kInputIlluminationTexture2);
    ref<Texture> pInputAlbedoTexture = renderData.getTexture(kInputAlbedoTexture);
    ref<Texture> pInputEmissionTexture = renderData.getTexture(kInputEmissionTexture);
    ref<Texture> pInputSpecularAlbedo = renderData.getTexture(kInputSpecularAlbedoTexture);

    ref<Texture> pOutputModulation = renderData.getTexture(kOutputModulatedImage);

    auto perImageModulateIlluminationCB = mpPrgIlluminationModulate->getRootVar()["PerImageCB"];
    perImageModulateIlluminationCB["gIllumination1"] = pInputIlluminationTexture1;
    perImageModulateIlluminationCB["gIllumination2"] = pInputIlluminationTexture2;
    perImageModulateIlluminationCB["gAlbedo"] = pInputAlbedoTexture;
    perImageModulateIlluminationCB["gEmissive"] = pInputEmissionTexture;
    perImageModulateIlluminationCB["gSpecularAlbedo"] = pInputSpecularAlbedo;
    mpPrgIlluminationModulate->execute(pRenderContext, mpModulatedImage);

    pRenderContext->blit(mpModulatedImage->getColorTexture(0)->getSRV(), pOutputModulation->getRTV());  

}

void MultipleIlluminationModulator::renderUI(Gui::Widgets& widget)
{
}
