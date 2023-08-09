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
const char kInternalPrevWPositionBuffer[] = "PrevWPositionBuffer";

// Input buffer names
const char kInputCurrentMVec[] = "MotionVectors";
const char kInputVisibilityBuffer[] = "VisibilityBuffer";
const char kInputBufferWorldNormal[] = "WorldNormal";
const char kInputBufferLinearZ[] = "LinearZ";
const char kInputWPositionBuffer[] = "WPos";
const char kInputPositionNormalFWidth[] = "PosNormalFWidth";

//Output buffer names
const char kOutputVisibilityBuffer[] = "VisibilityBuffer";
const char kOutputRandomNumberBuffer[] = "RandomNumberBuffer";
const char kOutputGradientSamples[] = "GradientSamplesBuffer";

//shader locations
const char kPackLinearZAndNormalShader[] = "RenderPasses/GradForwardProjPass/PackLinearZAndNormal.ps.slang";
const char kRandomNumberGeneratorShader[] = "RenderPasses/GradForwardProjPass/RandomNumGenerator.ps.slang";
const char kGradientForwardProjectionShader[] = "RenderPasses/GradForwardProjPass/GradientForwardProjection.cs.slang";

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
    mpPrgGradientForwardProjection = ComputePass::create(mpDevice, kGradientForwardProjectionShader, "main");
}

Properties GradForwardProjPass::getProperties() const
{
    return {};
}

void GradForwardProjPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int w = compileData.defaultTexDims.x;
    int h = compileData.defaultTexDims.y;
    
    mpGradientSamplesTexture = Texture::create2D(mpDevice, gradient_res(w), gradient_res(h), ResourceFormat::R32Uint, 1, 1, nullptr,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpRandomNumberTexture = Texture::create2D(mpDevice, w, h, ResourceFormat::R32Uint, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpVisibilityBufferTexture = Texture::create2D(mpDevice, w, h, ResourceFormat::RGBA32Uint, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    // Screen-size RGBA32F buffer for linear Z, derivative, and packed normal
    Fbo::Desc formatDescLinearZAndNormal;
    formatDescLinearZAndNormal.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    mpPackLinearZAndNormalFBO = Fbo::create2D(mpDevice, w, h, formatDescLinearZAndNormal);

    //Create random number generation pass output
    Fbo::Desc formatDescRndNum;
    formatDescRndNum.setColorTarget(0, Falcor::ResourceFormat::R32Uint);
    mpRandomNumberGenerationFBO = Fbo::create2D(mpDevice, w, h, formatDescRndNum);
    mpPrevRandomNumberTextureFBO = Fbo::create2D(mpDevice, w, h, formatDescRndNum);
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
    reflector.addInput(kInputWPositionBuffer, "WPositionBuffer");
    reflector.addInput(kInputPositionNormalFWidth, "PosNormalFWidth");
    
    //Internal
    reflector.addInternal(kInternalPrevNormalAndZBuffer, "Prev Normal And Z Buffer").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    reflector.addInternal(kInternalPrevVisibilityBuffer, "Prev Visibility Buffer").format(ResourceFormat::RGBA32Uint)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    reflector.addInternal(kInternalPrevWPositionBuffer, "Prev W Position Buffer").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    //Output
    reflector.addOutput(kOutputRandomNumberBuffer, "RandomNumberBuffer");
    reflector.addOutput(kOutputGradientSamples, "GradientSamples");
    reflector.addOutput(kOutputVisibilityBuffer, "GradientVisibilityBuffer");

    return reflector;
}

//TODO:: Resize buffers that depend upon gradient resolution and screen size
void GradForwardProjPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!m_pScene)
    {
        return;
    }

    ref<Texture> pInputCurrentMotionTexture = renderData.getTexture(kInputCurrentMVec);
    ref<Texture> pInputWorldNormalTexture = renderData.getTexture(kInputBufferWorldNormal);
    ref<Texture> pInputLinearZTexture = renderData.getTexture(kInputBufferLinearZ);
    ref<Texture> pInputVisibilityBuffer = renderData.getTexture(kInputVisibilityBuffer);
    pRenderContext->blit(pInputVisibilityBuffer->getSRV(), mpVisibilityBufferTexture->getRTV());
    ref<Texture> pInputWPosBuffer = renderData.getTexture(kInputWPositionBuffer);
    ref<Texture> pInputPosNormalFWidthBuffer = renderData.getTexture(kInputPositionNormalFWidth);
    
    ref<Texture> pInternalPrevNormalAndZTexture = renderData.getTexture(kInternalPrevNormalAndZBuffer);
    ref<Texture> pInternalPrevVisibilityBuffer = renderData.getTexture(kInternalPrevVisibilityBuffer);
    ref<Texture> pInternalPrevWPositionBuffer = renderData.getTexture(kInternalPrevWPositionBuffer);
    
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
    perImageRandomNumGeneratorCB["gSeed"] = ++frameNumber;
    mpPrgRandomNumberGenerator->execute(pRenderContext, mpRandomNumberGenerationFBO);
    pRenderContext->blit(mpRandomNumberGenerationFBO->getColorTexture(0)->getSRV(), mpRandomNumberTexture->getRTV());
    
    //Gradient forward projection generation pass
    float gradResWidth = gradient_res(pOutputRandomNumberTexture->getWidth());
    float gradResHeight = gradient_res(pOutputRandomNumberTexture->getHeight());

    auto perImageGradForwardProjCB = mpPrgGradientForwardProjection->getRootVar()["PerImageCB"];
    perImageGradForwardProjCB["gPrevRandomNumberTexture"] = mpPrevRandomNumberTextureFBO->getColorTexture(0);
    perImageGradForwardProjCB["gCurrentRandomNumberTexture"] = mpRandomNumberTexture;
    perImageGradForwardProjCB["gMotionVectors"] = pInputCurrentMotionTexture;
    perImageGradForwardProjCB["gLinearZAndNormalTexture"] = mpPackLinearZAndNormalFBO->getColorTexture(0);
    perImageGradForwardProjCB["gPrevLinearZAndNormalTexture"] = pInternalPrevNormalAndZTexture;
    perImageGradForwardProjCB["gVisibilityBuffer"] = mpVisibilityBufferTexture;
    perImageGradForwardProjCB["gPrevVisibilityBuffer"] = pInternalPrevVisibilityBuffer;
    perImageGradForwardProjCB["gGradientSamplesTexture"] = mpGradientSamplesTexture;
    perImageGradForwardProjCB["gPrevWPosTexture"] = pInternalPrevWPositionBuffer;
    perImageGradForwardProjCB["gPositionNormalFwidth"] = pInputPosNormalFWidthBuffer;
    perImageGradForwardProjCB["gViewProjMat"] = m_pScene->getCamera()->getViewProjMatrixNoJitter();
    perImageGradForwardProjCB["gTextureWidth"] = pOutputRandomNumberTexture->getWidth();
    perImageGradForwardProjCB["gTextureHeight"] = pOutputRandomNumberTexture->getHeight();
    perImageGradForwardProjCB["gGradientDownsample"] = gradientDownsample;
    perImageGradForwardProjCB["gFrameNumber"] = frameNumber;
    mpPrgGradientForwardProjection->execute(pRenderContext, (uint32_t)gradResWidth, (uint32_t)gradResHeight);

    //Blit outputs
    pRenderContext->blit(mpGradientSamplesTexture->getSRV(), pOutputGradientSamplesTexture->getRTV());
    pRenderContext->blit(mpRandomNumberTexture->getSRV(), pOutputRandomNumberTexture->getRTV());
    pRenderContext->blit(mpVisibilityBufferTexture->getSRV(), pOutputVisibilityBufferTexture->getRTV());
    
    //Swap buffers for next frame
    pRenderContext->blit(mpPackLinearZAndNormalFBO->getColorTexture(0)->getSRV(), pInternalPrevNormalAndZTexture->getRTV());
    pRenderContext->blit(mpVisibilityBufferTexture->getSRV(), pInternalPrevVisibilityBuffer->getRTV());
    pRenderContext->blit(pInputWPosBuffer->getSRV(), pInternalPrevWPositionBuffer->getRTV());
    pRenderContext->blit(mpRandomNumberTexture->getSRV(), mpPrevRandomNumberTextureFBO->getColorTexture(0)->getRTV());

    //Clear buffers
    pRenderContext->clearUAV(mpGradientSamplesTexture->getUAV().get(), uint4(0, 0, 0, 0));
    pRenderContext->clearUAV(mpRandomNumberTexture->getUAV().get(), uint4(0, 0, 0, 0));
    pRenderContext->clearUAV(mpVisibilityBufferTexture->getUAV().get(), uint4(0, 0, 0, 0));
}

void GradForwardProjPass::renderUI(Gui::Widgets& widget)
{
}

void GradForwardProjPass::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    m_pScene = pScene;
    logWarning("GRADFORWARDPROJ::SETTING THE SCENE !!!!!!!");
    if (!m_pScene)
    {
        logWarning("GRADFORWARDPROJ::SCENE IS NULL");
    }
    else if (!m_pScene->getCamera())
    {
        logWarning("GRADFORWARDPROJ::CAMERA IS NULL");    
    }
    else
    {
        float4x4 l_ProjMat =  m_pScene->getCamera()->getViewProjMatrixNoJitter();
    }
}
