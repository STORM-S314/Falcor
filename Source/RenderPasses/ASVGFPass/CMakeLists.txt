add_plugin(ASVGFPass)

target_sources(ASVGFPass PRIVATE
    ASVGFPass.cpp
    ASVGFPass.h
)

target_source_group(ASVGFPass "RenderPasses")
