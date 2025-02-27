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
const char kTemporalAccumulationShader[]            = "RenderPasses/ASVGFPass/TemporalAccumulation.ps.slang";
const char kEstimateVarianceShader[]                = "RenderPasses/ASVGFPass/VarianceEstimation.ps.slang";
const char kAtrousShader[]                          = "RenderPasses/ASVGFPass/AtrousFullScreen.ps.slang";
const char kCreateGradientSamplesShader[]           = "RenderPasses/ASVGFPass/CreateGradientSamples.ps.slang";
const char kAtrousGradientShader[]                  = "RenderPasses/ASVGFPass/AtrousGradient.ps.slang";
const char kTemporalMutualInfCalcShader[]           = "RenderPasses/ASVGFPass/TemporalMutualInformationCalculation.ps.slang";
const char kSpatialMutualInfCalcShader[]            = "RenderPasses/ASVGFPass/SpatialMutualInformationCalculation.ps.slang";

#if IS_DEBUG_PASS
const char kDebugPassShader[] = "RenderPasses/ASVGFPass/DebugPass.ps.slang";
#endif IS_DEBUG_PASS

// Names of valid entries in the parameter dictionary.
const char kNumIterations[]             = "NumIterations";
const char kHistoryTap[]                = "HistoryTap";
const char kFilterKernel[]              = "FilterKernel";
const char kTemporalColorAlpha[]        = "TemporalColorAlpha";
const char kTemporalMomentsAlpha[]      = "TemporalMomentsAlpha";
const char kDiffAtrousIterations[]      = "DiffAtrousIterations";
const char kGradientFilterRadius[]      = "GradientFilterRadius";
const char kSpatialMutInfRadius[]       = "SpatialMutInfRadius";
const char kNumFramesInMICalc[]         = "NumFramesInMICalc";
const char kGradDiffRatioThreshold[]    = "GradDiffRatioThreshold";
const char kSpatialMIThreshold[]        = "SpatialMIThreshold";
const char kNumLumGroupsInMICalc[]      = "NumLumGroupsInMICalc";
const char kFrameBinCountInTempMI[]     = "FrameBinCountInTempMI";
const char kMinHistoryCountSpatialThreshold[] = "MinHistoryCountSpatialThreshold";
const char kSpatialLumBinCount[] = "SpatialLumBinCount";

//Input buffer names
const char kInputColorTexture[]                 = "Color";
const char kInputAlbedoTexture[]                = "Albedo";
const char kInputEmissionTexture[]              = "Emission";
const char kInputSpecularAlbedoTexture[]        = "SpecularAlbedo";
const char kInputLinearZTexture[]               = "LinearZ";
const char kInputNormalsTexture[]               = "Normals";
const char kInputCurrVisibilityBufferTexture[]  = "CurrentVisibilityBuffer";
const char kInputGradVisibilityBufferTexture[]  = "GradientVisibilityBuffer";
const char kInputGradientSamplesTexture[]       = "GradientSamples";
const char kInputMotionVectors[]                = "MotionVectors";

// Internal buffer names
const char kInternalPrevColorTexture[]      = "PrevColor";
const char kInternalPrevAlbedoTexture[]     = "PrevAlbedo";
const char kInternalPrevEmissionTexture[]   = "PrevEmission";
const char kInternalPrevLinearZTexture[]    = "PrevLinearZ";
const char kInternalPrevNormalsTexture[]    = "PrevNormals";
const char kInternalPrevVisibilityBuffer[]  = "PrevVisBuffer";
const char kInternalPrevMutualInfResult[]   = "PrevMutInfResult";
const char kInternalPrevSpecularAlbedoTexture[] = "PrevSpecularAlbedo";

// Output buffer name
const char kOutputBufferFilteredImage[] = "Filtered image";
const char kDebugOutputBufferImage[] = "Debug Output image";

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
        else if (key == kSpatialMutInfRadius)   mSpatialMutualInfRadius = value;
        else if (key == kGradDiffRatioThreshold)mGradDiffRatioThreshold = value;
        else if (key == kSpatialMIThreshold)    mSpatialMIThreshold = value;
        else if (key == kFrameBinCountInTempMI)    mFrameLumBinCountInTempMI = value;
        else if (key == kMinHistoryCountSpatialThreshold) mMinHistoryCountSpatialThreshold = value;
        else if (key == kSpatialLumBinCount) mSpatialLumBinCount= value;
        
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
    props[kSpatialMutInfRadius]     = mSpatialMutualInfRadius;
    props[kNumFramesInMICalc]       = mNumFramesInMICalc;
    props[kGradDiffRatioThreshold]  = mGradDiffRatioThreshold;
    props[kSpatialMIThreshold]      = mSpatialMIThreshold;
    props[kFrameBinCountInTempMI]   = mFrameLumBinCountInTempMI;
    props[kMinHistoryCountSpatialThreshold] = mMinHistoryCountSpatialThreshold;
    props[kSpatialLumBinCount] = mSpatialLumBinCount;

    return props;
}

void ASVGFPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    screenWidth = compileData.defaultTexDims.x;
    screenHeight = compileData.defaultTexDims.y;

    allocateBuffers(pRenderContext);
    IsClearBuffers = true;
}

