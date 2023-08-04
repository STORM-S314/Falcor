add_plugin(GradForwardProjPass)

target_sources(GradForwardProjPass PRIVATE
    GradForwardProjPass.cpp
    GradForwardProjPass.h
)

target_source_group(GradForwardProjPass "RenderPasses")
