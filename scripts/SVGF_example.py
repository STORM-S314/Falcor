from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_g():
    g = RenderGraph('g')
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('GBufferRT', 'GBufferRT', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back', 'texLOD': 'Mip0', 'useTraceRayInline': False, 'useDOF': True})
    g.create_pass('SVGFPass', 'SVGFPass', {'Enabled': True, 'Iterations': 4, 'FeedbackTap': 1, 'VarianceEpsilon': 9.999999747378752e-05, 'PhiColor': 10.0, 'PhiNormal': 128.0, 'Alpha': 0.05000000074505806, 'MomentsAlpha': 0.20000000298023224})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.add_edge('PathTracer.albedo', 'SVGFPass.Albedo')
    g.add_edge('GBufferRT.posW', 'SVGFPass.WorldPosition')
    g.add_edge('PathTracer.color', 'SVGFPass.Color')
    g.add_edge('GBufferRT.faceNormalW', 'SVGFPass.PositionNormalFwidth')
    g.add_edge('GBufferRT.normW', 'SVGFPass.WorldNormal')
    g.add_edge('GBufferRT.linearZ', 'SVGFPass.LinearZ')
    g.add_edge('GBufferRT.mvec', 'SVGFPass.MotionVec')
    g.add_edge('GBufferRT.emissive', 'SVGFPass.Emission')
    g.add_edge('SVGFPass.Filtered image', 'ToneMapper.src')
    g.add_edge('ToneMapper.dst', 'TAA.colorIn')
    g.add_edge('GBufferRT.mvec', 'TAA.motionVecs')
    g.add_edge('GBufferRT.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRT.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRT.viewW', 'PathTracer.viewW')
    g.mark_output('TAA.colorOut')
    return g

g = render_graph_g()
try: m.addGraph(g)
except NameError: None
