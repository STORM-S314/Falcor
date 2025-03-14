add_plugin(ASVGFPass)

target_sources(ASVGFPass PRIVATE
	ASVGFPass.h
	ASVGFPass.cpp
	ASVGFCommon.slang
	ASVGFUtils.slang
	ColorSpace.slang
	CreateGradientSamples.ps.slang
	AtrousGradient.ps.slang
	TemporalAccumulation.ps.slang
	VarianceEstimation.ps.slang
	AtrousFullScreen.ps.slang
	TemporalMutualInformationCalculation.ps.slang
	SpatialMutualInformationCalculation.ps.slang
	CSVGFTemporalAccumulation.ps.slang
	CSVGFTemporalLUT.bin
    BestCSVGFTemporalLUT.bin
	DebugPass.ps.slang
)
target_copy_shaders(ASVGFPass RenderPasses/ASVGFPass)

target_source_group(ASVGFPass "RenderPasses")
