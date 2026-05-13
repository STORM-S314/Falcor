add_plugin(ModPathTracer)

target_sources(ModPathTracer PRIVATE
    ColorType.slang
    GeneratePaths.cs.slang
    GuideData.slang
    LoadShadingData.slang
    NRDHelpers.slang
    Params.slang
    PathState.slang
    PathTracer.slang
    PathTracer.cpp
    PathTracer.h
    PathTracerNRD.slang
    ReflectTypes.cs.slang
    ResolvePass.cs.slang
    StaticParams.slang
    TracePass.rt.slang
)

target_copy_shaders(ModPathTracer RenderPasses/ModPathTracer)

target_source_group(ModPathTracer "RenderPasses")