RenderPassReflection ASVGFPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;

    // Input
    reflector.addInput(kInputColorTexture, "Color");
    reflector.addInput(kInputAlbedoTexture, "Albedo");
    reflector.addInput(kInputEmissionTexture, "Emission");
    reflector.addInput(kInputSpecularAlbedoTexture, "SpecularAlbedo");
    reflector.addInput(kInputLinearZTexture, "LinearZ");
    reflector.addInput(kInputNormalsTexture, "Normals");
    reflector.addInput(kInputGradVisibilityBufferTexture, "GradientVisibilityBuffer").format(ResourceFormat::RGBA32Uint);
    reflector.addInput(kInputCurrVisibilityBufferTexture, "CurrentVisibilityBuffer").format(ResourceFormat::RGBA32Uint);
    reflector.addInput(kInputMotionVectors, "MotionVectors");
    reflector.addInput(kInputGradientSamplesTexture, "GradSamples").format(ResourceFormat::R32Uint);

    // Internal buffers
    reflector.addInternal(kInternalPrevColorTexture, "Previous Color")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevAlbedoTexture, "Previous Albedo")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevEmissionTexture, "Previous Emission")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevLinearZTexture, "Previous LinearZ")
        .format(ResourceFormat::RG32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevNormalsTexture, "Previous Normals")
        .format(ResourceFormat::RGBA32Float)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevVisibilityBuffer, "Previous Visibility Buffer").format(ResourceFormat::RGBA32Uint)
        .bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource | ResourceBindFlags::UnorderedAccess);

    reflector.addInternal(kInternalPrevMutualInfResult, "Previous Mutual Inf Result")
        .format(ResourceFormat::RGBA32Float).bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);

    reflector.addInternal(kInternalPrevSpecularAlbedoTexture, "Previous Specular Albedo")
        .format(ResourceFormat::RGBA8Unorm).bindFlags(ResourceBindFlags::RenderTarget | ResourceBindFlags::ShaderResource);
    
    /// Output image i.e. reconstructed image, (marked as output in the GraphEditor)
    reflector.addOutput(kOutputBufferFilteredImage, "Filtered image").format(ResourceFormat::RGBA32Float);

#if IS_DEBUG_PASS
    reflector.addOutput(kDebugOutputBufferImage, "Debug output image").format(ResourceFormat::RGBA32Float);
#endif IS_DEBUG_PASS
    
    return reflector;
}

void ASVGFPass::allocateBuffers(RenderContext* a_pRenderContext)
{
    int gradResWidth = gradient_res(screenWidth);
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
    formatDescAccumulationResult.setSampleCount(0);
    formatDescAccumulationResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float); // Accumulation color
    formatDescAccumulationResult.setColorTarget(1, Falcor::ResourceFormat::RG32Float);   // Accumulation moments
    formatDescAccumulationResult.setColorTarget(2, Falcor::ResourceFormat::R16Float);    // Accumulation length
    formatDescAccumulationResult.setColorTarget(3, Falcor::ResourceFormat::R16Float);    // Gradient difference ratio
    mpAccumulationBuffer = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatDescAccumulationResult);
    mpPrevAccumulationBuffer = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatDescAccumulationResult);

    // Atrous full screen
    Fbo::Desc formatAtrousFullScreenResult;
    formatAtrousFullScreenResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float); // Color.rgb, variance
    mpAtrousFullScreenResultPingPong[0] = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatAtrousFullScreenResult);
    mpAtrousFullScreenResultPingPong[1] = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatAtrousFullScreenResult);

    // Mutual inf result
    Fbo::Desc formatMutualInfDesc;
    formatMutualInfDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);    //Luminance Sum, Mutual Information, History Count
    formatMutualInfDesc.setColorTarget(1, Falcor::ResourceFormat::RGBA32Float);     // Accumulated rgb, temporal MI
    mpTemporalMutualInfResultBuffer = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatMutualInfDesc);

