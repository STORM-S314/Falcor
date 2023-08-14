add_plugin(ASVGFPass)

target_sources(ASVGFPass PRIVATE
	ASVGFPass.h
	ASVGFPass.cpp
	ASVGFShared.slang
	ColorSpace.slang
	CreateGradientSamples.ps.slang
	AtrousGradient.ps.slang
	TemporalAccumulation.ps.slang
)

target_source_group(ASVGFPass "RenderPasses")
