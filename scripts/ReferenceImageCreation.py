import time
from falcor import *

frame_output_path = r'D:/data/' if os.name == 'nt' else os.path.expanduser('~/data')
gt_frame_path = frame_output_path + '/GT'
scenes_path = r'D:/data' if os.name == 'nt' else os.path.expanduser('~/data/scenes')

SPP = 64
Frames = 64
def render_graph_DefaultRenderGraph():
    g = RenderGraph('Reference')
    g.create_pass('GBufferRT', 'GBufferRT', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': SPP, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('AccumulatePass', 'AccumulatePass', {'enabled': True, 'maxAccumulatedFrames': Frames, 'precisionMode': 'Double', 'autoReset': True, 'overflowMode': 'Reset'})
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})    
    g.add_edge('GBufferRT.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRT.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRT.viewW', 'PathTracer.viewW')
    g.add_edge('GBufferRT.mvec', 'TAA.motionVecs')
    g.add_edge('PathTracer.color', 'AccumulatePass.input')    
    # g.add_edge('TAA.colorOut', 'AccumulatePass.input')
    g.add_edge('AccumulatePass.output', 'TAA.colorIn')
    g.add_edge('TAA.colorOut', 'ToneMapper.src')
    # g.add_edge('AccumulatePass.output', 'ToneMapper.src')    
    
    # g.add_edge('AccumulatePass.output', 'TAA.colorIn')
    # g.add_edge('TAA.colorOut', 'ToneMapper.src')
    
    g.mark_output('ToneMapper.dst')    
    return g

ReferenceRenderGraph = render_graph_DefaultRenderGraph()
try:
    print("==================CAPUTRE======================")
    # scene_path = scenes_path + "/Bistro_v5_2/BistroExterior.pyscene"
    scene_path = scenes_path + "/EmeraldSquare_v4_1/EmeraldSquare_Day.pyscene"
    # scene_path = scenes_path + "/SunTemple_v4/SunTemple/SunTemple.pyscene"
    
    m.loadScene(scene_path)
    m.addGraph(ReferenceRenderGraph)
    camera = m.scene.camera
    camera.nearPlane = 0.1 

    m.clock.pause()
    m.clock.framerate = 60

    gt_output_path = gt_frame_path + '/' + scene_path.split('/')[-1].split('.')[0] + '/32SPP' + f'/{m.clock.framerate}FPS'
    print(f"gt output path={gt_output_path}")
    m.frameCapture.outputDir = gt_output_path
    start_frame_idx = 100
    end_frame_idx = 400
    step = 10
    
    for frame_idx in range(start_frame_idx,end_frame_idx,step):
        for sample in range(Frames):
            m.clock.frame = frame_idx
            m.renderFrame()                                    
        m.frameCapture.capture()
        print(f"\rGT Captured: {frame_idx + 1 - start_frame_idx} / {end_frame_idx - start_frame_idx}",end='')                
    
    exit()
except NameError: None

#/home/lijing/Falcor/build/linux-gcc/bin/Release/Mogwai --script="/home/lijing/Falcor/ASVGF/RenderPasses/ReferenceImageCreation.py" -v2 --width=1280 --height=720 --gpu=0 --headless -v5