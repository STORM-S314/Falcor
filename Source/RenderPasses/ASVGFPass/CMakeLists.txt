add_plugin(ASVGFPass)

target_sources(ASVGFPass PRIVATE
	ASVGFShared.slang
	ColorSpace.slang
	CreateGradientSamples.ps.slang
    ASVGFPass.cpp
    ASVGFPass.h
)

target_source_group(ASVGFPass "RenderPasses")
