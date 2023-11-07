add_plugin(MultipleIlluminationModulator)

target_sources(MultipleIlluminationModulator PRIVATE
    MultipleIlluminationModulator.cpp
    MultipleIlluminationModulator.h
	MultipleIlluminationModulator.ps.slang
)

target_source_group(MultipleIlluminationModulator "RenderPasses")
