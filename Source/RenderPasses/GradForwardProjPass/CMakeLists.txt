add_plugin(GradForwardProjPass)

target_sources(GradForwardProjPass PRIVATE
    GradForwardProjPass.cpp
    GradForwardProjPass.h
	RandomNumGenerator.ps.slang
	GradForwardProjCommon.slang
	GradientForwardProjection.cs.slang
)
target_copy_shaders(GradForwardProjPass RenderPasses/GradForwardProjPass)

target_source_group(GradForwardProjPass "RenderPasses")
