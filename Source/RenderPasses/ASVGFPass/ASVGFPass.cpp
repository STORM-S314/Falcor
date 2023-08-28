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
const char kEstimateVarianceShader[]        = "RenderPasses/ASVGFPass/VarianceEstimation.ps.slang";
const char kAtrousShader[]                  = "RenderPasses/ASVGFPass/AtrousFullScreen.ps.slang";
const char kCreateGradientSamplesShader[]   = "RenderPasses/ASVGFPass/CreateGradientSamples.ps.slang";
const char kAtrousGradientShader[]          = "RenderPasses/ASVGFPass/AtrousGradient.ps.slang";

#if IS_DEBUG_PASS
const char kDebugPassShader[] = "RenderPasses/ASVGFPass/DebugPass.ps.slang";
#endif IS_DEBUG_PASS

// Names of valid entries in the parameter dictionary.
const char kEnable[]                = "Enable";
const char kModulateAlbedo[]        = "ModulateAlbedo";
const char kNumIterations[]         = "NumIterations";
const char kHistoryTap[]            = "HistoryTap";
const char kFilterKernel[]          = "FilterKernel";
const char kTemporalColorAlpha[]    = "TemporalColorAlpha";
const char kTemporalMomentsAlpha[]  = "TemporalMomentsAlpha";
const char kDiffAtrousIterations[]  = "DiffAtrousIterations";
const char kGradientFilterRadius[]  = "GradientFilterRadius";
const char kNormalizeGradient[]     = "NormalizeGradient";
const char kShowAntilagAlpha[]      = "ShowAntilagAlpha";

//Input buffer names
const char kInputColorTexture[]                 = "Color";
const char kInputAlbedoTexture[]                = "Albedo";
const char kInputEmissionTexture[]              = "Emission";
const char kInputLinearZTexture[]               = "LinearZ";
const char kInputNormalsTexture[]               = "Normals";
const char kInputCurrVisibilityBufferTexture[]  = "CurrentVisibilityBuffer";
const char kInputGradVisibilityBufferTexture[]  = "GradientVisibilityBuffer";
const char kInputGradientSamplesTexture[]       = "GradientSamples";
const char kInputMotionVectors[]                = "MotionVectors";
const char kInputPosNormalFWidth[]              = "PosNormalFWidth";

// Internal buffer names
const char kInternalPrevColorTexture[]      = "PrevColor";
const char kInternalPrevAlbedoTexture[]     = "PrevAlbedo";
const char kInternalPrevEmissionTexture[]   = "PrevEmission";
const char kInternalPrevLinearZTexture[]    = "PrevLinearZ";
const char kInternalPrevNormalsTexture[]    = "PrevNormals";
const char kInternalPrevVisibilityBuffer[]  = "PrevVisBuffer";

// Output buffer name
const char kOutputBufferFilteredImage[] = "Filtered image";


ASVGFPass::ASVGFPass(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    for (const auto& [key, value] : props)
    {
        if (key == kNumIterations)              mNumIterations = value;
        else if (key == kHistoryTap)            mHistoryTap = value;
        else if (key == kFilterKernel)          mFilterKernel = value;
        else if (key == kTemporalColorAlpha)    mTemporalColorAlpha = value;
        else if (key == kTemporalMomentsAlpha)  mTemporalMomentsAlpha = value;
        else if (key == kDiffAtrousIterations)  mDiffAtrousIterations = value;
        else if (key == kGradientFilterRadius)  mGradientFilterRadius = value;
        //else if (key == kNormalizeGradient)     mNormalizeGradient = value;
        //else if (key == kShowAntilagAlpha)      mShowAntilagAlpha = value;
        else logWarning("Unknown property '{}' in ASVGFPass properties.", key);
    }
}

Properties ASVGFPass::getProperties() const
{
    Properties props;
    props[kNumIterations]           = mNumIterations;
    props[kHistoryTap]              = mHistoryTap;
    props[kFilterKernel]            = mFilterKernel;
    props[kTemporalColorAlpha]      = mTemporalColorAlpha;
    props[kTemporalMomentsAlpha]    = mTemporalMomentsAlpha;
    props[kDiffAtrousIterations]    = mDiffAtrousIterations;
    props[kGradientFilterRadius]    = mGradientFilterRadius;
    //props[kNormalizeGradient] = mNormalizeGradient;
    //props[kShowAntilagAlpha] = mShowAntilagAlpha;
    return props;
}

void ASVGFPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int screenWidth = compileData.defaultTexDims.x;
    int screenHeight = compileData.defaultTexDims.y;

    allocateBuffers(pRenderContext, screenWidth, screenHeight);
    IsClearBuffers = true;
}

RenderPassReflection ASVGFPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    //Input
    reflector.addInput(kInputColorTexture, "Color");
    reflector.addInput(kInputAlbedoTexture, "Albedo");
    reflector.addInput(kInputEmissionTexture, "Emission");
    reflector.addInput(kInputLinearZTexture, "LinearZ");
    reflector.addInput(kInputNormalsTexture, "Normals");
    reflector.addInput(kInputGradVisibilityBufferTexture, "GradientVisibilityBuffer").format(ResourceFormat::RGBA32Uint);
    reflector.addInput(kInputCurrVisibilityBufferTexture, "CurrentVisibilityBuffer").format(ResourceFormat::RGBA32Uint);
    reflector.addInput(kInputMotionVectors, "MotionVectors");
    reflector.addInput(kInputPosNormalFWidth, "PosNormalFWidth");
    reflector.addInput(kInputGradientSamplesTexture, "GradSamples").format(ResourceFormat::R32Uint);
    
    //Internal buffers
    reflector.addInternal(kInternalPrevColorTexture, "Previous Color").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevAlbedoTexture, "Previous Albedo").format(ResourceFormat::RGBA32Float).
        bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevEmissionTexture, "Previous Emission").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevLinearZTexture, "Previous LinearZ").format(ResourceFormat::RG32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevNormalsTexture, "Previous Normals").format(ResourceFormat::RGBA32Float)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevVisibilityBuffer, "Previous Visibility Buffer").format(ResourceFormat::RGBA32Uint)
        .bindFlags(Resource::BindFlags::RenderTarget | Resource::BindFlags::ShaderResource);

    /// Output image i.e. reconstructed image, (marked as output in the GraphEditor)
    reflector.addOutput(kOutputBufferFilteredImage, "Filtered image").format(ResourceFormat::RGBA32Float);

    return reflector;
}

void ASVGFPass::allocateBuffers(RenderContext* a_pRenderContext, int a_ScreenWidth,  int a_ScreenHeight)
{
    int screenWidth     = a_ScreenWidth;
    int screenHeight    = a_ScreenHeight;
    int gradResWidth    = gradient_res(screenWidth);
    int gradResHeight   = gradient_res(screenHeight);

    // Gradient
    Fbo::Desc formatDescGradientResult;
    //formatDescGradientResult.setSampleCount(0);
    formatDescGradientResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float); // gradients :: luminance max, luminance differece, 1.0 or 0.0, 0.0
    formatDescGradientResult.setColorTarget(1, Falcor::ResourceFormat::RGBA32Float); // variance :: total luminance, variance, depth current.x, depth current.y
    mpGradientResultPingPongBuffer[0] = Fbo::create2D(mpDevice, gradResWidth, gradResHeight, formatDescGradientResult);
    mpGradientResultPingPongBuffer[1] = Fbo::create2D(mpDevice, gradResWidth, gradResHeight, formatDescGradientResult);

    // Accumulation
    Fbo::Desc formatDescAccumulationResult;
    //formatDescAccumulationResult.setSampleCount(0);
    formatDescAccumulationResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float); // Accumulation color
    formatDescAccumulationResult.setColorTarget(1, Falcor::ResourceFormat::RG32Float);   // Accumulation moments
    formatDescAccumulationResult.setColorTarget(2, Falcor::ResourceFormat::R16Float);    // Accumulation length
    mpAccumulationBuffer = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatDescAccumulationResult);
    mpPrevAccumulationBuffer = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatDescAccumulationResult);

    // Atrous full screen
    Fbo::Desc formatAtrousFullScreenResult;
    //formatAtrousFullScreenResult.setSampleCount(0);
    formatAtrousFullScreenResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float); // Color.rgb, variance
    mpAtrousFullScreenResultPingPong[0] = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatAtrousFullScreenResult);
    mpAtrousFullScreenResultPingPong[1] = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatAtrousFullScreenResult);

