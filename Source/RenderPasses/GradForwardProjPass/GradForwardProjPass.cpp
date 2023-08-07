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
#include "GradForwardProjPass.h"
#include "Utils/Logger.h"

// Internal buffer names
const char kInternalPrevNormalAndZBuffer[] = "PrevNormalAndZBuffer";
const char kInternalPrevVisibilityBuffer[] = "PrevVisibilityBuffer";

// Input buffer names
const char kInputCurrentMVec[] = "MotionVectors";
const char kInputVisibilityBuffer[] = "VisibilityBuffer";
const char kInputBufferWorldNormal[] = "WorldNormal";
const char kInputBufferLinearZ[] = "LinearZ";

//Output buffer names
const char kOutputVisibilityBuffer[] = "VisibilityBuffer";
const char kOutputRandomNumberBuffer[] = "RandomNumberBuffer";
const char kOutputGradientSamples[] = "GradientSamplesBuffer";

//shader locations
const char kPackLinearZAndNormalShader[] = "RenderPasses/GradForwardProjPass/PackLinearZAndNormal.ps.slang";
const char kRandomNumberGeneratorShader[] = "RenderPasses/GradForwardProjPass/RandomNumGenerator.ps.slang";
const char kGradientForwardProjectionShader[] = "RenderPasses/GradForwardProjPass/GradientForwardProjection.ps.slang";

int GradForwardProjPass::gradient_res(int x)
{
    return (x + gradientDownsample - 1) / gradientDownsample;
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, GradForwardProjPass>();
}

GradForwardProjPass::GradForwardProjPass(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    m_pGraphicsState = GraphicsState::create(pDevice);

    mpPackLinearZAndNormal = FullScreenPass::create(mpDevice, kPackLinearZAndNormalShader);
    mpPrgRandomNumberGenerator = FullScreenPass::create(mpDevice, kRandomNumberGeneratorShader);
    mpPrgGradientForwardProjection = FullScreenPass::create(mpDevice, kGradientForwardProjectionShader);
}

Properties GradForwardProjPass::getProperties() const
{
    return {};
}

void GradForwardProjPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int w = compileData.defaultTexDims.x;
    int h = compileData.defaultTexDims.y;
    
    Fbo::Desc formatDescRndNum;
    formatDescRndNum.setSampleCount(0);
    formatDescRndNum.setColorTarget(0, Falcor::ResourceFormat::R32Uint);

    mpRandomNumberFBO[0] = Fbo::create2D(mpDevice, w, h, formatDescRndNum);
    mpRandomNumberFBO[1] = Fbo::create2D(mpDevice, w, h, formatDescRndNum);

    Fbo::Desc formatDescGradientForwardProj;
    formatDescGradientForwardProj.setSampleCount(0);
    formatDescGradientForwardProj.setColorTarget(0, Falcor::ResourceFormat::R32Uint);
    mpGradientSamples = Fbo::create2D(mpDevice, gradient_res(w), gradient_res(h), formatDescGradientForwardProj);

    // Screen-size RGBA32F buffer for linear Z, derivative, and packed normal
    Fbo::Desc formatDescLinearZAndNormal;
    formatDescLinearZAndNormal.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    mpPackLinearZAndNormalFBO = Fbo::create2D(mpDevice, w, h, formatDescLinearZAndNormal);
}

RenderPassReflection GradForwardProjPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    //Input
    reflector.addInput(kInputCurrentMVec, "MotionVectors");
    reflector.addInput(kInputBufferLinearZ, "LinearZ");
    reflector.addInput(kInputBufferWorldNormal, "WorldNormalBuffer");
    reflector.addInput(kInputVisibilityBuffer, "VisibilityBuffer");

    //Internal
    reflector.addInternal(kInternalPrevNormalAndZBuffer, "Prev Normal And Z Buffer").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    reflector.addInternal(kInternalPrevVisibilityBuffer, "Prev Visibility Buffer").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    //Output
    reflector.addOutput(kOutputRandomNumberBuffer, "RandomNumberBuffer");
    reflector.addOutput(kOutputGradientSamples, "GradientSamples");
    reflector.addOutput(kOutputVisibilityBuffer, "GradientVisibilityBuffer");

    return reflector;
}

void GradForwardProjPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pInputCurrentMotionTexture = renderData.getTexture(kInputCurrentMVec);
    ref<Texture> pInputWorldNormalTexture = renderData.getTexture(kInputBufferWorldNormal);
    ref<Texture> pInputLinearZTexture = renderData.getTexture(kInputBufferLinearZ);
    ref<Texture> pInputVisibilityBuffer = renderData.getTexture(kInputVisibilityBuffer);
    
    ref<Texture> pInternalPrevNormalAndZTexture = renderData.getTexture(kInternalPrevNormalAndZBuffer);
    ref<Texture> pInternalPrevVisibilityBuffer = renderData.getTexture(kInternalPrevVisibilityBuffer);
    ref<Texture> pOutputRandomNumberTexture = renderData.getTexture(kOutputRandomNumberBuffer);
    ref<Texture> pOutputGradientSamplesTexture = renderData.getTexture(kOutputGradientSamples);
    ref<Texture> pOutputVisibilityBufferTexture = renderData.getTexture(kOutputVisibilityBuffer);
    
    //Pack Linear Z and Normal
    auto perImageLinearZAndNormalCB = mpPackLinearZAndNormal->getRootVar()["PerImageCB"];
    perImageLinearZAndNormalCB["gLinearZ"] = pInputLinearZTexture;
    perImageLinearZAndNormalCB["gNormal"] = pInputWorldNormalTexture;
    mpPackLinearZAndNormal->execute(pRenderContext, mpPackLinearZAndNormalFBO);

    //Random number generator pass
    auto perImageRandomNumGeneratorCB = mpPrgRandomNumberGenerator->getRootVar()["PerImageCB"];
    perImageRandomNumGeneratorCB["gSeed"] = ++i;
    mpPrgRandomNumberGenerator->execute(pRenderContext, mpRandomNumberFBO[1]);

    //Gradient forward projection generation pass
    GraphicsState::Viewport vp1(0, 0, float(gradient_res(pOutputRandomNumberTexture->getWidth())), float(gradient_res(pOutputRandomNumberTexture->getHeight())), 0, 1);
    m_pGraphicsState->setViewport(0, vp1);

    auto perImageGradForwardProjCB = mpPrgGradientForwardProjection->getRootVar()["PerImageCB"];
    perImageGradForwardProjCB["gPrevRandomNumberTexture"] = mpRandomNumberFBO[0]->getColorTexture(0);
    perImageGradForwardProjCB["gCurrentRandomNumberTexture"] = mpRandomNumberFBO[1]->getColorTexture(0);
    perImageGradForwardProjCB["gMotionVectors"] = pInputCurrentMotionTexture;
    perImageGradForwardProjCB["gLinearZAndNormalTexture"] = mpPackLinearZAndNormalFBO->getColorTexture(0);
    perImageGradForwardProjCB["gPrevLinearZAndNormalTexture"] = pInternalPrevNormalAndZTexture;
    perImageGradForwardProjCB["gVisibilityBuffer"] = pInputVisibilityBuffer;
    perImageGradForwardProjCB["gPrevVisibilityBuffer"] = pInternalPrevVisibilityBuffer;
    
    perImageGradForwardProjCB["gSeed"] = ++i;

    mpPrgGradientForwardProjection->execute(pRenderContext, mpGradientSamples);
    GraphicsState::Viewport vp2(0, 0, float(pOutputRandomNumberTexture->getWidth()), float(pOutputRandomNumberTexture->getHeight()), 0, 1);
    m_pGraphicsState->setViewport(0, vp2);

    //Blit outputs
    pRenderContext->blit(mpGradientSamples->getColorTexture(0)->getSRV(), pOutputGradientSamplesTexture->getRTV());
    pRenderContext->blit(mpRandomNumberFBO[1]->getColorTexture(0)->getSRV(), pOutputRandomNumberTexture->getRTV());
    pRenderContext->blit(pInputVisibilityBuffer->getSRV(), pOutputVisibilityBufferTexture->getRTV());
    
    //Swap buffers for next frame
    pRenderContext->blit(mpPackLinearZAndNormalFBO->getColorTexture(0)->getSRV(), pInternalPrevNormalAndZTexture->getRTV());
    pRenderContext->blit(pInputVisibilityBuffer->getSRV(), pInternalPrevVisibilityBuffer->getRTV());
    
    std::swap(mpRandomNumberFBO[0], mpRandomNumberFBO[1]);
}

void GradForwardProjPass::renderUI(Gui::Widgets& widget)
{
}
