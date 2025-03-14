import time
from falcor import *


def render_graph_DefaultRenderGraph():
    g = RenderGraph('Reference')
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('ImageAggregator', 'ImageAggregator', {})
    g.create_pass('OptixDenoiser', 'OptixDenoiser',{})
    g.add_edge('GBufferRaster.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRaster.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRaster.viewW', 'PathTracer.viewW')
    g.add_edge('PathTracer.color', 'ImageAggregator.Color')
    g.add_edge('ImageAggregator.AggregatedImage', 'OptixDenoiser.color')
    g.add_edge('OptixDenoiser.output', 'ToneMapper.src')
    
    g.mark_output('ToneMapper.dst')
    g.mark_output('OptixDenoiser.output')
    g.mark_output('ImageAggregator.AggregatedImage')
    return g

ReferenceRenderGraph = render_graph_DefaultRenderGraph()
try:
    scene_path = "D:\\data\\Bistro_v5_2\\BistroExterior.pyscene" 
    m.loadScene(scene_path)
    m.addGraph(ReferenceRenderGraph)
    camera = m.scene.camera
    camera.nearPlane = 0.1 

    m.clock.pause()
    m.clock.framerate = 144

    m.frameCapture.outputDir = "D:\\data\\frames\\GT"
    frame_size = 1000
    
    for frame_id in range(100,frame_size):
        imageAggregatorPass = ReferenceRenderGraph.getPass('ImageAggregator')
        config = imageAggregatorPass.getDictionary()
        config['IsAggregating'] = True
        config['ImageAggregationCount'] = 32
        ReferenceRenderGraph.updatePass('ImageAggregator',config)
        while True:
            m.clock.frame = frame_id
            m.renderFrame()
            imageAggregatorPass = ReferenceRenderGraph.getPass('ImageAggregator')
            config = imageAggregatorPass.getDictionary()
            if not config['IsAggregating']:
                m.frameCapture.capture()
                print(f"\rGT Captured: {frame_id + 1} / {frame_size}",end='')
                break
    
except NameError: None

# C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --script="C:\Users\storm\Documents\GitHub\Falcor\ASVGF\RenderPasses\ReferenceImageCreation.py" -v2 --width=1280 --height=720 --gpu=0 --headless