#if IS_DEBUG_PASS
    Fbo::Desc formatDebugFullScreenResult;
    formatDebugFullScreenResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
    mpDebugBuffer = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatDebugFullScreenResult);

    mpTestColorTexture = Texture::create2D(
        mpDevice, screenWidth, screenHeight, ResourceFormat::RGBA32Float, 1, 1, nullptr,
        ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess
    );
#endif IS_DEBUG_PASS
}

void ASVGFPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    if (!pScene)
    {
        return;
    }

    ref<Texture> pInputColorTexture             = renderData.getTexture(kInputColorTexture);
    ref<Texture> pInputAlbedoTexture            = renderData.getTexture(kInputAlbedoTexture);
    ref<Texture> pInputEmissionTexture          = renderData.getTexture(kInputEmissionTexture);
    ref<Texture> pInputLinearZTexture           = renderData.getTexture(kInputLinearZTexture);
    ref<Texture> pInputGradientSamples          = renderData.getTexture(kInputGradientSamplesTexture);
    ref<Texture> pInputCurrVisibilityBuffer     = renderData.getTexture(kInputCurrVisibilityBufferTexture);
    ref<Texture> pInputGradVisibilityBuffer     = renderData.getTexture(kInputGradVisibilityBufferTexture);
    ref<Texture> pInputMotionVectors            = renderData.getTexture(kInputMotionVectors);
    ref<Texture> pInputNormalVectors            = renderData.getTexture(kInputNormalsTexture);
    ref<Texture> pInputPosNormalFWidth          = renderData.getTexture(kInputPosNormalFWidth);
    
    ref<Texture> pInternalPrevColorTexture      = renderData.getTexture(kInternalPrevColorTexture);
    ref<Texture> pInternalPrevAlbedoTexture     = renderData.getTexture(kInternalPrevAlbedoTexture);
    ref<Texture> pInternalPrevEmissionTexture   = renderData.getTexture(kInternalPrevEmissionTexture);
    ref<Texture> pInternalPrevLinearZTexture    = renderData.getTexture(kInternalPrevLinearZTexture);
    ref<Texture> pInternalPrevNormalsTexture    = renderData.getTexture(kInternalPrevNormalsTexture);
    ref<Texture> pInternalPrevVisBufferTexture  = renderData.getTexture(kInternalPrevVisibilityBuffer);
    
    ref<Texture> pOutputFilteredImage = renderData.getTexture(kOutputBufferFilteredImage);

    if (IsClearBuffers)
    {
        clearBuffers(pRenderContext, renderData);
        IsClearBuffers = false;
    }

    int screenWidth = pInputColorTexture->getWidth();
    int screenHeight = pInputColorTexture->getHeight();

    FALCOR_ASSERT(
        mpAccumulationBuffer &&
        mpAccumulationBuffer->getWidth() == screenWidth &&
        mpAccumulationBuffer->getHeight() == screenHeight
    );


    float gradResWidth = gradient_res(screenWidth);
    float gradResHeight = gradient_res(screenHeight);

    auto& currentCamera = *pScene->getCamera();
    float2 cameraJitter = float2(currentCamera.getJitterX(), currentCamera.getJitterY());
    float2 jitterOffset = (cameraJitter - mPrevFrameJitter);

    GraphicsState::Viewport vpGradRes(0, 0, gradResWidth, gradResHeight, 0, 1);

    //Gradient creation pass
    auto gradForwardGraphicState = mpPrgGradientForwardProjection->getState();
    gradForwardGraphicState->setViewport(0, vpGradRes);

    auto perImageGradForwardProjCB = mpPrgGradientForwardProjection->getRootVar()["PerImageCB"];
    perImageGradForwardProjCB["gColorTexture"]          =   pInputColorTexture;
    perImageGradForwardProjCB["gPrevColorTexture"]      =   pInternalPrevColorTexture;
    perImageGradForwardProjCB["gAlbedoTexture"]         =   pInputAlbedoTexture;
    perImageGradForwardProjCB["gPrevAlbedoTexture"]     =   pInternalPrevAlbedoTexture;
    perImageGradForwardProjCB["gEmissionTexture"]       =   pInputEmissionTexture;
    perImageGradForwardProjCB["gPrevEmissionTexture"]   =   pInternalPrevEmissionTexture;
    perImageGradForwardProjCB["gGradientSamples"]       =   pInputGradientSamples;
    perImageGradForwardProjCB["gVisibilityBuffer"]      =   pInputCurrVisibilityBuffer;
    perImageGradForwardProjCB["gLinearZTexture"]        =   pInputLinearZTexture;
    perImageGradForwardProjCB["gGradientDownsample"]    =   gradientDownsample;
    perImageGradForwardProjCB["gScreenWidth"]           =   screenWidth;
    //perImageGradForwardProjCB["gColorTest"]             =   mpTestColorTexture;

    mpPrgGradientForwardProjection->execute(pRenderContext, mpGradientResultPingPongBuffer[0], false);