#if IS_DEBUG_PASS
    Fbo::Desc formatDebugFullScreenResult;
    formatDebugFullScreenResult.setSampleCount(0);
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

    if (IsClearBuffers)
    {
        resetBuffers(pRenderContext, renderData);
        IsClearBuffers = false;
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
    ref<Texture> pInputSpecularAlbedo           = renderData.getTexture(kInputSpecularAlbedoTexture);
    
    ref<Texture> pInternalPrevColorTexture      = renderData.getTexture(kInternalPrevColorTexture);
    ref<Texture> pInternalPrevAlbedoTexture     = renderData.getTexture(kInternalPrevAlbedoTexture);
    ref<Texture> pInternalPrevEmissionTexture   = renderData.getTexture(kInternalPrevEmissionTexture);
    ref<Texture> pInternalPrevLinearZTexture    = renderData.getTexture(kInternalPrevLinearZTexture);
    ref<Texture> pInternalPrevNormalsTexture    = renderData.getTexture(kInternalPrevNormalsTexture);
    ref<Texture> pInternalPrevVisBufferTexture  = renderData.getTexture(kInternalPrevVisibilityBuffer);
    ref<Texture> pInternalPrevMutualInfTexture  = renderData.getTexture(kInternalPrevMutualInfResult);
    ref<Texture> pInternalPrevSpecularAlbedo    = renderData.getTexture(kInternalPrevSpecularAlbedoTexture);
    
    ref<Texture> pOutputFilteredImage = renderData.getTexture(kOutputBufferFilteredImage);

    #if IS_DEBUG_PASS
        ref<Texture> pDebugOutputImage = renderData.getTexture(kDebugOutputBufferImage);
    #endif
    
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

    //logWarning("jitterOffset  : {} , {} ", cameraJitter.x, cameraJitter.y);

    GraphicsState::Viewport vpGradRes(0, 0, gradResWidth, gradResHeight, 0, 1);

    // Gradient creation pass
    {
        auto gradForwardGraphicState = mpPrgGradientForwardProjection->getState();
        gradForwardGraphicState->setViewport(0, vpGradRes);

        auto perImageGradForwardProjCB = mpPrgGradientForwardProjection->getRootVar()["PerImageCB"];
        perImageGradForwardProjCB["gColorTexture"]          = pInputColorTexture;
        perImageGradForwardProjCB["gPrevColorTexture"]      = pInternalPrevColorTexture;
        perImageGradForwardProjCB["gAlbedoTexture"]         = pInputAlbedoTexture;
        perImageGradForwardProjCB["gPrevAlbedoTexture"]     = pInternalPrevAlbedoTexture;
        perImageGradForwardProjCB["gEmissionTexture"]       = pInputEmissionTexture;
        perImageGradForwardProjCB["gPrevEmissionTexture"]   = pInternalPrevEmissionTexture;
        perImageGradForwardProjCB["gSpecularAlbedo"]        = pInputSpecularAlbedo;
        perImageGradForwardProjCB["gPrevSpecularAlbedo"]    = pInternalPrevSpecularAlbedo;
        perImageGradForwardProjCB["gGradientSamples"]       = pInputGradientSamples;
        perImageGradForwardProjCB["gVisibilityBuffer"]      = pInputCurrVisibilityBuffer;
        perImageGradForwardProjCB["gLinearZTexture"]        = pInputLinearZTexture;
        perImageGradForwardProjCB["gGradientDownsample"]    = gradientDownsample;
        perImageGradForwardProjCB["gScreenWidth"]           = screenWidth;
#if IS_DEBUG_PASS
        perImageGradForwardProjCB["gColorTest"] = mpTestColorTexture;
#endif

        mpPrgGradientForwardProjection->execute(pRenderContext, mpGradientResultPingPongBuffer[0], true);
    }

    //A-trous gradient
    {
        auto atrousGradCalcGraphicState = mpPrgAtrousGradientCalculation->getState();
        atrousGradCalcGraphicState->setViewport(0, vpGradRes);

        auto perImageATrousGradientCB = mpPrgAtrousGradientCalculation->getRootVar()["PerImageCB"];
        perImageATrousGradientCB["gGradientResDimensions"] = float2(gradResWidth, gradResHeight);
        perImageATrousGradientCB["gGradientDownsample"] = gradientDownsample;
        perImageATrousGradientCB["gPhiColor"] = weightPhiColor;

        for (int indexAtrous = 0; indexAtrous < mDiffAtrousIterations; indexAtrous++)
        {
            perImageATrousGradientCB["gGradientLuminance"] = mpGradientResultPingPongBuffer[0]->getColorTexture(0);
            perImageATrousGradientCB["gGradientVariance"] = mpGradientResultPingPongBuffer[0]->getColorTexture(1);
            perImageATrousGradientCB["gStepSize"] = (1 << indexAtrous);

            mpPrgAtrousGradientCalculation->execute(pRenderContext, mpGradientResultPingPongBuffer[1], true);
            std::swap(mpGradientResultPingPongBuffer[0], mpGradientResultPingPongBuffer[1]);
        }
    }
    
    //Temporal Accumulation
    {
        auto perImageAccumulationCB = mpPrgTemporalAccumulation->getRootVar()["PerImageCB"];
        perImageAccumulationCB["gColor"] = pInputColorTexture;
        perImageAccumulationCB["gMotionVectorsTexture"] = pInputMotionVectors;
        perImageAccumulationCB["gPrevAccumColorTexture"] = mpPrevAccumulationBuffer->getColorTexture(0);
        perImageAccumulationCB["gPrevAccumMomentsTexture"] = mpPrevAccumulationBuffer->getColorTexture(1);
        perImageAccumulationCB["gPrevHistLenTexture"] = mpPrevAccumulationBuffer->getColorTexture(2);
        perImageAccumulationCB["gLinearZTexture"] = pInputLinearZTexture;
        perImageAccumulationCB["gPrevLinearZTexture"] = pInternalPrevLinearZTexture;
        perImageAccumulationCB["gNormalsTexture"] = pInputNormalVectors;
        perImageAccumulationCB["gPrevNormalsTexture"] = pInternalPrevNormalsTexture;
        perImageAccumulationCB["gVisibilityBuffer"] = pInputCurrVisibilityBuffer;
        perImageAccumulationCB["gPrevVisibilityBuffer"] = pInternalPrevVisBufferTexture;
        perImageAccumulationCB["gAlbedoTexture"] = pInputAlbedoTexture;
        perImageAccumulationCB["gPrevAlbedoTexture"] = pInternalPrevAlbedoTexture;
        perImageAccumulationCB["gEmissionTexture"] = pInputEmissionTexture;
        perImageAccumulationCB["gSpecularAlbedo"] = pInputSpecularAlbedo;
        perImageAccumulationCB["gPrevSpecularAlbedo"] = pInternalPrevSpecularAlbedo;
        perImageAccumulationCB["gGradientDifferenceTexture"] = mpGradientResultPingPongBuffer[0]->getColorTexture(0);
        perImageAccumulationCB["gGradientDownsample"] = gradientDownsample;
        perImageAccumulationCB["gScreenDimension"] = float2(screenWidth, screenHeight);
        perImageAccumulationCB["gJitterOffset"] = jitterOffset;
        perImageAccumulationCB["gTemporalColorAlpha"] = mTemporalColorAlpha;
        perImageAccumulationCB["gTemporalMomentsAlpha"] = mTemporalMomentsAlpha;
        perImageAccumulationCB["gGradientFilterRadius"] = mGradientFilterRadius;
        
#if IS_DEBUG_PASS
        perImageAccumulationCB["gColorTest"] = mpTestColorTexture;
#endif

        mpPrgTemporalAccumulation->execute(pRenderContext, mpAccumulationBuffer);
    }

    // Estimate variance
    {
        auto perImageEstimateVarianceCB = mpPrgEstimateVariance->getRootVar()["PerImageCB"];

        perImageEstimateVarianceCB["gCurrentAccumColor"] = mpAccumulationBuffer->getColorTexture(0);
        perImageEstimateVarianceCB["gCurrentAccumMoments"] = mpAccumulationBuffer->getColorTexture(1);
        perImageEstimateVarianceCB["gCurrentAccumHistLen"] = mpAccumulationBuffer->getColorTexture(2);
        perImageEstimateVarianceCB["gLinearZTexture"] = pInputLinearZTexture;
        perImageEstimateVarianceCB["gNormalsTexture"] = pInputNormalVectors;
        perImageEstimateVarianceCB["gVisibilityBuffer"] = pInputCurrVisibilityBuffer;
#if IS_DEBUG_PASS
        perImageEstimateVarianceCB["gColorTest"] = mpTestColorTexture;
#endif

        mpPrgEstimateVariance->execute(pRenderContext, mpAtrousFullScreenResultPingPong[0]);
    }

    // Mutual information computation
    if (mUseMutualInformation)
    {
        //Temporal Mutual information calculation
        if (mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_ONLY_TEMPORAL ||
            mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_TEMPORAL_AND_SPATIAL)
        {
            if (mdqTimeStep.size() == mNumFramesInMICalc)
            {
                mdqTimeStep.pop_back();
            }
            mClock.tick();
            if (mCurrentLinearlyIncrTemporalVal == LinearlyIncreasingTemporalValue::TIME_STEP)
            {
                mdqTimeStep.push_front((float)mClock.getTime()); // With time-step
            }
            else
            {
                mdqTimeStep.push_front((float)(mCurrentFrameNumber + 1)); //With frame number
            }
            
            std::vector<float> lvecTimeSteps {mdqTimeStep.begin(), mdqTimeStep.end()};
            mpFrameTimeSteps->setBlob(&lvecTimeSteps[0], 0, sizeof(float) * lvecTimeSteps.size());

            auto perImageTemporalMutualInfCalcCB = mpPrgTemporalMutualInfCalc->getRootVar()["PerImageCB"];
            perImageTemporalMutualInfCalcCB["gColorAndVariance"]        = mpAtrousFullScreenResultPingPong[0]->getColorTexture(0);
            perImageTemporalMutualInfCalcCB["gColor"]                   = pInputColorTexture;
            perImageTemporalMutualInfCalcCB["gTimeStepBuffer"]           = mpFrameTimeSteps;
            perImageTemporalMutualInfCalcCB["gAlbedoTexture"]           = pInputAlbedoTexture;
            perImageTemporalMutualInfCalcCB["gEmissionTexture"]         = pInputEmissionTexture;
            perImageTemporalMutualInfCalcCB["gSpecularAlbedo"]          = pInputSpecularAlbedo;
            perImageTemporalMutualInfCalcCB["gLinearZTexture"]          = pInputLinearZTexture;
            perImageTemporalMutualInfCalcCB["gPrevLinearZTexture"]      = pInternalPrevLinearZTexture;
            perImageTemporalMutualInfCalcCB["gNormalsTexture"]          = pInputNormalVectors;
            perImageTemporalMutualInfCalcCB["gPrevNormalsTexture"]      = pInternalPrevNormalsTexture;
            perImageTemporalMutualInfCalcCB["gVisibilityBuffer"]        = pInputCurrVisibilityBuffer;
            perImageTemporalMutualInfCalcCB["gPrevVisibilityBuffer"]    = pInternalPrevVisBufferTexture;
            perImageTemporalMutualInfCalcCB["gMotionVectorsTexture"]    = pInputMotionVectors;
            perImageTemporalMutualInfCalcCB["gGradDifferenceRatio"]     = mpAccumulationBuffer->getColorTexture(3);
            perImageTemporalMutualInfCalcCB["gPrevMutualInfBuffer"]     = mpPrevMutualInformationCalcBuffer;
            perImageTemporalMutualInfCalcCB["gMutualInfBuffer"]         = mpMutualInformationCalcBuffer;
            perImageTemporalMutualInfCalcCB["gPrevMutualInfResult"]     = pInternalPrevMutualInfTexture;
            perImageTemporalMutualInfCalcCB["gScreenDimension"]         = float2(screenWidth, screenHeight);
            perImageTemporalMutualInfCalcCB["gTotalPixelsInFrame"]      = screenWidth * screenHeight;
            perImageTemporalMutualInfCalcCB["gInfCalcType"]             = (uint32_t)mInfCalcType;
            
#if IS_DEBUG_PASS
            perImageTemporalMutualInfCalcCB["gTemporalDebugBuffer"] = mpTemporalDebugMICalc;
            perImageTemporalMutualInfCalcCB["gColorTest"] = mpTestColorTexture;
            perImageTemporalMutualInfCalcCB["gDebugPixel"] = mDebugSelectedPixel;
            
#endif
            mpPrgTemporalMutualInfCalc->execute(pRenderContext, mpTemporalMutualInfResultBuffer);
            pRenderContext->blit(mpTemporalMutualInfResultBuffer->getColorTexture(1)->getSRV(), mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getRTV());
      
#if IS_DEBUG_PASS // Print temporal MI CALC
            if (mDebugLogMICalc)
            {
                pRenderContext->flush();
                float* lRead = reinterpret_cast<float*>(mpTemporalDebugMICalc->map(Falcor::Buffer::MapType::Read));

                float historyLength = lRead[0];
                float MIResult = lRead[1];

                logInfo("=========TEMPORAL START============");

                logInfo("History = {}\nMI = {}\n", lRead[0], lRead[1]);
                logInfo("Luminance Values");
                for (int historyIndex = 0; historyIndex < historyLength; historyIndex++)
                {
                    logInfo(
                        "{} Lum Val : {}    ,   TimeStep : {}   ,   TimeStepProb : {} \n", historyIndex, lRead[2 + historyIndex],
                        lRead[2 + historyIndex + mNumFramesInMICalc], lRead[2 + historyIndex + mNumFramesInMICalc + mNumFramesInMICalc]
                    );
                }

                logInfo("\nLum Bucket Values\n");
                for (int bucketIndex = 0; bucketIndex < mFrameLumBinCountInTempMI; bucketIndex++)
                {
                    logInfo(
                        "Buc Val {} : {}\n", bucketIndex,
                        lRead[2 + mNumFramesInMICalc + mNumFramesInMICalc + mNumFramesInMICalc + bucketIndex]
                    );
                }
                mpTemporalDebugMICalc->unmap();
                logInfo("=========TEMPORAL END============");
            }
#endif
}

        // Spatial Mutual information calculation
        if (mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_ONLY_SPATIAL ||
            mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_TEMPORAL_AND_SPATIAL)
        {
            auto perImageSpatialMutualInfCalcCB = mpPrgSpatialMutualInfCalc->getRootVar()["PerImageCB"];
            perImageSpatialMutualInfCalcCB["gColor"] = pInputColorTexture;
            perImageSpatialMutualInfCalcCB["gAlbedoTexture"] = pInputAlbedoTexture;
            perImageSpatialMutualInfCalcCB["gEmissionTexture"] = pInputEmissionTexture;
            perImageSpatialMutualInfCalcCB["gSpecularAlbedo"] = pInputSpecularAlbedo;
            perImageSpatialMutualInfCalcCB["gColorAndVariance"]     = mpAtrousFullScreenResultPingPong[0]->getColorTexture(0); //switch variance and output mutual inf
            perImageSpatialMutualInfCalcCB["gLinearZTexture"]       = pInputLinearZTexture;
            perImageSpatialMutualInfCalcCB["gNormalsTexture"]       = pInputNormalVectors;
            perImageSpatialMutualInfCalcCB["gVisibilityBuffer"]     = pInputCurrVisibilityBuffer;
            perImageSpatialMutualInfCalcCB["gScreenDimension"]      = float2(screenWidth, screenHeight);
            perImageSpatialMutualInfCalcCB["gTemporalMutualInfResult"]  = mpTemporalMutualInfResultBuffer->getColorTexture(0);
            perImageSpatialMutualInfCalcCB["gMinHistoryCount"]      = mMinHistoryCountSpatialThreshold;
            perImageSpatialMutualInfCalcCB["gGradDifferenceRatio"]  = mpAccumulationBuffer->getColorTexture(3);
            perImageSpatialMutualInfCalcCB["gGradDiffRatioThreshold"] = mGradDiffRatioThreshold;
            perImageSpatialMutualInfCalcCB["gSpatialMIThreshold"] = mSpatialMIThreshold;
            perImageSpatialMutualInfCalcCB["gInfCalcType"] = (uint32_t)mInfCalcType;
#if IS_DEBUG_PASS
            perImageSpatialMutualInfCalcCB["gColorTest"] = mpTestColorTexture;
            perImageSpatialMutualInfCalcCB["gSpatialDebugBuffer"] = mpSpatialDebugMICalc;
            perImageSpatialMutualInfCalcCB["gDebugPixel"] = mDebugSelectedPixel;
#endif
            mpPrgSpatialMutualInfCalc->execute(pRenderContext, mpAtrousFullScreenResultPingPong[0]);

#if IS_DEBUG_PASS // Print spatial MI CALC
            if (mDebugLogMICalc)
            {
                pRenderContext->flush();
                float* lRead = reinterpret_cast<float*>(mpSpatialDebugMICalc->map(Falcor::Buffer::MapType::Read));

                float acceptedPixelCount = lRead[0];
                float MIResult = lRead[1];

                logInfo("=========SPATIAL START============");

                const int SPATIAL_PIXEL_BIN_COUNT = (mSpatialMutualInfRadius * 2 + 1) * (mSpatialMutualInfRadius * 2 + 1);

                logInfo("AcceptedPixelCount = {}\nMI = {}\n", lRead[0], lRead[1]);
                logInfo("Pixel Index Values");
                for (int pixelIndex = 0; pixelIndex < acceptedPixelCount; pixelIndex++)
                {
                    logInfo("PixelIndex : {}    \n", lRead[2 + pixelIndex]);
                }

                logInfo("Lum Values");
                for (int pixelIndex = 0; pixelIndex < SPATIAL_PIXEL_BIN_COUNT; pixelIndex++)
                {
                    logInfo("LumValue : {}\n", lRead[2 + pixelIndex + SPATIAL_PIXEL_BIN_COUNT]);
                }

                logInfo("Lum Bucket Prob Values");
                for (int pixelIndex = 0; pixelIndex < SPATIAL_PIXEL_BIN_COUNT; pixelIndex++)
                {
                    logInfo("LumBucketProbValue : {}\n", lRead[2 + pixelIndex + SPATIAL_PIXEL_BIN_COUNT + SPATIAL_PIXEL_BIN_COUNT]);
                }

                mpTemporalDebugMICalc->unmap();
                logInfo("=========SPATIAL END============");
            }
#endif
        }
    }

    if (mNumIterations == 0)
    {
        pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), mpAccumulationBuffer->getColorTexture(0)->getRTV());
    }

    // Atrous full screen
    {
        auto perImageAtrousFullScreenCB = mpPrgAtrousFullScreen->getRootVar()["PerImageCB"];
        perImageAtrousFullScreenCB["gLinearZTexture"]       = pInputLinearZTexture;
        perImageAtrousFullScreenCB["gNormalsTexture"]       = pInputNormalVectors;
        perImageAtrousFullScreenCB["gAlbedoTexture"]        = pInputAlbedoTexture;
        perImageAtrousFullScreenCB["gEmissionTexture"]      = pInputEmissionTexture;
        perImageAtrousFullScreenCB["gSpecularAlbedo"]       = pInputSpecularAlbedo;
        perImageAtrousFullScreenCB["gPhiColor"]             = weightPhiColor;
        perImageAtrousFullScreenCB["gPhiNormal"]            = weightPhiNormal;
        perImageAtrousFullScreenCB["gScreenDimension"]      = int2(screenWidth, screenHeight);
        perImageAtrousFullScreenCB["gIsUseMutualInf"]       = mUseMutualInformation;
        perImageAtrousFullScreenCB["gGradDifferenceRatio"]  = mpAccumulationBuffer->getColorTexture(3);
#if IS_DEBUG_PASS
        perImageAtrousFullScreenCB["gColorTest"] = mpTestColorTexture;
#endif

        for (int i = 0; i < mNumIterations; i++)
        {
            if ((i - 1) == mHistoryTap)
            {
                pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), mpAccumulationBuffer->getColorTexture(0)->getRTV());
            }

            perImageAtrousFullScreenCB["gColorAndVariance"] = mpAtrousFullScreenResultPingPong[0]->getColorTexture(0);
            perImageAtrousFullScreenCB["gStepSize"] = (1 << i);
            perImageAtrousFullScreenCB["gIsModulateAlbedo"] = int(i == (mNumIterations - 1));

            mpPrgAtrousFullScreen->execute(pRenderContext, mpAtrousFullScreenResultPingPong[1]);

            std::swap(mpAtrousFullScreenResultPingPong[0], mpAtrousFullScreenResultPingPong[1]);
        }
    }

    //Blit outputs
    #if IS_DEBUG_PASS
        debugPass(pRenderContext, renderData);
        pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), pOutputFilteredImage->getRTV());
        pRenderContext->blit(mpDebugBuffer->getColorTexture(0)->getSRV(), pDebugOutputImage->getRTV());
        pRenderContext->clearTexture(mpTestColorTexture.get());
        pRenderContext->clearFbo(mpDebugBuffer.get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    #else
        pRenderContext->blit(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0)->getSRV(), pOutputFilteredImage->getRTV());
    #endif

    // Swap buffers for next frame}
    pRenderContext->blit(pInputColorTexture->getSRV(),          pInternalPrevColorTexture->getRTV());
    pRenderContext->blit(pInputAlbedoTexture->getSRV(),         pInternalPrevAlbedoTexture->getRTV());
    pRenderContext->blit(pInputEmissionTexture->getSRV(),       pInternalPrevEmissionTexture->getRTV());
    pRenderContext->blit(pInputLinearZTexture->getSRV(),        pInternalPrevLinearZTexture->getRTV());
    pRenderContext->blit(pInputNormalVectors->getSRV(),         pInternalPrevNormalsTexture->getRTV());
    pRenderContext->blit(pInputCurrVisibilityBuffer->getSRV(),  pInternalPrevVisBufferTexture->getRTV());
    pRenderContext->blit(pInputSpecularAlbedo->getSRV(),        pInternalPrevSpecularAlbedo->getRTV());
    pRenderContext->blit(mpTemporalMutualInfResultBuffer->getColorTexture(0)->getSRV(), pInternalPrevMutualInfTexture->getRTV());
    
    std::swap(mpAccumulationBuffer, mpPrevAccumulationBuffer);
    std::swap( mpMutualInformationCalcBuffer, mpPrevMutualInformationCalcBuffer);
    mPrevFrameJitter = cameraJitter;
    mCurrentFrameNumber++;

    #if IS_DEBUG_PASS
    if (isConductLightFlickerTest)
    {
        if (testMat)
        {
            Falcor::ref<Falcor::Material> clight = pScene->getMaterialByName(mEmissiveMatName);
            Falcor::StandardMaterial* smLight = reinterpret_cast<Falcor::StandardMaterial*>(clight.get());

            if (mCurrentLightFlickerFrameIndex == 0)
            {
                mOldLightIntensityVal = smLight->getEmissiveFactor();
            }
        
            float cLightIntensityVal = 0.0;
            if (mCurrentLightFlickerFrameIndex < (mNumFramesLightTest - 1))
            {
                cLightIntensityVal = mLightValuesPerFrame[mCurrentLightFlickerFrameIndex];
                logWarning("IntensityVal: {}", cLightIntensityVal);
            }
            else if (mCurrentLightFlickerFrameIndex == (mNumFramesLightTest - 1))
            {
                cLightIntensityVal = mOldLightIntensityVal;
            }
            else
            {
                cLightIntensityVal = mOldLightIntensityVal;
                isConductLightFlickerTest = false;
                takeOutputScreenshot(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0));
            }

            smLight->setEmissiveFactor(cLightIntensityVal);
            mCurrentLightFlickerFrameIndex++;
        }
        else
        {
            Falcor::ref<Falcor::Light> clight = pScene->getLight(mLightId);
            Falcor::PointLight* smLight = reinterpret_cast<Falcor::PointLight*>(clight.get());

            if (mCurrentLightFlickerFrameIndex == 0)
            {
                mOldLightIntensityVal = smLight->getIntensityForUI();
            }

            float cLightIntensityVal = 0.0;
            if (mCurrentLightFlickerFrameIndex < (mNumFramesLightTest - 1))
            {
                cLightIntensityVal = mLightValuesPerFrame[mCurrentLightFlickerFrameIndex];
                logWarning("IntensityVal: {}", cLightIntensityVal);
            }
            else if (mCurrentLightFlickerFrameIndex == (mNumFramesLightTest - 1))
            {
                cLightIntensityVal = mOldLightIntensityVal;
            }
            else
            {
                cLightIntensityVal = mOldLightIntensityVal;
                isConductLightFlickerTest = false;
                takeOutputScreenshot(mpAtrousFullScreenResultPingPong[0]->getColorTexture(0));
            }

            smLight->setIntensityFromUI(cLightIntensityVal);
            mCurrentLightFlickerFrameIndex++;
        }
    }
    
    #endif
}

