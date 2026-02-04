add_plugin(MultipleIlluminationModulator)

target_sources(MultipleIlluminationModulator PRIVATE
    MultipleIlluminationModulator.cpp
    MultipleIlluminationModulator.h
    MultipleIlluminationModulator.ps.slang
)

target_copy_shaders(MultipleIlluminationModulator RenderPasses/MultipleIlluminationModulator)

target_source_group(MultipleIlluminationModulator "RenderPasses")