#if IS_DEBUG_PASS
    debugPass(pRenderContext, renderData);
    pRenderContext->blit(mpDebugBuffer->getColorTexture(0)->getSRV(), pOutputFilteredImage->getRTV());

    //// Swap buffers for next frame}
    pRenderContext->blit(pInputColorTexture->getSRV(), pInternalPrevColorTexture->getRTV());
    pRenderContext->blit(pInputAlbedoTexture->getSRV(), pInternalPrevAlbedoTexture->getRTV());
    pRenderContext->blit(pInputEmissionTexture->getSRV(), pInternalPrevEmissionTexture->getRTV());
    pRenderContext->blit(pInputLinearZTexture->getSRV(), pInternalPrevLinearZTexture->getRTV());
    pRenderContext->blit(pInputNormalVectors->getSRV(), pInternalPrevNormalsTexture->getRTV());
    pRenderContext->blit(pInputCurrVisibilityBuffer->getSRV(), pInternalPrevVisBufferTexture->getRTV());
    std::swap(mpAccumulationBuffer, mpPrevAccumulationBuffer);
    mPrevFrameJitter = cameraJitter;
    pRenderContext->clearTexture(mpTestColorTexture.get());

    return;
#endif

    //A-trous gradient
    auto atrousGradCalcGraphicState = mpPrgAtrousGradientCalculation->getState();
    atrousGradCalcGraphicState->setViewport(0, vpGradRes);

    auto perImageATrousGradientCB = mpPrgAtrousGradientCalculation->getRootVar()["PerImageCB"];
    perImageATrousGradientCB["gGradientResDimensions"]  = float2(gradResWidth, gradResHeight);
    perImageATrousGradientCB["gGradientDownsample"]     = gradientDownsample;
    perImageATrousGradientCB["gPhiColor"]               = weightPhiColor;

    for (int indexAtrous = 0; indexAtrous < mDiffAtrousIterations; indexAtrous++)
    {
        perImageATrousGradientCB["gGradientLuminance"]      = mpGradientResultPingPongBuffer[0]->getColorTexture(0);
        perImageATrousGradientCB["gGradientVariance"]       = mpGradientResultPingPongBuffer[0]->getColorTexture(1);
        perImageATrousGradientCB["gStepSize"]               = (1 << indexAtrous);

        mpPrgAtrousGradientCalculation->execute(pRenderContext, mpGradientResultPingPongBuffer[1], false);
        std::swap(mpGradientResultPingPongBuffer[0], mpGradientResultPingPongBuffer[1]);
    }

    //Temporal Accumulation
    auto perImageAccumulationCB = mpPrgTemporalAccumulation->getRootVar()["PerImageCB"];
    perImageAccumulationCB["gColor"]                        = pInputColorTexture;
    perImageAccumulationCB["gMotionVectorsTexture"]         = pInputMotionVectors;
    perImageAccumulationCB["gPrevAccumColorTexture"]        = mpPrevAccumulationBuffer->getColorTexture(0);
    perImageAccumulationCB["gPrevAccumMomentsTexture"]      = mpPrevAccumulationBuffer->getColorTexture(1);
    perImageAccumulationCB["gPrevHistLenTexture"]           = mpPrevAccumulationBuffer->getColorTexture(2);
    perImageAccumulationCB["gLinearZTexture"]               = pInputLinearZTexture;
    perImageAccumulationCB["gPrevLinearZTexture"]           = pInternalPrevLinearZTexture;
    perImageAccumulationCB["gNormalsTexture"]               = pInputNormalVectors;
    perImageAccumulationCB["gPrevNormalsTexture"]           = pInternalPrevNormalsTexture;
    perImageAccumulationCB["gPosNormalFWidth"]              = pInputPosNormalFWidth;
    perImageAccumulationCB["gVisibilityBuffer"]             = pInputCurrVisibilityBuffer;
    perImageAccumulationCB["gPrevVisibilityBuffer"]         = pInternalPrevVisBufferTexture;
    perImageAccumulationCB["gAlbedoTexture"]                = pInputAlbedoTexture;
    perImageAccumulationCB["gPrevAlbedoTexture"]            = pInternalPrevAlbedoTexture;
    perImageAccumulationCB["gEmissionTexture"]              = pInputEmissionTexture;
    perImageAccumulationCB["gPrevEmissionTexture"]          = pInternalPrevEmissionTexture;
    perImageAccumulationCB["gGradientDifferenceTexture"]    = mpGradientResultPingPongBuffer[0]->getColorTexture(0);
    perImageAccumulationCB["gGradientDownsample"]           = gradientDownsample;
    perImageAccumulationCB["gScreenDimension"]              = float2(screenWidth, screenHeight);
    perImageAccumulationCB["gJitterOffset"]                 = jitterOffset;
    perImageAccumulationCB["gTemporalColorAlpha"]           = mTemporalColorAlpha;
    perImageAccumulationCB["gTemporalMomentsAlpha"]         = mTemporalMomentsAlpha;
    perImageAccumulationCB["gGradientFilterRadius"]         = mGradientFilterRadius;
    
    mpPrgTemporalAccumulation->execute(pRenderContext, mpAccumulationBuffer);

    // Estimate variance
    auto perImageEstimateVarianceCB = mpPrgEstimateVariance->getRootVar()["PerImageCB"];

    perImageEstimateVarianceCB["gCurrentAccumColor"]    =   mpAccumulationBuffer->getColorTexture(0);
    perImageEstimateVarianceCB["gCurrentAccumMoments"]  =   mpAccumulationBuffer->getColorTexture(1);
    perImageEstimateVarianceCB["gCurrentAccumHistLen"]  =   mpAccumulationBuffer->getColorTexture(2);
    perImageEstimateVarianceCB["gLinearZTexture"]       =   pInputLinearZTexture;
    perImageEstimateVarianceCB["gNormalsTexture"]       =   pInputNormalVectors;
    perImageEstimateVarianceCB["gVisibilityBuffer"]     =   pInputCurrVisibilityBuffer;
    //perImageEstimateVarianceCB["gColorTest"]          =   mpTestColorTexture;


    mpPrgEstimateVariance->execute(pRenderContext, mpAtrousFullScreenResultPingPong[0]);


    if (mNumIterations == 0)
    {
        pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), mpAccumulationBuffer->getColorTexture(0)->getRTV());
    }

    // Atrous full screen

    auto perImageAtrousFullScreenCB = mpPrgAtrousFullScreen->getRootVar()["PerImageCB"];
    perImageAtrousFullScreenCB["gLinearZTexture"]   =   pInputLinearZTexture;
    perImageAtrousFullScreenCB["gNormalsTexture"]   =   pInputNormalVectors;
    perImageAtrousFullScreenCB["gAlbedoTexture"]    =   pInputAlbedoTexture;
    perImageAtrousFullScreenCB["gEmissionTexture"]  =   pInputEmissionTexture;
    perImageAtrousFullScreenCB["gPhiColor"]         =   weightPhiColor;
    perImageAtrousFullScreenCB["gPhiNormal"]        =   weightPhiNormal;
    perImageAtrousFullScreenCB["gScreenDimension"]  =   int2(screenWidth, screenHeight);
    

    for (int i = 0; i < mNumIterations; i++)
    {
        if ((i - 1) == mHistoryTap)
        {
            pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), mpAccumulationBuffer->getColorTexture(0)->getRTV());
        }

        perImageAtrousFullScreenCB["gColorAndVariance"] =   mpAtrousFullScreenResultPingPong[0]->getColorTexture(0);
        perImageAtrousFullScreenCB["gStepSize"]         =   (1 << i);
        perImageAtrousFullScreenCB["gIsModulateAlbedo"] =   int(i == (mNumIterations - 1));

        mpPrgAtrousFullScreen->execute(pRenderContext, mpAtrousFullScreenResultPingPong[1]);

        std::swap(mpAtrousFullScreenResultPingPong[0], mpAtrousFullScreenResultPingPong[1]);
    }

    //Blit outputs
    pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), pOutputFilteredImage->getRTV());

    // Swap buffers for next frame}
    pRenderContext->blit(pInputColorTexture->getSRV(),          pInternalPrevColorTexture->getRTV());
    pRenderContext->blit(pInputAlbedoTexture->getSRV(),         pInternalPrevAlbedoTexture->getRTV());
    pRenderContext->blit(pInputEmissionTexture->getSRV(),       pInternalPrevEmissionTexture->getRTV());
    pRenderContext->blit(pInputLinearZTexture->getSRV(),        pInternalPrevLinearZTexture->getRTV());
    pRenderContext->blit(pInputNormalVectors->getSRV(),         pInternalPrevNormalsTexture->getRTV());
    pRenderContext->blit(pInputCurrVisibilityBuffer->getSRV(),  pInternalPrevVisBufferTexture->getRTV());

    std::swap(mpAccumulationBuffer, mpPrevAccumulationBuffer);
    mPrevFrameJitter = cameraJitter;
}

