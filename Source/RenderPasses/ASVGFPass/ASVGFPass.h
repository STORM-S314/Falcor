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

#define IS_DEBUG_PASS 0

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
    virtual bool onMouseEvent(const MouseEvent& a_mouseEvent) override { return false; }
    virtual bool onKeyEvent(const KeyboardEvent& a_keyEvent) override { return false; }
    int gradient_res(int x);
    void allocateBuffers(RenderContext* a_pRenderContext, int a_ScreenWidth, int a_ScreenHeight);
    void clearBuffers(RenderContext* pRenderContext, const RenderData& renderData);

    #if IS_DEBUG_PASS
    void debugPass(RenderContext* pRenderContext, const RenderData& );
    #endif IS_DEBUG_PASS

private:
    ref<Scene> pScene;
    bool IsClearBuffers = false;

    //Params
    int mNumIterations          = 5;
    int mHistoryTap             = 0;
    uint32_t mFilterKernel      = 1;
    float mTemporalColorAlpha   = 0.1f;
    float mTemporalMomentsAlpha = 0.6f;
    int mDiffAtrousIterations   = 5;
    int mGradientFilterRadius   = 1;
    int gradientDownsample      = 3;
    float weightPhiColor        = 3.0f;
    float weightPhiNormal       = 128.0f;

    //Params frame
    float2 mPrevFrameJitter{0.0f, 0.0f};

    //Full screen passes
    ref<FullScreenPass> mpPrgGradientForwardProjection;
    ref<FullScreenPass> mpPrgAtrousGradientCalculation;
    ref<FullScreenPass> mpPrgTemporalAccumulation;
    ref<FullScreenPass> mpPrgEstimateVariance;
    ref<FullScreenPass> mpPrgAtrousFullScreen;

    //FBOs
    ref<Fbo> mpGradientResultPingPongBuffer[2];
    ref<Fbo> mpAtrousFullScreenResultPingPong[2];
    ref<Fbo> mpAccumulationBuffer;
    ref<Fbo> mpPrevAccumulationBuffer;

    //Debug
    #if IS_DEBUG_PASS
    ref<FullScreenPass> mpPrgDebugFullScreen;
    ref<Fbo> mpDebugBuffer;
    ref<Texture> mpTestColorTexture;
    #endif IS_DEBUG_PASS
};
