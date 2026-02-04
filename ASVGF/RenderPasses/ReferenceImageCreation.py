import time
from falcor import *

SPP = 4096
Frames = 1024

def render_graph_DefaultRenderGraph():
    g = RenderGraph('Reference')
    g.create_pass('GBufferRT', 'GBufferRT', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': SPP, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': True, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('AccumulatePass', 'AccumulatePass', {'enabled': True, 'maxAccumulatedFrames': Frames, 'precisionMode': 'Double', 'autoReset': True, 'overflowMode': 'Reset'})    
    g.add_edge('GBufferRT.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRT.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRT.viewW', 'PathTracer.viewW')    
    g.add_edge('PathTracer.color', 'AccumulatePass.input')            
    g.add_edge('AccumulatePass.output', 'ToneMapper.src')
        
    g.mark_output('ToneMapper.dst')    
    return g

ReferenceRenderGraph = render_graph_DefaultRenderGraph()
try:
    #scene_path = "/home/lijing/data/scenes/EmeraldSquare_v4_1/EmeraldSquare_Day.pyscene" 
    scene_path = "/home/lijing/data/scenes/Bistro_v5_2/BistroExterior.pyscene"
    #scene_path = "/home/lijing/data/scenes/SunTemple_v4/SunTemple/SunTemple.pyscene"
    m.loadScene(scene_path)    
    m.addGraph(ReferenceRenderGraph)    
    camera = m.scene.camera
    camera.nearPlane = 0.1 

    m.clock.pause()
    m.clock.framerate = 60

    #m.frameCapture.outputDir = "/home/lijing/data/GT/EmeraldSquare_Day/32SPP/144FPS"
    m.frameCapture.outputDir = "/home/lijing/data/GT/BistroExterior/32SPP/60FPS"   
    #m.frameCapture.outputDir = "/home/lijing/data/GT/SunTemple/60FPS" 
    start_frame_id = 235
    end_frame_id = 300
    
    for frame_id in range(start_frame_id, end_frame_id):
        m.clock.frame = frame_id
        for frame in range(Frames):
            m.clock.frame = frame_id
            m.renderFrame()                
        m.frameCapture.capture()
        print(f"\rGT Captured: {frame_id + 1 - start_frame_id} / {end_frame_id - start_frame_id}",end='')
    exit()
except NameError: None

#/home/lijing/Falcor/build/linux-gcc/bin/Release/Mogwai --script="/home/lijing/Falcor/ASVGF/RenderPasses/ReferenceImageCreation.py" -v2 --width=1280 --height=720 --gpu=0 --headless -v5