void ASVGFPass::clearBuffers(RenderContext* pRenderContext, const RenderData& renderData)
{
    pRenderContext->clearFbo(mpGradientResultPingPongBuffer[0].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpGradientResultPingPongBuffer[1].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpAtrousFullScreenResultPingPong[0].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpAtrousFullScreenResultPingPong[1].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpAccumulationBuffer.get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpPrevAccumulationBuffer.get(), float4(0), 1.0f, 0, FboAttachmentType::All);

    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevColorTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevAlbedoTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevEmissionTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevLinearZTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevNormalsTexture).get());
    //pRenderContext->clearTexture(renderData.getTexture(kInternalPrevVisibilityBuffer).get());
}

void ASVGFPass::setScene(RenderContext* a_pRenderContext, const ref<Scene>& a_pScene)
{
    pScene = a_pScene;
    auto sceneDefines = pScene->getSceneDefines();

    mpPrgGradientForwardProjection  = FullScreenPass::create(mpDevice, kCreateGradientSamplesShader, sceneDefines);
    mpPrgAtrousGradientCalculation  = FullScreenPass::create(mpDevice, kAtrousGradientShader, sceneDefines);
    mpPrgTemporalAccumulation       = FullScreenPass::create(mpDevice, kTemporalAccumulationShader, sceneDefines);
    mpPrgEstimateVariance           = FullScreenPass::create(mpDevice, kEstimateVarianceShader, sceneDefines);
    mpPrgAtrousFullScreen           = FullScreenPass::create(mpDevice, kAtrousShader, sceneDefines);

    #if IS_DEBUG_PASS
    mpPrgDebugFullScreen = FullScreenPass::create(mpDevice, kDebugPassShader, sceneDefines);
    #endif IS_DEBUG_PASS
}

