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
const char kInternalPrevWNormalBuffer[] = "PrevWNormalBuffer";
const char kInternalPrevLinearZBuffer[] = "PrevLinearZBuffer";
const char kInternalPrevVisibilityBuffer[] = "PrevVisibilityBuffer";
const char kInternalPrevWPositionBuffer[] = "PrevWPositionBuffer";

// Input buffer names
const char kInputVisibilityBuffer[] = "VisibilityBuffer";
const char kInputBufferWorldNormal[] = "WorldNormal";
const char kInputBufferLinearZ[] = "LinearZ";
const char kInputWPositionBuffer[] = "WPos";
const char kInputPositionNormalFWidth[] = "PosNormalFWidth";

//Output buffer names
const char kOutputVisibilityBuffer[] = "VisibilityBuffer";
const char kOutputRandomNumberBuffer[] = "RandomNumberBuffer";
const char kOutputGradientSamples[] = "GradientSamplesBuffer";

const char kOutputColorTestBuffer[] = "ColorTestBuffer";

//shader locations
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
}

Properties GradForwardProjPass::getProperties() const
{
    return {};
}

void GradForwardProjPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int screenWidth = compileData.defaultTexDims.x;
    int screenHeight = compileData.defaultTexDims.y;
    
    mpGradientSamplesTexture = Texture::create2D(mpDevice, gradient_res(screenWidth), gradient_res(screenHeight), ResourceFormat::R32Uint, 1, 1, nullptr,
        ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpRandomNumberTexture = Texture::create2D(mpDevice, screenWidth, screenHeight, ResourceFormat::R32Uint, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpPrevRandomNumberTexture = Texture::create2D(mpDevice, screenWidth, screenHeight, ResourceFormat::R32Uint, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpVisibilityBufferTexture = Texture::create2D(mpDevice, screenWidth, screenHeight, ResourceFormat::RGBA32Uint, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    mpColorTestTexture = Texture::create2D(mpDevice, screenWidth, screenHeight, ResourceFormat::RGBA32Float, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );

    //Create random number generation pass output
    Fbo::Desc formatDescRndNum;
    formatDescRndNum.setColorTarget(0, Falcor::ResourceFormat::R32Uint);
    mpRandomNumberGenerationFBO = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatDescRndNum);
}

RenderPassReflection GradForwardProjPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    //Input
    reflector.addInput(kInputBufferLinearZ, "LinearZ");
    reflector.addInput(kInputBufferWorldNormal, "WorldNormalBuffer");
    reflector.addInput(kInputVisibilityBuffer, "VisibilityBuffer");
    reflector.addInput(kInputWPositionBuffer, "WPositionBuffer");
    reflector.addInput(kInputPositionNormalFWidth, "PosNormalFWidth");
    
    //Internal
    reflector.addInternal(kInternalPrevWNormalBuffer, "Prev W Normal Buffer").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    reflector.addInternal(kInternalPrevLinearZBuffer, "Prev Linear Z Buffer").format(ResourceFormat::RG32Float).
        bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    reflector.addInternal(kInternalPrevVisibilityBuffer, "Prev Visibility Buffer").format(ResourceFormat::RGBA32Uint)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);
    reflector.addInternal(kInternalPrevWPositionBuffer, "Prev W Position Buffer").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    //Output
    reflector.addOutput(kOutputRandomNumberBuffer, "RandomNumberBuffer");
    reflector.addOutput(kOutputGradientSamples, "GradientSamples");
    reflector.addOutput(kOutputVisibilityBuffer, "GradientVisibilityBuffer");


    reflector.addOutput(kOutputColorTestBuffer, "kOutputColorTestBuffer");
    
    return reflector;
}

