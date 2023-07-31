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

using namespace Falcor;

class ASVGFPass : public RenderPass
{
public:
    FALCOR_PLUGIN_CLASS(ASVGFPass, "ASVGFPass", "Adaptive-SVGF.");

    static ref<ASVGFPass> create(ref<Device> a_pDevice, const Properties& a_props) { return make_ref<ASVGFPass>(a_pDevice, a_props); }

    ASVGFPass(ref<Device> a_pDevice, const Properties& a_props);

    virtual Properties getProperties() const override;
    virtual RenderPassReflection reflect(const CompileData& a_compileData) override;
    virtual void compile(RenderContext* a_pRenderContext, const CompileData& a_compileData) override {}
    virtual void execute(RenderContext* a_pRenderContext, const RenderData& a_renderData) override;
    virtual void renderUI(Gui::Widgets& a_widget) override;
    virtual void setScene(RenderContext* a_pRenderContext, const ref<Scene>& a_pScene);
    virtual bool onMouseEvent(const MouseEvent& a_mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& a_keyEvent) override { return false; }

private:
    ref<Scene> m_pScene;
    ref<GraphicsProgram> m_pProgram;
    ref<GraphicsState> m_pGraphicsState;
    ref<RasterizerState> m_pRasterState;
    ref<GraphicsVars> m_pVars;

    //Params
    bool mEnable = true;
    bool mModulateAlbedo = true;
    int mNumIterations = 5;
    int mHistoryTap = 0;
    uint32_t mFilterKernel = 1;
    float mTemporalAlpha = 0.1f;
    int mDiffAtrousIterations = 5;
    int mGradientFilterRadius = 2;
    bool mNormalizeGradient = true;
    bool mShowAntilagAlpha = false;

    //Full screen passes
    ref<FullScreenPass> mpPrgTemporalAccumulation;
    ref<FullScreenPass> mpPrgEstimateVariance;
    ref<FullScreenPass> mpPrgAtrous;
    ref<FullScreenPass> mpPrgCreateGradientSamples;
    ref<FullScreenPass> mpPrgAtrousGradient;

    //FBOs
    ref<Fbo> mpFBOAccum, mpFBOAccumPrev, mpFBOPing, mpFBOPong, mpColorHistory;
    ref<Fbo> mpColorHistoryUnfiltered, mpAntilagAlpha;
    ref<Fbo> mpDiffPing, mpDiffPong;
};