void ASVGFPass::renderUI(Gui::Widgets& widget)
{
    
    bool isDirty = widget.var("Temporal Color Alpha", mTemporalColorAlpha, 0.0f, 1.0f, 0.01f);
    isDirty |= widget.var("Temporal Moments Alpha", mTemporalMomentsAlpha, 0.0f, 1.0f, 0.01f);
    isDirty |= widget.var("# Iterations", mNumIterations, 0, 16, 1);
    isDirty |= widget.var("History Tap", mHistoryTap, -1, 16, 1);

    Falcor::Gui::DropdownList filterKernels{
        {0, "A-Trous"}, {1, "Box 3x3"}, {2, "Box 5x5"}, {3, "Sparse"}, {4, "Box3x3 / Sparse"}, {5, "Box5x5 / Sparse"},
    };
    isDirty |= widget.dropdown("Kernel", filterKernels, mFilterKernel);

    //widget.checkbox("Show Antilag Alpha", mShowAntilagAlpha);
    isDirty |= widget.var("# Diff Iterations", mDiffAtrousIterations, 0, 16, 1);
    isDirty |= widget.var("Gradient Filter Radius", mGradientFilterRadius, 0, 16, 1);

    isDirty |= widget.var("Weight Phi", weightPhiColor, 0.0f, 100.0f, 1.0f);
    isDirty |= widget.var("Weight Normal", weightPhiNormal, 0.0f, 128.0f, 1.0f);

    if (isDirty)
    {
        IsClearBuffers = true;
    }
}

