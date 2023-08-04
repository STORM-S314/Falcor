add_plugin(PathTracerMod)

target_sources(PathTracerMod PRIVATE
    ColorType.slang
    GeneratePaths.cs.slang
    GuideData.slang
    LoadShadingData.slang
    NRDHelpers.slang
    Params.slang
    PathState.slang
    PathTracer.slang
    PathTracerMod.cpp
    PathTracerMod.h
    PathTracerNRD.slang
    ReflectTypes.cs.slang
    ResolvePass.cs.slang
    StaticParams.slang
    TracePass.rt.slang
)

target_copy_shaders(PathTracerMod RenderPasses/PathTracerMod)

target_source_group(PathTracerMod "RenderPasses")
