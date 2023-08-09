add_plugin(GradForwardProjPass)

target_sources(GradForwardProjPass PRIVATE
    GradForwardProjPass.cpp
    GradForwardProjPass.h
	RandomNumGenerator.ps.slang
	GradientForwardProjection.ps.slang
	PackLinearZAndNormal.ps.slang
)

target_source_group(GradForwardProjPass "RenderPasses")