#if IS_DEBUG_PASS
void ASVGFPass::debugPass(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pInputColorTexture = renderData.getTexture(kInputColorTexture);
    ref<Texture> pInputAlbedoTexture = renderData.getTexture(kInputAlbedoTexture);
    ref<Texture> pInputEmissionTexture = renderData.getTexture(kInputEmissionTexture);
    ref<Texture> pInputLinearZTexture = renderData.getTexture(kInputLinearZTexture);
    ref<Texture> pInputGradientSamples = renderData.getTexture(kInputGradientSamplesTexture);
    ref<Texture> pInputVisibilityBuffer = renderData.getTexture(kInputCurrVisibilityBufferTexture);
    ref<Texture> pInputMotionVectors = renderData.getTexture(kInputMotionVectors);
    ref<Texture> pInputNormalVectors = renderData.getTexture(kInputNormalsTexture);
    ref<Texture> pInputPosNormalFWidth = renderData.getTexture(kInputPosNormalFWidth);

    ref<Texture> pInternalPrevColorTexture = renderData.getTexture(kInternalPrevColorTexture);
    ref<Texture> pInternalPrevAlbedoTexture = renderData.getTexture(kInternalPrevAlbedoTexture);
    ref<Texture> pInternalPrevEmissionTexture = renderData.getTexture(kInternalPrevEmissionTexture);
    ref<Texture> pInternalPrevLinearZTexture = renderData.getTexture(kInternalPrevLinearZTexture);
    ref<Texture> pInternalPrevNormalsTexture = renderData.getTexture(kInternalPrevNormalsTexture);
    ref<Texture> pInternalPrevVisBufferTexture = renderData.getTexture(kInternalPrevVisibilityBuffer);

    int screenWidth = pInputColorTexture->getWidth();
    int screenHeight = pInputColorTexture->getHeight();

    float gradResWidth = gradient_res(screenWidth);
    float gradResHeight = gradient_res(screenHeight);

    GraphicsState::Viewport vp1(0, 0, gradResWidth, gradResHeight, 0, 1);
    auto gradforwardGraphicState = mpPrgDebugFullScreen->getState();
    gradforwardGraphicState->setViewport(0, vp1);

    auto perImageDebugFullScreenCB = mpPrgDebugFullScreen->getRootVar()["PerImageCB"];
    perImageDebugFullScreenCB["gColor"] = mpTestColorTexture;//mpGradientResultPingPongBuffer[0]->getColorTexture(2);
    perImageDebugFullScreenCB["gAlbedo"] = pInputAlbedoTexture;
    perImageDebugFullScreenCB["gEmission"] = pInputEmissionTexture;
    perImageDebugFullScreenCB["gGradientSample"] = pInputGradientSamples;

    perImageDebugFullScreenCB["gLinearZTexture"] = pInputLinearZTexture;
    perImageDebugFullScreenCB["gNormalsTexture"] = pInputNormalVectors;
    perImageDebugFullScreenCB["gVisibilityBuffer"] = pInputVisibilityBuffer;
    perImageDebugFullScreenCB["gPrevVisibilityBuffer"] = pInternalPrevVisBufferTexture;
    mpPrgDebugFullScreen->execute(pRenderContext, mpDebugBuffer);
}
#endif IS_DEBUG_PASS
