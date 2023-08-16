add_plugin(ASVGFPass)

target_sources(ASVGFPass PRIVATE
	ASVGFPass.h
	ASVGFPass.cpp
	ASVGFCommon.slang
	ColorSpace.slang
	CreateGradientSamples.ps.slang
	AtrousGradient.ps.slang
	TemporalAccumulation.ps.slang
	VarianceEstimation.ps.slang
	AtrousFullScreen.ps.slang
	DebugPass.ps.slang
)

target_source_group(ASVGFPass "RenderPasses")
