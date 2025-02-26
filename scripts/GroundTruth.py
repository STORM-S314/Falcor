from pathlib import WindowsPath, PosixPath
from falcor import *
import time
import math

def render_graph_GT():
    g = RenderGraph('GT')
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 512, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'useSER': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})

    g.add_edge('GBufferRaster.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRaster.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRaster.viewW', 'PathTracer.viewW')
    g.add_edge('PathTracer.color', 'ToneMapper.src')
    g.mark_output('ToneMapper.dst')
    g.mark_output('GBufferRaster.posW')
    g.mark_output('GBufferRaster.normW')
    g.mark_output('GBufferRaster.pnFwidth')
    g.mark_output('GBufferRaster.linearZ')
    g.mark_output('GBufferRaster.mvec')
    g.mark_output('GBufferRaster.emissive')
    g.mark_output('PathTracer.color')
    g.mark_output('PathTracer.albedo')
    return g



GT = render_graph_GT()
try:
    
    # print("==================CAPUTRE======================")
    # print(dir(m.frameCapture))
    # scene_path = "C:\\Users\\storm\\Documents\\GitHub\\Falcor\\media\\test_scenes\\cornell_box_bunny.pyscene"
    # scene_path = "C:\\Users\\storm\\Documents\\GitHub\\Falcor\\media\\Arcade\\Arcade.pyscene"
    # scene_path = "D:\\data\\SunTemple_v4\\SunTemple\\SunTemple.pyscene"
    scene_path = "D:\\data\\Bistro_v5_2\\BistroExterior.pyscene"
    # scene_path = "D:\\data\\EmeraldSquare_v4_1\\EmeraldSquare_Day.pyscene"
    # scene_path = "D:\\data\\ZeroDay_v1\\ZeroDay_One.pyscene"
    # scene_path = "D:\\data\\ZeroDay_v1\\ZeroDay_Seven.pyscene"
    # scene_path = "D:\\data\\bathroom\\bathroom.pyscene"
    m.loadScene(scene_path)
    m.addGraph(GT)
    camera = m.scene.camera
    camera.nearPlane = 0.1 
    # frame capture
    m.clock.pause()
    m.frameCapture.outputDir = "D:\\data\\frames\\GT"
    for i in range(250):
        renderFrame()
        m.frameCapture.baseFilename = f"{i:04d}"
        m.frameCapture.capture()
        print(f"\r=======================Rendered frames{i + 1:04d}==============================",end='',flush=True)
    print('============================Done!=================================')
    exit()
    
    
except NameError: None

#.\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\scripts\GroundTruth.py" -v2 --width=1280 --height=720 --gpu=0