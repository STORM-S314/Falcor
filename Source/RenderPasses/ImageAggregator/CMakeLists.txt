add_plugin(ImageAggregator)

target_sources(ImageAggregator PRIVATE
    ImageAggregator.cpp
    ImageAggregator.h
	ImageAggregator.ps.slang
)

target_source_group(ImageAggregator "RenderPasses")
