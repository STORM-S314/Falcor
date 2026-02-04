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
#include "ImageAggregator.h"
#include <time.h>
#include <algorithm>

extern "C" FALCOR_API_EXPORT void registerPlugin(Falcor::PluginRegistry& registry)
{
    registry.registerClass<RenderPass, ImageAggregator>();
}

// Input buffer names
const char kInputColorTexture[]     = "Color";
const char kOutputAggregatedImage[] = "AggregatedImage";

const char kImageAggregationCount[] = "ImageAggregationCount";

const char kIsAggregating[] = "IsAggregating";

// Shader source files
const char kImageAggregator[] = "RenderPasses/ImageAggregator/ImageAggregator.ps.slang";

ImageAggregator::ImageAggregator(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
    for (const auto& [key, value] : props)
    {
        if (key == kImageAggregationCount)
            mImageAggregationCount = value;
        else if (key == kIsAggregating)
            mStartAggregation = value;
        else
            logWarning("Unknown property '{}' in ASVGFPass properties.", key);
    }
}

Properties ImageAggregator::getProperties() const
{
    Properties props;
    props[kImageAggregationCount] = mImageAggregationCount;
    props[kIsAggregating] = mStartAggregation;
    return props;
}

void ImageAggregator::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    if(!mpPrgImageAggregation){
        int screenWidth = compileData.defaultTexDims.x;
        int screenHeight = compileData.defaultTexDims.y;
        // aggregated image result
        Fbo::Desc formatAggregatedImageResult;
        formatAggregatedImageResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);
        mpImageAggregatorFullScreen = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatAggregatedImageResult);
        mpPrgImageAggregation = FullScreenPass::create(mpDevice, kImageAggregator);
    }
}

RenderPassReflection ImageAggregator::reflect(const CompileData& compileData)
{
    // Define the required resources here
    RenderPassReflection reflector;

    //Input
    reflector.addInput(kInputColorTexture, "Color");

    // Output
    reflector.addOutput(kOutputAggregatedImage, "AggregatedImage").format(ResourceFormat::RGBA32Float);
    
    return reflector;
}

void ImageAggregator::execute(RenderContext* pRenderContext, const RenderData& renderData)
{

    if (mIsDirty)
    {
        pRenderContext->clearFbo(mpImageAggregatorFullScreen.get(), float4(0), 1.0f, 0, FboAttachmentType::All);
        mIsDirty = false;
    }

    ref<Texture> pInputColorTexture = renderData.getTexture(kInputColorTexture);

    ref<Texture> pOutputAggregatedTexture = renderData.getTexture(kOutputAggregatedImage);

    // Image Aggregation
    if (mStartAggregation)
    {
        mCurrentImagesAccumulated++;
        auto perImageImgAggregationCB = mpPrgImageAggregation->getRootVar()["PerImageCB"];
        perImageImgAggregationCB["gColor"] = pInputColorTexture;
        perImageImgAggregationCB["gPrevAccumulationColor"] = mpImageAggregatorFullScreen->getColorTexture(0);                
        perImageImgAggregationCB["gInputImageIndex"] = mCurrentImagesAccumulated;
        mpPrgImageAggregation->execute(pRenderContext, mpImageAggregatorFullScreen);    
        if (mCurrentImagesAccumulated >= (mImageAggregationCount))
        {
            logWarning(
                "Percentage Completed: {}",
                ((((float)mCurrentImagesAccumulated) /
                  mImageAggregationCount) *
                 100.0f)
            );
            mStartAggregation = false;            
            mCurrentImagesAccumulated = 0;
        }
    }    
    pRenderContext->blit(mpImageAggregatorFullScreen->getColorTexture(0)->getSRV(), pOutputAggregatedTexture->getRTV());
}

void ImageAggregator::renderUI(Gui::Widgets& widget)
{
    if (!mStartAggregation)
    {
        mStartAggregation = widget.button("Create Aggregation");
        mIsDirty = widget.var("Image aggregation count x * x. Specify x:", mImageAggregationCount, 10, 300, 10);
    }
}
