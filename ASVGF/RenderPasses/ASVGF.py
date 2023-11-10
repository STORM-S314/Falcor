from pathlib import WindowsPath, PosixPath
from falcor import *

def render_graph_DefaultRenderGraph():
    g = RenderGraph('DefaultRenderGraph')
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('ASVGFPass', 'ASVGFPass', {'NumIterations': 5, 'HistoryTap': 0, 'FilterKernel': 1, 'TemporalColorAlpha': 0.0, 'TemporalMomentsAlpha': 0.10000000149011612, 'DiffAtrousIterations': 5, 'GradientFilterRadius': 2, 'FramesPerMICalc': 20, 'UseMutualInfCalc': False, 'SpatialMutInfRadius': 1, 'NumFramesInMICalc': 20, 'GradDiffRatioThreshold': 0.15000000596046448, 'SpatialMIThreshold': 0.05000000074505806})
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('GradForwardProjPass', 'GradForwardProjPass', {})
    g.create_pass('PathTracerMod', 'PathTracerMod', {'samplesPerPixel': 1, 'maxSurfaceBounces': 1, 'maxDiffuseBounces': 1, 'maxSpecularBounces': 1, 'maxTransmissionBounces': 0, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.add_edge('GBufferRaster.linearZ', 'GradForwardProjPass.InLinearZ')
    g.add_edge('TAA.colorOut', 'ToneMapper.src')
    g.add_edge('ASVGFPass.Filtered image', 'TAA.colorIn')
    g.add_edge('PathTracerMod.albedo', 'ASVGFPass.Albedo')
    g.add_edge('PathTracerMod.color', 'ASVGFPass.Color')
    g.add_edge('GBufferRaster.mvec', 'TAA.motionVecs')
    g.add_edge('GradForwardProjPass.OutGradVisibilityBuffer', 'PathTracerMod.vbuffer')
    g.add_edge('GBufferRaster.mvec', 'PathTracerMod.mvec')
    g.add_edge('GradForwardProjPass.OutGradWViewBuffer', 'PathTracerMod.viewW')
    g.add_edge('GradForwardProjPass.OutRandomNumberBuffer', 'PathTracerMod.randomNumbers')
    g.add_edge('GradForwardProjPass.OutGradSamplesBuffer', 'ASVGFPass.GradientSamples')
    g.add_edge('GBufferRaster.normW', 'GradForwardProjPass.InWorldNormal')
    g.add_edge('GBufferRaster.vbuffer', 'GradForwardProjPass.InVisibilityBuffer')
    g.add_edge('GBufferRaster.posW', 'GradForwardProjPass.InWPos')
    g.add_edge('GBufferRaster.viewW', 'GradForwardProjPass.InWViewBuffer')
    g.add_edge('GBufferRaster.linearZ', 'ASVGFPass.LinearZ')
    g.add_edge('GBufferRaster.normW', 'ASVGFPass.Normals')
    g.add_edge('GradForwardProjPass.OutGradVisibilityBuffer', 'ASVGFPass.GradientVisibilityBuffer')
    g.add_edge('GBufferRaster.vbuffer', 'ASVGFPass.CurrentVisibilityBuffer')
    g.add_edge('GBufferRaster.mvec', 'ASVGFPass.MotionVectors')
    g.add_edge('PathTracerMod.specularAlbedo', 'ASVGFPass.SpecularAlbedo')
    g.add_edge('GBufferRaster.emissive', 'ASVGFPass.Emission')
    g.mark_output('ToneMapper.dst')
    return g

DefaultRenderGraph = render_graph_DefaultRenderGraph()
try: m.addGraph(DefaultRenderGraph)
except NameError: None