//TODO:: Resize buffers that depend upon gradient resolution and screen size
void GradForwardProjPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!m_pScene)
    {
        return;
    }

    ref<Texture> pInputWorldNormalTexture = renderData.getTexture(kInputBufferWorldNormal);
    ref<Texture> pInputLinearZTexture = renderData.getTexture(kInputBufferLinearZ);
    ref<Texture> pInputVisibilityBuffer = renderData.getTexture(kInputVisibilityBuffer);
    pRenderContext->blit(pInputVisibilityBuffer->getSRV(), mpVisibilityBufferTexture->getRTV());
    ref<Texture> pInputWPosBuffer = renderData.getTexture(kInputWPositionBuffer);
    ref<Texture> pInputPosNormalFWidthBuffer = renderData.getTexture(kInputPositionNormalFWidth);
    
    ref<Texture> pInternalPrevWNormalTexture = renderData.getTexture(kInternalPrevWNormalBuffer);
    ref<Texture> pInternalPrevLinearZTexture = renderData.getTexture(kInternalPrevLinearZBuffer);
    ref<Texture> pInternalPrevVisibilityBuffer = renderData.getTexture(kInternalPrevVisibilityBuffer);
    ref<Texture> pInternalPrevWPositionBuffer = renderData.getTexture(kInternalPrevWPositionBuffer);
    
    ref<Texture> pOutputRandomNumberTexture = renderData.getTexture(kOutputRandomNumberBuffer);
    ref<Texture> pOutputGradientSamplesTexture = renderData.getTexture(kOutputGradientSamples);
    ref<Texture> pOutputVisibilityBufferTexture = renderData.getTexture(kOutputVisibilityBuffer);
    
    //Random number generator pass
    auto perImageRandomNumGeneratorCB = mpPrgRandomNumberGenerator->getRootVar()["PerImageCB"];
    perImageRandomNumGeneratorCB["gSeed"] = ++frameNumber;
    mpPrgRandomNumberGenerator->execute(pRenderContext, mpRandomNumberGenerationFBO);
    pRenderContext->blit(mpRandomNumberGenerationFBO->getColorTexture(0)->getSRV(), mpRandomNumberTexture->getRTV());
    
    float screenWidth = pInputWPosBuffer->getWidth();
    float screenHeight = pInputWPosBuffer->getHeight();

    //Gradient forward projection generation pass
    float gradResWidth = gradient_res(screenWidth);
    float gradResHeight = gradient_res(screenHeight);

    auto perImageGradForwardProjCB = mpPrgGradientForwardProjection->getRootVar()["PerImageCB"];
    perImageGradForwardProjCB["gPrevRandomNumberTexture"] = mpPrevRandomNumberTexture;
    perImageGradForwardProjCB["gCurrentRandomNumberTexture"] = mpRandomNumberTexture;
    perImageGradForwardProjCB["gLinearZTexture"] = pInputLinearZTexture;
    perImageGradForwardProjCB["gPrevLinearZTexture"] = pInternalPrevLinearZTexture;
    perImageGradForwardProjCB["gWNormalTexture"] = pInputWorldNormalTexture;
    perImageGradForwardProjCB["gPrevWNormalTexture"] = pInternalPrevWNormalTexture;
    perImageGradForwardProjCB["gVisibilityBuffer"] = mpVisibilityBufferTexture;
    perImageGradForwardProjCB["gPrevVisibilityBuffer"] = pInternalPrevVisibilityBuffer;
    perImageGradForwardProjCB["gGradientSamplesTexture"] = mpGradientSamplesTexture;
    perImageGradForwardProjCB["gPosTexture"] = pInputWPosBuffer;
    perImageGradForwardProjCB["gPrevWPosTexture"] = pInternalPrevWPositionBuffer;
    perImageGradForwardProjCB["gPositionNormalFwidth"] = pInputPosNormalFWidthBuffer;
    perImageGradForwardProjCB["gViewProjMat"] = m_pScene->getCamera()->getViewProjMatrix();
    perImageGradForwardProjCB["gTextureWidth"] = screenWidth;
    perImageGradForwardProjCB["gTextureHeight"] = screenHeight;
    perImageGradForwardProjCB["gGradientDownsample"] = gradientDownsample;
    perImageGradForwardProjCB["gFrameNumber"] = frameNumber;
    perImageGradForwardProjCB["gColorTestTexture"] = mpColorTestTexture;

    mpPrgGradientForwardProjection->execute(pRenderContext, gradResWidth, gradResHeight);

    //Blit outputs
    pRenderContext->blit(mpGradientSamplesTexture->getSRV(), pOutputGradientSamplesTexture->getRTV());
    pRenderContext->blit(mpRandomNumberTexture->getSRV(), pOutputRandomNumberTexture->getRTV());
    pRenderContext->blit(mpVisibilityBufferTexture->getSRV(), pOutputVisibilityBufferTexture->getRTV());

    pRenderContext->blit(mpColorTestTexture->getSRV(), renderData.getTexture(kOutputColorTestBuffer)->getRTV());
    
    //Swap buffers for next frame
    pRenderContext->blit(pInputLinearZTexture->getSRV(), pInternalPrevLinearZTexture->getRTV());
    pRenderContext->blit(pInputWorldNormalTexture->getSRV(), pInternalPrevWNormalTexture->getRTV());
    pRenderContext->blit(pInputVisibilityBuffer->getSRV(), pInternalPrevVisibilityBuffer->getRTV());
    pRenderContext->blit(pInputWPosBuffer->getSRV(), pInternalPrevWPositionBuffer->getRTV());
    pRenderContext->blit(mpRandomNumberTexture->getSRV(), mpPrevRandomNumberTexture->getRTV());

    //Clear buffers
    pRenderContext->clearUAV(mpGradientSamplesTexture->getUAV().get(), uint4(0, 0, 0, 0));
    pRenderContext->clearUAV(mpRandomNumberTexture->getUAV().get(), uint4(0, 0, 0, 0));
    pRenderContext->clearUAV(mpVisibilityBufferTexture->getUAV().get(), uint4(0, 0, 0, 0));
    pRenderContext->clearUAV(mpColorTestTexture->getUAV().get(), uint4(0, 0, 0, 0));
}

void GradForwardProjPass::renderUI(Gui::Widgets& widget)
{
}

void GradForwardProjPass::setScene(RenderContext* pRenderContext, const ref<Scene>& pScene)
{
    m_pScene = pScene;
    auto defines = m_pScene->getSceneDefines();
    mpPrgRandomNumberGenerator = FullScreenPass::create(mpDevice, kRandomNumberGeneratorShader, defines);
    mpPrgGradientForwardProjection = ComputePass::create(mpDevice, kGradientForwardProjectionShader, "main", defines);

}
