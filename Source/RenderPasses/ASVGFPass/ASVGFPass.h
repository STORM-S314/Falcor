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
#pragma once
#include "Falcor.h"
#include "RenderGraph/RenderPass.h"
#include "Core/Pass/FullScreenPass.h"
#include "Utils/Timing/FrameRate.h"
#include <fstream>
#define IS_DEBUG_PASS 1

using namespace Falcor;

class ASVGFPass : public RenderPass
{
public:
    FALCOR_PLUGIN_CLASS(ASVGFPass, "ASVGFPass", "Adaptive-SVGF.");

    static ref<ASVGFPass> create(ref<Device> a_pDevice, const Properties& a_props) { return make_ref<ASVGFPass>(a_pDevice, a_props); }

    ASVGFPass(ref<Device> a_pDevice, const Properties& a_props);

    virtual Properties getProperties() const override;
    virtual RenderPassReflection reflect(const CompileData& a_compileData) override;
    virtual void compile(RenderContext* a_pRenderContext, const CompileData& a_compileData) override;
    virtual void execute(RenderContext* a_pRenderContext, const RenderData& a_renderData) override;
    virtual void renderUI(Gui::Widgets& a_widget) override;
    virtual void setScene(RenderContext* a_pRenderContext, const ref<Scene>& a_pScene);
    virtual bool onMouseEvent(const MouseEvent& a_mouseEvent) override;
    virtual bool onKeyEvent(const KeyboardEvent& a_keyEvent) override { return false; }
    int gradient_res(int x);
    void allocateBuffers(RenderContext* a_pRenderContext);
    void resetBuffers(RenderContext* pRenderContext, const RenderData& renderData);
    void takeOutputScreenshot(ref<Texture> pScreenShotTexture);

#if IS_DEBUG_PASS
    void debugPass(RenderContext* pRenderContext, const RenderData&);
#endif IS_DEBUG_PASS

private:
    enum class DenoisingAlgorithm
    {
        ASVGF = 0,
        MI_ONLY_TEMPORAL = 1,
        MI_ONLY_SPATIAL = 2,
        MI_TEMPORAL_AND_SPATIAL = 3,
        CSVGF_ONLY_TEMPORAL = 4,
    };

    enum class InformationCalcType
    {
        MI = 0,
        ENTROPY = 1
    };

    enum class LinearlyIncreasingTemporalValue
    {
        TIME_STEP = 0,
        FRAME_NUMBER = 1,
    };

    ref<Scene> pScene;
    bool IsClearBuffers = false;

    // Params
    int mNumIterations = 5;
    int mHistoryTap = 0;
    uint32_t mFilterKernel = 1;
    float mTemporalColorAlpha = 0.1f;
    float mTemporalMomentsAlpha = 0.6f;
    int mDiffAtrousIterations = 5;
    int mGradientFilterRadius = 2;
    int gradientDownsample = 3;
    float weightPhiColor = 3.0f;
    float weightPhiNormal = 128.0f;
    bool mUseMutualInformation = false;
    bool mUseCSVGF = false;
    int mSpatialMutualInfRadius = 1;
    int mNumFramesInMICalc = 20;
    int mFrameLumBinCountInTempMI = 5;
    float mGradDiffRatioThreshold = 0.05f;
    float mSpatialMIThreshold = 0.05f;
    int mMinHistoryCountSpatialThreshold = 1;
    int mSpatialLumBinCount = 5;
    int mLumDimSize = 8;
    int mDepthDimSize = 8;

    std::vector<float> mCSVGFTemporalLUT;

    int screenWidth = 0;
    int screenHeight = 0;

    DenoisingAlgorithm mCurrentDenoisingAlgorithm = DenoisingAlgorithm::CSVGF_ONLY_TEMPORAL;
    LinearlyIncreasingTemporalValue mCurrentLinearlyIncrTemporalVal = LinearlyIncreasingTemporalValue::TIME_STEP;
    InformationCalcType mInfCalcType = InformationCalcType::MI;

