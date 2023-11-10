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

// Shader source files
const char kImageAggregator[] = "RenderPasses/ImageAggregator/ImageAggregator.ps.slang";

ImageAggregator::ImageAggregator(ref<Device> pDevice, const Properties& props)
    : RenderPass(pDevice)
{
}

Properties ImageAggregator::getProperties() const
{
    return {};
}

void ImageAggregator::compile(RenderContext* pRenderContext, const CompileData& compileData)
{
    int screenWidth = compileData.defaultTexDims.x;
    int screenHeight = compileData.defaultTexDims.y;

    // aggregated image result
    Fbo::Desc formatAggregatedImageResult;
    formatAggregatedImageResult.setColorTarget(0, Falcor::ResourceFormat::RGBA32Float);

    mpImageAggregatorFullScreen = Fbo::create2D(mpDevice, screenWidth, screenHeight, formatAggregatedImageResult);
    mpImageAccumulationBuffer[0] = Buffer::create(mpDevice, screenWidth * screenHeight * AGGREGATION_IMAGE_COUNT * sizeof(float) * 3);
    mpImageAccumulationBuffer[1] = Buffer::create(mpDevice, screenWidth * screenHeight * AGGREGATION_IMAGE_COUNT * sizeof(float) * 3);

    DefineList newDefines;
    newDefines.add("AGGREGATION_IMAGE_COUNT", std::to_string(AGGREGATION_IMAGE_COUNT));
    mpPrgImageAggregation = FullScreenPass::create(mpDevice, kImageAggregator, newDefines);
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
    ref<Texture> pInputColorTexture = renderData.getTexture(kInputColorTexture);

    ref<Texture> pOutputAggregatedTexture = renderData.getTexture(kOutputAggregatedImage);

    // Image Aggregation
    if (mStartAggregation)
    {
        auto perImageImgAggregationCB = mpPrgImageAggregation->getRootVar()["PerImageCB"];
        perImageImgAggregationCB["gColor"] = pInputColorTexture;
        perImageImgAggregationCB["gAccumulationBuffer"] = mpImageAccumulationBuffer[0]->asBuffer();
        perImageImgAggregationCB["gInputImageIndex"] = mCurrentImagesAccumulated;
        perImageImgAggregationCB["gImageDimensions"] = float2(pInputColorTexture->getWidth(), pInputColorTexture->getHeight());
        perImageImgAggregationCB["gPixelsInFrame"] = pInputColorTexture->getWidth() * pInputColorTexture->getHeight();
        mpPrgImageAggregation->execute(pRenderContext, mpImageAggregatorFullScreen);
        mCurrentImagesAccumulated++;

        if (mCurrentImagesAccumulated >= (AGGREGATION_IMAGE_COUNT))
        {
            auto perImageImgAggregationCB1 = mpPrgImageAggregation->getRootVar()["PerImageCB"];
            perImageImgAggregationCB1["gColor"] = mpImageAggregatorFullScreen->getColorTexture(0);
            perImageImgAggregationCB1["gAccumulationBuffer"] = mpImageAccumulationBuffer[1]->asBuffer();
            perImageImgAggregationCB1["gInputImageIndex"] = mCurrentAggregateImagesAccumulated;
            perImageImgAggregationCB1["gImageDimensions"] = float2(pInputColorTexture->getWidth(), pInputColorTexture->getHeight());
            perImageImgAggregationCB1["gPixelsInFrame"] = pInputColorTexture->getWidth() * pInputColorTexture->getHeight();
            mpPrgImageAggregation->execute(pRenderContext, mpImageAggregatorFullScreen);
            mCurrentAggregateImagesAccumulated++;

            if (mCurrentAggregateImagesAccumulated >= (AGGREGATION_IMAGE_COUNT))
            { 
                mStartAggregation = false;
                mAggregationComplete = true;
                auto ext = Bitmap::getFileExtFromResourceFormat(mpImageAggregatorFullScreen->getColorTexture(0)->getFormat());
                auto fileformat = Bitmap::getFormatFromFileExtension(ext);

                time_t now = time(0);
                struct tm tstruct;
                char buf[80];
                tstruct = *localtime(&now);
                strftime(buf, sizeof(buf), "%Y%m%d_%X", &tstruct);
                //logWarning("DateTime: {}", buf);

                std::string savePath = "FrameCapture\\" + std::string(buf) + "." + ext;
                savePath.erase(std::remove(savePath.begin(), savePath.end(), ':'), savePath.end());

                mpImageAggregatorFullScreen->getColorTexture(0)->captureToFile(0, 0, std::filesystem::path(savePath), fileformat,
                    Falcor::Bitmap::ExportFlags::None, true
                );

                /*std::string savePathPNG = "FrameCapture\\" + std::string(buf) + "png" + "." + "png";
                savePathPNG.erase(std::remove(savePathPNG.begin(), savePathPNG.end(), ':'), savePathPNG.end());

                mpImageAggregatorFullScreen->getColorTexture(0)->captureToFile(
                    0, 0, std::filesystem::path(savePathPNG), Falcor::Bitmap::FileFormat::PngFile, Falcor::Bitmap::ExportFlags::None, true
                );*/
                mCurrentAggregateImagesAccumulated = 0;
            }
            mCurrentImagesAccumulated = 0;
        }
    }

    if (mAggregationComplete)
    {
        pRenderContext->blit(mpImageAggregatorFullScreen->getColorTexture(0)->getSRV(), pOutputAggregatedTexture->getRTV());
    }
    else
    {
        pRenderContext->blit(pInputColorTexture->getSRV(), pOutputAggregatedTexture->getRTV());
    }
}

void ImageAggregator::renderUI(Gui::Widgets& widget)
{
    if (!mStartAggregation)
    {
        mStartAggregation = widget.button("Create Aggregation");
    }
}
