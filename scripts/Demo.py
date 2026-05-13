from pathlib import WindowsPath, PosixPath
from falcor import *
import time
import os
import logging
import traceback

frame_output_path = r'D:/data/' if os.name == 'nt' else os.path.expanduser('~/data/')
denoised_frame_path = frame_output_path + '/Denoised'
noise_frame_path = frame_output_path + '/Noisy'
csvgf_path = denoised_frame_path + '/CSVGF'
asvgf_path = denoised_frame_path + '/ASVGF'
scenes_path = r'D:/data' if os.name == 'nt' else os.path.expanduser('~/data/scenes')
logging.basicConfig(filename=frame_output_path + '/error_log.txt', level=logging.ERROR, format='%(asctime)s %(message)s')
def render_graph_ASVGF(useCSVGF = False, isTemporalTrain = False, isSpatialTrain = False):
    g = RenderGraph('ASVGF')
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.create_pass('GBufferRT', 'GBufferRT', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('ASVGFPass', 'ASVGFPass', {'UseCSVGF': useCSVGF,'IsTemporalTrain': isTemporalTrain, 'IsSpatialTrain': isSpatialTrain})
    if not useCSVGF:
        g.create_pass('GradForwardProjPass', 'GradForwardProjPass', {'UseCSVGF': useCSVGF})
        g.create_pass('ModPathTracer', 'ModPathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'useSER' : False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    else:
        g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'useSER': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    
    
    g.mark_output('ToneMapper.dst')
    # g.mark_output('ASVGFPass.Debug Output image')

    g.add_edge('TAA.colorOut', 'ToneMapper.src')
    g.add_edge('ASVGFPass.Filtered image', 'TAA.colorIn')

    # g.add_edge('ASVGFPass.Filtered image', 'ToneMapper.src')
    if not useCSVGF:
        g.add_edge('ModPathTracer.albedo', 'ASVGFPass.Albedo')
        g.add_edge('ModPathTracer.color', 'ASVGFPass.Color')
        g.add_edge('ModPathTracer.specularAlbedo', 'ASVGFPass.SpecularAlbedo')
        g.add_edge('GradForwardProjPass.OutGradVisibilityBuffer', 'ModPathTracer.vbuffer')
        g.add_edge('GBufferRT.mvec', 'ModPathTracer.mvec')
        g.add_edge('GradForwardProjPass.OutGradWViewBuffer', 'ModPathTracer.viewW')
        g.add_edge('GradForwardProjPass.OutRandomNumberBuffer', 'ModPathTracer.randomNumbers')
        g.add_edge('GradForwardProjPass.OutGradSamplesBuffer', 'ASVGFPass.GradientSamples')
        g.add_edge('GBufferRT.normW', 'GradForwardProjPass.InWorldNormal')
        g.add_edge('GBufferRT.vbuffer', 'GradForwardProjPass.InVisibilityBuffer')
        g.add_edge('GBufferRT.posW', 'GradForwardProjPass.InWPos')
        g.add_edge('GBufferRT.viewW', 'GradForwardProjPass.InWViewBuffer')
        g.add_edge('GBufferRT.linearZ', 'GradForwardProjPass.InLinearZ')
        g.add_edge('GradForwardProjPass.OutGradVisibilityBuffer', 'ASVGFPass.GradientVisibilityBuffer')
        # g.mark_output('ModPathTracer.albedo')
        # g.mark_output('ModPathTracer.color')
        # g.mark_output('ModPathTracer.specularAlbedo')
        # g.mark_output('ModPathTracer.indirectAlbedo')
        # g.mark_output('ModPathTracer.guideNormal')
        # g.mark_output('ModPathTracer.reflectionPosW')
        # g.mark_output('ModPathTracer.rayCount')
        # g.mark_output('ModPathTracer.pathLength')
        # g.mark_output('GradForwardProjPass.OutGradSamplesBuffer')
        # g.mark_output('GradForwardProjPass.OutGradVisibilityBuffer')
    else:
        g.add_edge('PathTracer.albedo', 'ASVGFPass.Albedo')
        g.add_edge('PathTracer.color', 'ASVGFPass.Color')
        g.add_edge('PathTracer.specularAlbedo', 'ASVGFPass.SpecularAlbedo')
        g.add_edge('GBufferRT.vbuffer', 'PathTracer.vbuffer')
        g.add_edge('GBufferRT.mvec', 'PathTracer.mvec')
        g.add_edge('GBufferRT.viewW', 'PathTracer.viewW')
        g.add_edge('GBufferRT.vbuffer', 'ASVGFPass.GradientVisibilityBuffer')
        # g.mark_output('PathTracer.albedo')
        # g.mark_output('PathTracer.color')
        # g.mark_output('PathTracer.specularAlbedo')
        # g.mark_output('PathTracer.indirectAlbedo')
        # g.mark_output('PathTracer.guideNormal')
        # g.mark_output('PathTracer.reflectionPosW')
        # g.mark_output('PathTracer.rayCount')
        # g.mark_output('PathTracer.pathLength')
        
        
    g.add_edge('GBufferRT.mvec', 'TAA.motionVecs')
    g.add_edge('GBufferRT.emissive', 'ASVGFPass.Emission')
    g.add_edge('GBufferRT.linearZ', 'ASVGFPass.LinearZ')
    g.add_edge('GBufferRT.normW', 'ASVGFPass.Normals')
    g.add_edge('GBufferRT.vbuffer', 'ASVGFPass.CurrentVisibilityBuffer')
    g.add_edge('GBufferRT.mvec', 'ASVGFPass.MotionVectors')
    
    
    
    # g.mark_output('GBufferRT.emissive')
    # g.mark_output('GBufferRT.linearZ')
    # g.mark_output('GBufferRT.normW')
    # g.mark_output('GBufferRT.vbuffer')
    # g.mark_output('GBufferRT.mvec')
    # g.mark_output('GBufferRT.pnFwidth')
    return g
useCSVGF = True
ASVGF = render_graph_ASVGF(useCSVGF, isTemporalTrain=False, isSpatialTrain=False)
try: 
    m.addGraph(ASVGF)
    camera = m.scene.camera
    camera.nearPlane = 0.1     
except Exception as e:
    logging.error("An error occurred: %s", e)
    logging.error(traceback.format_exc())
#C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\scripts\Demo.py" -v0 --width=1280 --height=720 --gpu=0