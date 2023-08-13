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
 
/*
    Reimplementation of A-SVGF as per the paper https://cg.ivd.kit.edu/atf.php by Christoph Schied
*/

#include "ASVGFPass.h"


int ASVGFPass::gradient_res(int x)
{
    return (x + gradientDownsample - 1) / gradientDownsample;
}

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, ASVGFPass>();
}

// Shader source files
const char kTemporalAccumulationShader[]    = "RenderPasses/ASVGFPass/TemporalAccumulation.ps.slang";
const char kEstimateVarianceShader[]        = "RenderPasses/ASVGFPass/EstimateVariance.ps.slang";
const char kAtrousShader[]                  = "RenderPasses/ASVGFPass/Atrous.ps.slang";
const char kCreateGradientSamplesShader[]   = "RenderPasses/ASVGFPass/CreateGradientSamples.ps.slang";
const char kAtrousGradientShader[]          = "RenderPasses/ASVGFPass/AtrousGradient.ps.slang";

// Names of valid entries in the parameter dictionary.
const char kEnable[]                = "Enable";
const char kModulateAlbedo[]        = "ModulateAlbedo";
const char kNumIterations[]         = "NumIterations";
const char kHistoryTap[]            = "HistoryTap";
const char kFilterKernel[]          = "FilterKernel";
const char kTemporalAlpha[]         = "TemporalAlpha";
const char kDiffAtrousIterations[]  = "DiffAtrousIterations";
const char kGradientFilterRadius[]  = "GradientFilterRadius";
const char kNormalizeGradient[]     = "NormalizeGradient";
const char kShowAntilagAlpha[]      = "ShowAntilagAlpha";

//Input buffer names
const char kInputColorTexture[] = "Color";
const char kInputAlbedoTexture[] = "Albedo";
const char kInputEmissionTexture[] = "Emission";
const char kInputLinearZTexture[] = "LinearZ";
const char kInputVisibilityBufferTexture[] = "VisibilityBuffer";
const char kInputGradientSamplesTexture[] = "GradientSamples";

// Internal buffer names
const char kInternalPrevColorTexture[] = "PrevColor";
const char kInternalPrevAlbedoTexture[] = "PrevAlbedo";
const char kInternalPrevEmissionTexture[] = "PrevEmission";

// Output buffer name
const char kOutputBufferFilteredImage[] = "Filtered image";


ASVGFPass::ASVGFPass(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    for (const auto& [key, value] : props)
    {
        if (key == kEnable)                     mEnable = value;
        else if (key == kModulateAlbedo)        mModulateAlbedo = value;
        else if (key == kNumIterations)         mNumIterations = value;
        else if (key == kHistoryTap)            mHistoryTap = value;
        else if (key == kFilterKernel)          mFilterKernel = value;
        else if (key == kTemporalAlpha)         mTemporalAlpha = value;
        else if (key == kDiffAtrousIterations)  mDiffAtrousIterations = value;
        else if (key == kGradientFilterRadius)  mGradientFilterRadius = value;
        else if (key == kNormalizeGradient)     mNormalizeGradient = value;
        else if (key == kShowAntilagAlpha)      mShowAntilagAlpha = value;
        else logWarning("Unknown property '{}' in ASVGFPass properties.", key);
    }

    pGraphicsState = GraphicsState::create(pDevice);

    mpPrgGradientForwardProjection = FullScreenPass::create(mpDevice, kCreateGradientSamplesShader);
    mpPrgAtrousGradientCalculation = FullScreenPass::create(mpDevice, kAtrousGradientShader);
}

Properties ASVGFPass::getProperties() const
{
    Properties props;
    props[kEnable] = mEnable;
    props[kModulateAlbedo] = mModulateAlbedo;
    props[kNumIterations] = mNumIterations;
    props[kHistoryTap] = mHistoryTap;
    props[kFilterKernel] = mFilterKernel;
    props[kTemporalAlpha] = mTemporalAlpha;
    props[kDiffAtrousIterations] = mDiffAtrousIterations;
    props[kGradientFilterRadius] = mGradientFilterRadius;
    props[kNormalizeGradient] = mNormalizeGradient;
    props[kShowAntilagAlpha] = mShowAntilagAlpha;
    return props;
}

void ASVGFPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int screenWidth = compileData.defaultTexDims.x;
    int screenHeight = compileData.defaultTexDims.y;
    int gradResWidth = gradient_res(screenWidth);
    int gradResHeight = gradient_res(screenHeight);

    Fbo::Desc formatDescGradientResult;
    formatDescGradientResult.setSampleCount(0);
    formatDescGradientResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float); // gradients :: luminance max, luminance differece, 1.0/0.0, 0.0 
    formatDescGradientResult.setColorTarget(1, Falcor::ResourceFormat::RGBA32Float); // variance :: total luminance, variance, depth current.x, depth current.y
    mpGradientResultPingPongBuffer[0] = Fbo::create2D(mpDevice, gradResWidth, gradResHeight, formatDescGradientResult);
    mpGradientResultPingPongBuffer[1] = Fbo::create2D(mpDevice, gradResWidth, gradResHeight, formatDescGradientResult);
}

RenderPassReflection ASVGFPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    //Input
    reflector.addInput(kInputColorTexture, "Color");
    reflector.addInput(kInputAlbedoTexture, "Albedo");
    reflector.addInput(kInputEmissionTexture, "Emission");
    reflector.addInput(kInputLinearZTexture, "LinearZ");
    reflector.addInput(kInputVisibilityBufferTexture, "VisibilityBuffer");

    //Internal buffers
    reflector.addInternal(kInternalPrevColorTexture, "Previous Color").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevAlbedoTexture, "Previous Albedo").format(ResourceFormat::RGBA32Float).
        bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevEmissionTexture, "Previous Emission").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    /// Output image i.e. reconstructed image, (marked as output in the GraphEditor)
    reflector.addOutput(kOutputBufferFilteredImage, "Filtered image").format(ResourceFormat::RGBA32Float);

    return reflector;
}

void ASVGFPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!pScene)
    {
        return;
    }

    ref<Texture> pInputColorTexture = renderData.getTexture(kInputColorTexture);
    ref<Texture> pInputAlbedoTexture = renderData.getTexture(kInputAlbedoTexture);
    ref<Texture> pInputEmissionTexture = renderData.getTexture(kInputEmissionTexture);
    ref<Texture> pInputGradSamplesTexture = renderData.getTexture(kInputGradientSamplesTexture);
    ref<Texture> pInputLinearZTexture = renderData.getTexture(kInputLinearZTexture);
    ref<Texture> pInputGradientSamples = renderData.getTexture(kInputGradientSamplesTexture);
    ref<Texture> pInputVisibilityBuffer = renderData.getTexture(kInputVisibilityBufferTexture);
    
    ref<Texture> pInternalPrevColorTexture = renderData.getTexture(kInternalPrevColorTexture);
    ref<Texture> pInternalPrevAlbedoTexture = renderData.getTexture(kInternalPrevAlbedoTexture);
    ref<Texture> pInternalPrevEmissionTexture = renderData.getTexture(kInternalPrevEmissionTexture);

    ref<Texture> pOutputFilteredImage = renderData.getTexture(kOutputBufferFilteredImage);

    int screenWidth = pInputColorTexture->getWidth();
    int screenHeight = pInputColorTexture->getHeight();

    float gradResWidth = gradient_res(screenWidth);
    float gradResHeight = gradient_res(screenHeight);

    auto& currentCamera = *pScene->getCamera();
    float2 cameraJitter = float2(currentCamera.getJitterX(), currentCamera.getJitterY());
    float2 jitterOffset = (cameraJitter - mPrevFrameJitter);

    GraphicsState::Viewport vp1(0, 0, gradResWidth, gradResHeight, 0, 1);
    pGraphicsState->setViewport(0, vp1);

    //Gradient creation pass
    auto perImageGradForwardProjCB = mpPrgGradientForwardProjection->getRootVar()["PerImageCB"];
    perImageGradForwardProjCB["gColorTexture"] = pInputColorTexture;
    perImageGradForwardProjCB["gPrevColorTexture"] = pInternalPrevColorTexture;
    perImageGradForwardProjCB["gAlbedoTexture"] = pInputAlbedoTexture;
    perImageGradForwardProjCB["gPrevAlbedoTexture"] = pInternalPrevAlbedoTexture;
    perImageGradForwardProjCB["gEmissionTexture"] = pInputEmissionTexture;
    perImageGradForwardProjCB["gPrevEmissionTexture"] = pInternalPrevEmissionTexture;
    perImageGradForwardProjCB["gGradientSamples"] = pInputGradientSamples;
    perImageGradForwardProjCB["gVisibilityBuffer"] = pInputVisibilityBuffer;
    perImageGradForwardProjCB["gLinearZTexture"] = pInputLinearZTexture;
    perImageGradForwardProjCB["gGradientDownsample"] = gradientDownsample;
    perImageGradForwardProjCB["gScreenWidth"] = screenWidth;
    
    mpPrgGradientForwardProjection->execute(pRenderContext, mpGradientResultPingPongBuffer[0]);

    //A-trous gradient
    for (int indexAtrous = 0; indexAtrous < mNumIterations; indexAtrous++)
    {
        auto perImageATrousGradientCB = mpPrgAtrousGradientCalculation->getRootVar()["PerImageCB"];
        perImageATrousGradientCB["gGradientLuminance"] = mpGradientResultPingPongBuffer[0]->getColorTexture(0);
        perImageATrousGradientCB["gGradientVariance"] = mpGradientResultPingPongBuffer[0]->getColorTexture(1);
        perImageATrousGradientCB["gStepSize"] = 1 << indexAtrous;
        perImageATrousGradientCB["gGradientDownsample"] = gradientDownsample;

        mpPrgAtrousGradientCalculation->execute(pRenderContext, mpGradientResultPingPongBuffer[1]);
        std::swap(mpGradientResultPingPongBuffer[0], mpGradientResultPingPongBuffer[1]);
    }

    GraphicsState::Viewport vp2(0, 0, screenWidth, screenHeight, 0, 1);
    pGraphicsState->setViewport(0, vp2);





    //Blit outputs
    //pRenderContext->blit(mpGradientResultBuffer->getColorTexture(0)->getSRV(), pOutputFilteredImage->getRTV());

    // Swap buffers for next frame}
    pRenderContext->blit(pInputColorTexture->getSRV(), pInternalPrevColorTexture->getRTV());
    pRenderContext->blit(pInputAlbedoTexture->getSRV(), pInternalPrevAlbedoTexture->getRTV());
    pRenderContext->blit(pInputEmissionTexture->getSRV(), pInternalPrevEmissionTexture->getRTV());
    mPrevFrameJitter = cameraJitter;
}

void ASVGFPass::setScene(RenderContext* a_pRenderContext, const ref<Scene>& a_pScene)
{
    pScene = a_pScene;
}

void ASVGFPass::renderUI(Gui::Widgets& widget)
{
    widget.checkbox("Enable", mEnable);
    widget.checkbox("Modulate Albedo", mModulateAlbedo);
    widget.var("Temporal Alpha", mTemporalAlpha);
    widget.var("# Iterations", mNumIterations, 0, 16, 1);
    widget.var("History Tap", mHistoryTap, -1, 16, 1);

    Falcor::Gui::DropdownList filterKernels{
        {0, "A-Trous"}, {1, "Box 3x3"}, {2, "Box 5x5"}, {3, "Sparse"}, {4, "Box3x3 / Sparse"}, {5, "Box5x5 / Sparse"},
    };
    widget.dropdown( "Kernel", filterKernels, mFilterKernel);

    widget.checkbox("Show Antilag Alpha", mShowAntilagAlpha);
    widget.var("# Diff Iterations", mDiffAtrousIterations, 0, 16, 1);
    widget.var("Gradient Filter Radius", mGradientFilterRadius, 0, 16, 1);

    widget.var("Gradient Downsample", gradientDownsample, 0, 16, 1);
}
