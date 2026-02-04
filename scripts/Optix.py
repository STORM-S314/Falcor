from pathlib import WindowsPath, PosixPath
from falcor import *
import time
def render_graph_Optix():
    g = RenderGraph('Optix')
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': True, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('GBufferRT', 'GBufferRT', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('Optix', 'OptixDenoiser', {})
    
    g.add_edge('PathTracer.color', 'Optix.color')
    g.add_edge('PathTracer.albedo', 'Optix.albedo')
    g.add_edge('PathTracer.guideNormal', 'Optix.normal')
    g.add_edge('GBufferRT.mvec', 'Optix.mvec')
    g.add_edge('GBufferRT.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRT.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRT.viewW', 'PathTracer.viewW')
    g.add_edge('GBufferRT.mvec', 'TAA.motionVecs')
    g.add_edge('Optix.output', 'TAA.colorIn')
    g.add_edge('TAA.colorOut', 'ToneMapper.src')

    # g.add_edge('Optix.output', 'ToneMapper.src')

    g.mark_output('ToneMapper.dst')
    return g



Optix = render_graph_Optix()
try: 
    print("==================CAPUTRE======================")    
    # scene_path = "C:\\Users\\storm\\Documents\\GitHub\\Falcor\\media\\test_scenes\\cornell_box_bunny.pyscene"
    # scene_path = "C:\\Users\\storm\\Documents\\GitHub\\Falcor\\media\\Arcade\\Arcade.pyscene"
    # scene_path = "D:\\data\\SunTemple_v4\\SunTemple\\SunTemple.pyscene"
    scene_path = "D:\\data\\Bistro_v5_2\\BistroExterior.pyscene"
    # scene_path = "D:\\data\\EmeraldSquare_v4_1\\EmeraldSquare_Day.pyscene"
    # scene_path = "D:\\data\\ZeroDay_v1\\ZeroDay_One.pyscene"
    # scene_path = "D:\\data\\ZeroDay_v1\\ZeroDay_Seven.pyscene"
    # scene_path = "D:\\data\\bathroom\\bathroom.pyscene"
    m.loadScene(scene_path)
    m.addGraph(Optix)
    camera = m.scene.camera
    camera.nearPlane = 0.1 
    
    m.clock.pause()
    m.clock.framerate = 60
    frames = 300
    start_frame_idx = 100
    m.profiler.enabled = True
    m.profiler.start_capture()
    # frame capture
    m.frameCapture.outputDir = "D:\\data\\frames\\Optix"
    for i in range(start_frame_idx + frames + 1):
        m.clock.frame = i
        m.renderFrame()
        if i >= start_frame_idx:        
            m.frameCapture.capture()
            print(f"\rProgress: {i - start_frame_idx}/{frames} frames captured")
    capture = m.profiler.end_capture()
    m.profiler.enabled = False

    frameCount = capture["frame_count"]    
    lastFrameTime_denoise = capture["events"]["/onFrameRender/RenderGraphExe::execute()/OptixPass/gpu_time"]["records"][frameCount - 1]
    meanFrameTime_denoise = capture["events"]["/onFrameRender/RenderGraphExe::execute()/OptixPass/gpu_time"]["stats"]["mean"]
    print(f"Frame Count: {frameCount}")
    print(f"Last frame gpu time:\n\t Denoise {lastFrameTime_denoise} ms")
    print(f"Mean frame gpu time:\n\t Denoise {meanFrameTime_denoise} ms")
    with open(r"D:\\data\\frames\\Optix\\Optix.csv", "w") as f:
        f.write("Frame ID, Denoise Time\n")
        for i in range(frameCount):
            f.write(f"{i}, {capture['events']['/onFrameRender/RenderGraphExe::execute()/OptixPass/gpu_time']['records'][i]}\n")
    exit()
    
    
except NameError: None

#C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\scripts\Optix.py" -v2 --width=1280 --height=720 --gpu=0
#.\RenderGraphEditor --editor --graph-file "C:\Users\storm\Documents\GitHub\Falcor\scripts\Optix.py" --graph-name Optix