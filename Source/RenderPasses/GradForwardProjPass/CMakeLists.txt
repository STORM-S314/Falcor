add_plugin(GradForwardProjPass)

target_sources(GradForwardProjPass PRIVATE
    GradForwardProjPass.cpp
    GradForwardProjPass.h
	RandomNumGenerator.ps.slang
	PackLinearZAndNormal.ps.slang
	GradForwardProjCommon.slang
	GradientForwardProjection.cs.slang
)

target_source_group(GradForwardProjPass "RenderPasses")