void ASVGFPass::resetBuffers(RenderContext* pRenderContext, const RenderData& renderData)
{
    pRenderContext->clearFbo(mpGradientResultPingPongBuffer[0].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpGradientResultPingPongBuffer[1].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpAtrousFullScreenResultPingPong[0].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpAtrousFullScreenResultPingPong[1].get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpAccumulationBuffer.get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpPrevAccumulationBuffer.get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    pRenderContext->clearFbo(mpTemporalMutualInfResultBuffer.get(), float4(0), 1.0f, 0, FboAttachmentType::All);
    
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevColorTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevAlbedoTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevEmissionTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevLinearZTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevNormalsTexture).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevMutualInfResult).get());
    pRenderContext->clearTexture(renderData.getTexture(kInternalPrevSpecularAlbedoTexture).get());

    pRenderContext->clearUAV(renderData.getTexture(kInternalPrevVisibilityBuffer)->getUAV().get(), uint4(0, 0, 0, 0));
    
    if (mUseMutualInformation)
    {
        uint2 textureDims = renderData.getDefaultTextureDims();
        mpMutualInformationCalcBuffer =
            mpDevice->createBuffer(textureDims.x * textureDims.y * mNumFramesInMICalc * sizeof(float), ResourceBindFlags::UnorderedAccess);
        mpPrevMutualInformationCalcBuffer =
            mpDevice->createBuffer(textureDims.x * textureDims.y * mNumFramesInMICalc * sizeof(float), ResourceBindFlags::UnorderedAccess);
        mpFrameTimeSteps = mpDevice->createBuffer(mNumFramesInMICalc * sizeof(float), ResourceBindFlags::UnorderedAccess);

        auto sceneDefines = pScene->getSceneDefines();
        DefineList temporalDefines(sceneDefines);
        temporalDefines.add("LUM_FRAME_BIN_COUNT", std::to_string(mFrameLumBinCountInTempMI));
        temporalDefines.add("FRAME_HISTORY_COUNT", std::to_string(mNumFramesInMICalc));
        
        DefineList spatialDefines(sceneDefines);
        spatialDefines.add("SPATIAL_RADIUS", std::to_string(mSpatialMutualInfRadius));
        spatialDefines.add("SPATIAL_LUM_BIN_COUNT", std::to_string(mSpatialLumBinCount));

#if IS_DEBUG_PASS
    temporalDefines.add("IS_DEBUG_PASS", std::to_string(1));
    spatialDefines.add("IS_DEBUG_PASS", std::to_string(1));
    
    mpTemporalDebugMICalc = Buffer::create(
        mpDevice,
        (sizeof(float) /* History */ + sizeof(float) /* MI */ + mNumFramesInMICalc * sizeof(float) + mNumFramesInMICalc * sizeof(float) +
         mNumFramesInMICalc * sizeof(float) + mFrameLumBinCountInTempMI * sizeof(float)),
        Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Falcor::Buffer::CpuAccess::None
    );

    const int SPATIAL_PIXEL_BIN_COUNT = (mSpatialMutualInfRadius * 2 + 1) * (mSpatialMutualInfRadius * 2 + 1);
    mpSpatialDebugMICalc = Buffer::create(
        mpDevice,
        sizeof(float) /* accepted pixel count */ + sizeof(float) /* MI */ + SPATIAL_PIXEL_BIN_COUNT * sizeof(float) +
            SPATIAL_PIXEL_BIN_COUNT * sizeof(float) + SPATIAL_PIXEL_BIN_COUNT * sizeof(float),
        Resource::BindFlags::ShaderResource | Resource::BindFlags::UnorderedAccess, Falcor::Buffer::CpuAccess::None
    );
#else
    temporalDefines.add("IS_DEBUG_PASS", std::to_string(0));
    spatialDefines.add("IS_DEBUG_PASS", std::to_string(0));


#endif IS_DEBUG_PASS

        mpPrgTemporalMutualInfCalc = FullScreenPass::create(mpDevice, kTemporalMutualInfCalcShader, temporalDefines);
        mpPrgSpatialMutualInfCalc = FullScreenPass::create(mpDevice, kSpatialMutualInfCalcShader, spatialDefines);
    }

