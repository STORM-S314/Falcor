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

    mpPrgTemporalAccumulation   = FullScreenPass::create(mpDevice, kTemporalAccumulationShader);
    mpPrgEstimateVariance       = FullScreenPass::create(mpDevice, kEstimateVarianceShader);
    mpPrgAtrous                 = FullScreenPass::create(mpDevice, kAtrousShader);
    mpPrgCreateGradientSamples  = FullScreenPass::create(mpDevice, kCreateGradientSamplesShader);
    mpPrgAtrousGradient         = FullScreenPass::create(mpDevice, kAtrousGradientShader);

    ///////////////////////////////////////////
	m_pProgram = GraphicsProgram::createFromFile(pDevice, "RenderPasses/ASVGFPass/ASVGF_test.3d.slang", "vsMain", "psMain");
    RasterizerState::Desc l_ASVGFFrameDesc;
    l_ASVGFFrameDesc.setFillMode(RasterizerState::FillMode::Wireframe);
    l_ASVGFFrameDesc.setCullMode(RasterizerState::CullMode::None);
    m_pRasterState = RasterizerState::create(l_ASVGFFrameDesc);

    m_pGraphicsState = GraphicsState::create(pDevice);
    m_pGraphicsState->setProgram(m_pProgram);
    m_pGraphicsState->setRasterizerState(m_pRasterState);
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

RenderPassReflection ASVGFPass::reflect(const CompileData& compileData)
{
    RenderPassReflection reflector;
    reflector.addOutput("output", "ASVGF test view");
    return reflector;
}

void ASVGFPass::execute(RenderContext* a_pRenderContext, const RenderData& a_renderData)
{
    auto pTargetFbo = Fbo::create(mpDevice, {a_renderData.getTexture("output")});
    const float4 clearColor(0, 0, 0, 1);
    a_pRenderContext->clearFbo(pTargetFbo.get(), clearColor, 1.0f, 0, FboAttachmentType::All);
    m_pGraphicsState->setFbo(pTargetFbo);

    if (m_pScene)
    {
        auto l_RootVar = m_pVars->getRootVar();
        l_RootVar["PerFrameCB"]["gColor"] = float4(0, 0, 1, 1);

        m_pScene->rasterize(a_pRenderContext, m_pGraphicsState.get(), m_pVars.get(), m_pRasterState, m_pRasterState);
    }
}


void ASVGFPass::setScene(RenderContext* a_pRenderContext, const ref<Scene>& a_pScene)
{
    m_pScene = a_pScene;
    if (m_pScene)
        m_pProgram->addDefines(m_pScene->getSceneDefines());
    m_pVars = GraphicsVars::create(mpDevice, m_pProgram->getReflector());
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
    
    //mpGui->addCheckBox("Enable", &mEnable, mGuiGroupName);
    //mpGui->addCheckBox("Modulate Albedo", &mModulateAlbedo, mGuiGroupName);
    //mpGui->addFloatVar("Temporal Alpha", &mTemporalAlpha, mGuiGroupName);
    //mpGui->addIntVar("# Iterations", &mNumIterations, mGuiGroupName, 0, 16, 1);
    //mpGui->addIntVar("History Tap", &mHistoryTap, mGuiGroupName, -1, 16, 1);
    //mpGui->addDropdown("Kernel", filterKernels, &mFilterKernel, mGuiGroupName);
    //mpGui->addCheckBox("Show Antilag Alpha", &mShowAntilagAlpha, mGuiGroupName);
    //mpGui->addIntVar("# Diff Iterations", &mDiffAtrousIterations, mGuiGroupName, 0, 16, 1);
    //mpGui->addIntVar("Gradient Filter Radius", &mGradientFilterRadius, mGuiGroupName, 0, 16, 1);
}
