add_plugin(ImageAggregator)

target_sources(ImageAggregator PRIVATE
    ImageAggregator.cpp
    ImageAggregator.h
	ImageAggregator.ps.slang
)
target_copy_shaders(ImageAggregator RenderPasses/ImageAggregator)

target_source_group(ImageAggregator "RenderPasses")