#if IS_DEBUG_PASS
    // Setup light random intensity values
    srand(1);
    mLightValuesPerFrame.clear();
    for (int i = 0; i < (mNumFramesLightTest - 1); i++)
    {
        mLightValuesPerFrame.push_back((rand() % (int)(mMaxLightValue - mMinLightValue)) + mMinLightValue);
    }
#endif IS_DEBUG_PASS

    mCurrentFrameNumber = 0;
    mdqTimeStep.clear();
}

void ASVGFPass::setScene(RenderContext* a_pRenderContext, const ref<Scene>& a_pScene)
{
    pScene = a_pScene;
    IsClearBuffers  = true;

    auto sceneDefines = pScene->getSceneDefines();
    DefineList newDefines(sceneDefines);

#if IS_DEBUG_PASS
    newDefines.add("IS_DEBUG_PASS", std::to_string(1));
#else
    newDefines.add("IS_DEBUG_PASS", std::to_string(0));
#endif IS_DEBUG_PASS

    mpPrgGradientForwardProjection  = FullScreenPass::create(mpDevice, kCreateGradientSamplesShader, newDefines);
    mpPrgAtrousGradientCalculation  = FullScreenPass::create(mpDevice, kAtrousGradientShader, newDefines);
    mpPrgTemporalAccumulation       = FullScreenPass::create(mpDevice, kTemporalAccumulationShader, newDefines);
    mpPrgEstimateVariance           = FullScreenPass::create(mpDevice, kEstimateVarianceShader, newDefines);
    mpPrgAtrousFullScreen           = FullScreenPass::create(mpDevice, kAtrousShader, newDefines);
    
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

    isDirty |= widget.var("# Diff Iterations", mDiffAtrousIterations, 0, 16, 1);
    isDirty |= widget.var("Gradient Filter Radius", mGradientFilterRadius, 0, 16, 1);

    isDirty |= widget.var("Weight Phi", weightPhiColor, 0.0f, 100.0f, 1.0f);
    isDirty |= widget.var("Weight Normal", weightPhiNormal, 0.0f, 128.0f, 1.0f);

    isDirty |= widget.dropdown( "Denoising Algorithm", DENOISING_ALGORITHM_LIST, *(reinterpret_cast<uint32_t*>(&mCurrentDenoisingAlgorithm)));

    mUseMutualInformation = mCurrentDenoisingAlgorithm != DenoisingAlgorithm::ASVGF;
    if (mUseMutualInformation)
    {
        if (mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_ONLY_TEMPORAL ||
            mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_TEMPORAL_AND_SPATIAL)
        {
            isDirty |=
                widget.dropdown("Linearly Increasing Temporal Value", LINEARLY_INCREASING_TEMPORAL_VALUE_LIST, *(reinterpret_cast<uint32_t*>(&mCurrentLinearlyIncrTemporalVal)));
            isDirty |= widget.var("Num Frames for MI Calc", mNumFramesInMICalc, 1, 200, 1);
            isDirty |= widget.var("Frame Lum Bin count in Temp MI", mFrameLumBinCountInTempMI, 2, 200, 1);
            
        }
        if (mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_ONLY_SPATIAL ||
            mCurrentDenoisingAlgorithm == DenoisingAlgorithm::MI_TEMPORAL_AND_SPATIAL)
        {
            isDirty |= widget.var("Spatial Lum Bin Count", mSpatialLumBinCount, 3, 10, 1);
            isDirty |= widget.var("Grad Diff Threshold Ratio", mGradDiffRatioThreshold, 0.01f, 1.0f, 0.01f);
            isDirty |= widget.var("Spatial MI Threshold", mSpatialMIThreshold, 0.0f, 10.0f, 0.01f);
            isDirty |= widget.var("Spatial radius", mSpatialMutualInfRadius, 1, 4, 1);
            isDirty |= widget.var("Min History Count Spatial Threshold", mMinHistoryCountSpatialThreshold, 1, 100, 1);
        }

        isDirty |= widget.dropdown("Information Calc Type", INFORMATION_CALC_TYPE_LIST, *(reinterpret_cast<uint32_t*>(&mInfCalcType)));
    }

    #if IS_DEBUG_PASS
    auto groupDebug = widget.group("DEBUG");
    {
        auto groupDebugLightTest = groupDebug.group("Light Test");
        isDirty |= groupDebugLightTest.var("Debug: Min Light Value", mMinLightValue, 0.0f, 500.0f, 1.0f);
        isDirty |= groupDebugLightTest.var("Debug: Max Light Value", mMaxLightValue, 0.0f, 500.0f, 1.0f);
        isDirty |= groupDebugLightTest.var("Debug: Light Test Frames", mNumFramesLightTest, 1, 500, 1);
        isDirty |= groupDebugLightTest.checkbox("Debug: Test Emissive Mat(T) or Light(F)", testMat);
        isDirty |= groupDebugLightTest.textbox("Debug: Emissive Mat Name", mEmissiveMatName);
        isDirty |= groupDebugLightTest.var("Debug: Point Light ID", mLightId, 0, 500, 1);

        if (!isConductLightFlickerTest)
        {
            isConductLightFlickerTest = groupDebugLightTest.button("Debug: Conduct Flickering Light Test");
            mCurrentLightFlickerFrameIndex = 0;
        }

        groupDebug.checkbox("Debug: LOG MI Calc", mDebugLogMICalc);
    }
    #endif

    if (isDirty)
    {
        IsClearBuffers = true;
    }
}

