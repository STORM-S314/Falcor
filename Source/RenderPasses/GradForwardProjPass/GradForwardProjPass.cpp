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

// Input buffer names
const char kInputCurrentMVec[] = "MotionVectors";
const char kInputVisibilityBuffer[] = "VisibilityBuffer";
const char kInputDepthBuffer[] = "DepthBuffer";
const char kInputNormalBuffer[] = "NormalBuffer";

//Output buffer names
const char kOutputCurrentGradientSamples[] = "GradientSamples";
const char kOutputVisibilityBuffer[] = "VisibilityBuffer";
const char kOutputRandomNumberBuffer[] = "RandomNumberBuffer";

//shader locations
const char kRandomNumberGeneratorShader[] = "RenderPasses/GradForwardProjPass/RandomNumGenerator.ps.slang";

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, GradForwardProjPass>();
}

GradForwardProjPass::GradForwardProjPass(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    mpPrgRandomNumberGenerator = FullScreenPass::create(mpDevice, kRandomNumberGeneratorShader);
}

Properties GradForwardProjPass::getProperties() const
{
    return {};
}

void GradForwardProjPass::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int w = compileData.defaultTexDims.x;
    int h = compileData.defaultTexDims.y;
    
    Fbo::Desc formatDesc;
    formatDesc.setSampleCount(0);
    formatDesc.setColorTarget(0, Falcor::ResourceFormat::RGBA32Uint);

    mpRandomNumberFBO = Fbo::create2D(mpDevice, w, h, formatDesc);
}

RenderPassReflection GradForwardProjPass::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;
    //reflector.addOutput("dst");
    //reflector.addInput("src");

    //Input
    reflector.addInput(kInputCurrentMVec, "MotionVectors");

    //Output
    reflector.addOutput(kOutputRandomNumberBuffer, "RandomNumberBuffer");

    return reflector;
}

void GradForwardProjPass::execute(RenderContext* pRenderContext, const RenderData& renderData)
{
    ref<Texture> pRandomNumberTexture = renderData.getTexture(kOutputRandomNumberBuffer);

    // renderData holds the requested resources
    // auto& pTexture = renderData.getTexture("src");

    auto perImageCB = mpPrgRandomNumberGenerator->getRootVar()["PerImageCB"];
    perImageCB["seed"] = ++i;
    mpPrgRandomNumberGenerator->execute(pRenderContext, mpRandomNumberFBO);

    pRenderContext->blit(mpRandomNumberFBO->getColorTexture(0)->getSRV(), pRandomNumberTexture->getRTV());
}

void GradForwardProjPass::renderUI(Gui::Widgets& widget)
{
}