    const Falcor::Gui::DropdownList DENOISING_ALGORITHM_LIST = {
        {(uint32_t)DenoisingAlgorithm::ASVGF, "ASVGF"},
        {(uint32_t)DenoisingAlgorithm::MI_ONLY_TEMPORAL, "MI:Only Temporal"},
        {(uint32_t)DenoisingAlgorithm::MI_ONLY_SPATIAL, "MI:Only Spatial"},
        {(uint32_t)DenoisingAlgorithm::MI_TEMPORAL_AND_SPATIAL, "MI:Temporal and Spatial"},
        {(uint32_t)DenoisingAlgorithm::CSVGF_ONLY_TEMPORAL, "CSVGF:Only Temporal"}};

    const Falcor::Gui::DropdownList INFORMATION_CALC_TYPE_LIST = {
        {(uint32_t)InformationCalcType::MI, "MI"},
        {(uint32_t)InformationCalcType::ENTROPY, "Entropy"}
    };    

    const Falcor::Gui::DropdownList LINEARLY_INCREASING_TEMPORAL_VALUE_LIST = {
        {(uint32_t)LinearlyIncreasingTemporalValue::TIME_STEP, "Time Step"},
        {(uint32_t)LinearlyIncreasingTemporalValue::FRAME_NUMBER, "Frame Number"}
    };

    uint mCurrentFrameNumber = 0;
    std::deque<float> mdqTimeStep;

    Falcor::Clock mClock;

    //Params frame
    float2 mPrevFrameJitter{0.0f, 0.0f};

    //Full screen passes
    ref<FullScreenPass> mpPrgGradientForwardProjection;
    ref<FullScreenPass> mpPrgAtrousGradientCalculation;
    ref<FullScreenPass> mpPrgTemporalAccumulation;
    ref<FullScreenPass> mpPrgEstimateVariance;
    ref<FullScreenPass> mpPrgAtrousFullScreen;
    ref<FullScreenPass> mpPrgTemporalMutualInfCalc;
    ref<FullScreenPass> mpPrgSpatialMutualInfCalc;
    ref<FullScreenPass> mpPrgCSVGFTemporalAccumulation;

    //FBOs
    ref<Fbo> mpGradientResultPingPongBuffer[2];
    ref<Fbo> mpAtrousFullScreenResultPingPong[2];
    ref<Fbo> mpAccumulationBuffer;
    ref<Fbo> mpPrevAccumulationBuffer;
    ref<Fbo> mpTemporalMutualInfResultBuffer;
    ref<Fbo> mpBuffer;
    ref<Fbo> mpPrevBuffer;

    //Mutual Information
    ref<Buffer> mpMutualInformationCalcBuffer;
    ref<Buffer> mpPrevMutualInformationCalcBuffer;

    // Temporal filter coef lookup table
    ref<Buffer> mpCSVGFTemporalLUTBuffer;

    ref<Buffer> mpFrameTimeSteps;

    //Debug
    #if IS_DEBUG_PASS
    ref<FullScreenPass> mpPrgDebugFullScreen;
    ref<Fbo> mpDebugBuffer;
    ref<Texture> mpTestColorTexture;
    ref<Buffer> mpTemporalDebugMICalc;
    ref<Buffer> mpSpatialDebugMICalc;

    int2 mDebugSelectedPixel = int2(0,0);
    bool mDebugLogMICalc = false;

    int mNumFramesLightTest = 10;
    float mMinLightValue = 0.0f;
    float mMaxLightValue = 20.0f;
    std::vector<float> mLightValuesPerFrame;
    bool isConductLightFlickerTest = false;
    int mCurrentLightFlickerFrameIndex = 0;
    float mOldLightIntensityVal = 0.0;
    bool testMat = false;
    int mLightId = 0;
    std::string mEmissiveMatName;

    #endif IS_DEBUG_PASS
};