void ASVGFPass::takeOutputScreenshot(ref<Texture> pScreenShotTexture)
{
    //ref<Texture> pScreenShotTexture = mpAtrousFullScreenResultPingPong[0]->getColorTexture(0);

    auto ext = Bitmap::getFileExtFromResourceFormat(pScreenShotTexture->getFormat());
    auto fileformat = Bitmap::getFormatFromFileExtension(ext);

    time_t now = time(0);
    struct tm tstruct;
    char buf[80];
    tstruct = *localtime(&now);
    strftime(buf, sizeof(buf), "%Y%m%d_%X", &tstruct);

    std::string savePath = "FrameCapture\\" + std::string(buf) + "." + ext;
    savePath.erase(std::remove(savePath.begin(), savePath.end(), ':'), savePath.end());

    pScreenShotTexture->captureToFile(
        0, 0, std::filesystem::path(savePath), fileformat, Falcor::Bitmap::ExportFlags::None, true
    );
}

bool ASVGFPass::onMouseEvent(const MouseEvent& a_mouseEvent)
{
    #if IS_DEBUG_PASS
    if (a_mouseEvent.type == MouseEvent::Type::ButtonDown && a_mouseEvent.button == Input::MouseButton::Left)
    {
        float2 screenDims(screenWidth, screenHeight);
        mDebugSelectedPixel = clamp((int2)(a_mouseEvent.pos * screenDims), {0, 0}, (int2)screenDims - 1);

        logInfo("Debug Mouse Selected Pixel : {},{}\n", mDebugSelectedPixel.x, mDebugSelectedPixel.y);
    }
    #endif
    return false;
}

#if IS_DEBUG_PASS
void ASVGFPass::debugPass(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pInputColorTexture = renderData.getTexture(kInputColorTexture);
    ref<Texture> pInputAlbedoTexture = renderData.getTexture(kInputAlbedoTexture);
    ref<Texture> pInputLinearZTexture = renderData.getTexture(kInputLinearZTexture);
    ref<Texture> pInputGradientSamples = renderData.getTexture(kInputGradientSamplesTexture);
    ref<Texture> pInputVisibilityBuffer = renderData.getTexture(kInputCurrVisibilityBufferTexture);
    ref<Texture> pInputMotionVectors = renderData.getTexture(kInputMotionVectors);
    ref<Texture> pInputNormalVectors = renderData.getTexture(kInputNormalsTexture);

    ref<Texture> pInternalPrevColorTexture = renderData.getTexture(kInternalPrevColorTexture);
    ref<Texture> pInternalPrevAlbedoTexture = renderData.getTexture(kInternalPrevAlbedoTexture);
    ref<Texture> pInternalPrevLinearZTexture = renderData.getTexture(kInternalPrevLinearZTexture);
    ref<Texture> pInternalPrevNormalsTexture = renderData.getTexture(kInternalPrevNormalsTexture);
    ref<Texture> pInternalPrevVisBufferTexture = renderData.getTexture(kInternalPrevVisibilityBuffer);

    float gradResWidth = gradient_res(screenWidth);
    float gradResHeight = gradient_res(screenHeight);

    GraphicsState::Viewport vp1(0, 0, gradResWidth, gradResHeight, 0, 1);
    auto gradforwardGraphicState = mpPrgDebugFullScreen->getState();
    gradforwardGraphicState->setViewport(0, vp1);

    auto perImageDebugFullScreenCB = mpPrgDebugFullScreen->getRootVar()["PerImageCB"];
    perImageDebugFullScreenCB["gColor"] = mpTestColorTexture;//mpGradientResultPingPongBuffer[0]->getColorTexture(2);
    perImageDebugFullScreenCB["gAlbedo"] = pInputAlbedoTexture;
    perImageDebugFullScreenCB["gGradientSample"] = pInputGradientSamples;
    perImageDebugFullScreenCB["gGradLuminanceDiff"] = mpGradientResultPingPongBuffer[0]->getColorTexture(0);

    perImageDebugFullScreenCB["gLinearZTexture"] = pInputLinearZTexture;
    perImageDebugFullScreenCB["gNormalsTexture"] = pInputNormalVectors;
    perImageDebugFullScreenCB["gVisibilityBuffer"] = pInputVisibilityBuffer;
    perImageDebugFullScreenCB["gPrevVisibilityBuffer"] = pInternalPrevVisBufferTexture;
    mpPrgDebugFullScreen->execute(pRenderContext, mpDebugBuffer);
}
#endif IS_DEBUG_PASS
