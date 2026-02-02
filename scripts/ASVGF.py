from pathlib import WindowsPath, PosixPath
from falcor import *
import time
import os

def render_graph_ASVGF(useCSVGF = False):
    g = RenderGraph('ASVGF')
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('ASVGFPass', 'ASVGFPass', {'UseCSVGF': useCSVGF})
    if not useCSVGF:
        g.create_pass('GradForwardProjPass', 'GradForwardProjPass', {'UseCSVGF': useCSVGF})
        g.create_pass('PathTracerMod', 'PathTracerMod', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'useSER' : False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    else:
        g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'useSER': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    
    
    g.mark_output('ToneMapper.dst')
    g.mark_output('ASVGFPass.Debug Output image')

    g.add_edge('TAA.colorOut', 'ToneMapper.src')
    g.add_edge('ASVGFPass.Filtered image', 'TAA.colorIn')

    # g.add_edge('ASVGFPass.Filtered image', 'ToneMapper.src')
    if not useCSVGF:
        g.add_edge('PathTracerMod.albedo', 'ASVGFPass.Albedo')
        g.add_edge('PathTracerMod.color', 'ASVGFPass.Color')
        g.add_edge('PathTracerMod.specularAlbedo', 'ASVGFPass.SpecularAlbedo')
        g.add_edge('GradForwardProjPass.OutGradVisibilityBuffer', 'PathTracerMod.vbuffer')
        g.add_edge('GBufferRaster.mvec', 'PathTracerMod.mvec')
        g.add_edge('GradForwardProjPass.OutGradWViewBuffer', 'PathTracerMod.viewW')
        g.add_edge('GradForwardProjPass.OutRandomNumberBuffer', 'PathTracerMod.randomNumbers')
        g.add_edge('GradForwardProjPass.OutGradSamplesBuffer', 'ASVGFPass.GradientSamples')
        g.add_edge('GBufferRaster.normW', 'GradForwardProjPass.InWorldNormal')
        g.add_edge('GBufferRaster.vbuffer', 'GradForwardProjPass.InVisibilityBuffer')
        g.add_edge('GBufferRaster.posW', 'GradForwardProjPass.InWPos')
        g.add_edge('GBufferRaster.viewW', 'GradForwardProjPass.InWViewBuffer')
        g.add_edge('GBufferRaster.linearZ', 'GradForwardProjPass.InLinearZ')
        g.add_edge('GradForwardProjPass.OutGradVisibilityBuffer', 'ASVGFPass.GradientVisibilityBuffer')
        # g.mark_output('PathTracerMod.albedo')
        # g.mark_output('PathTracerMod.color')
        # g.mark_output('GradForwardProjPass.OutGradSamplesBuffer')
        # g.mark_output('GradForwardProjPass.OutGradVisibilityBuffer')
    else:
        g.add_edge('PathTracer.albedo', 'ASVGFPass.Albedo')
        g.add_edge('PathTracer.color', 'ASVGFPass.Color')
        g.add_edge('PathTracer.specularAlbedo', 'ASVGFPass.SpecularAlbedo')
        g.add_edge('GBufferRaster.vbuffer', 'PathTracer.vbuffer')
        g.add_edge('GBufferRaster.mvec', 'PathTracer.mvec')
        g.add_edge('GBufferRaster.viewW', 'PathTracer.viewW')
        g.add_edge('GBufferRaster.vbuffer', 'ASVGFPass.GradientVisibilityBuffer')
        # g.mark_output('PathTracer.albedo')
        # g.mark_output('PathTracer.color')
        # g.mark_output('PathTracer.specularAlbedo')
        
    g.add_edge('GBufferRaster.mvec', 'TAA.motionVecs')
    g.add_edge('GBufferRaster.emissive', 'ASVGFPass.Emission')
    g.add_edge('GBufferRaster.linearZ', 'ASVGFPass.LinearZ')
    g.add_edge('GBufferRaster.normW', 'ASVGFPass.Normals')
    g.add_edge('GBufferRaster.vbuffer', 'ASVGFPass.CurrentVisibilityBuffer')
    g.add_edge('GBufferRaster.mvec', 'ASVGFPass.MotionVectors')
    
    
    
    # g.mark_output('GBufferRaster.emissive')
    # g.mark_output('GBufferRaster.linearZ')
    # g.mark_output('GBufferRaster.normW')
    # g.mark_output('GBufferRaster.vbuffer')
    # g.mark_output('GBufferRaster.mvec')
    # g.mark_output('GBufferRaster.pnFwidth')
    return g
useCSVGF = True
ASVGF = render_graph_ASVGF(useCSVGF)
try: 
    print("==================CAPUTRE======================")
    scene_path = "D:\\data\\Bistro_v5_2\\BistroExterior.pyscene"
    # scene_path = "D:\\data\\Bistro_v5_2\\BistroInterior_Wine.pyscene"
    # scene_path = "D:\\data\\SunTemple_v4\\SunTemple\\SunTemple.pyscene"
    # scene_path = "D:\\data\\EmeraldSquare_v4_1\\EmeraldSquare_Day.pyscene"
    # scene_path = "D:\\data\\bathroom\\bathroom.pyscene"
    # scene_path = "C:\\Users\\storm\\Documents\\GitHub\\Falcor\\media\\test_scenes\\cornell_box_bunny.pyscene"
    # scene_path = "D:\\data\\ZeroDay_v1\\ZeroDay_One.pyscene"
    m.loadScene(scene_path, buildFlags=SceneBuilderFlags.UseCache)
    m.addGraph(ASVGF)
    camera = m.scene.camera
    camera.nearPlane = 0.1 
    
    m.clock.pause()
    m.clock.framerate = 60
    frames = 300
    start_frame_idx = 100
    m.profiler.enabled = True
    m.profiler.start_capture()
    # frame capture
    if not useCSVGF:
        m.frameCapture.outputDir = "D:\\data\\frames\\ASVGF"
    else:
        m.frameCapture.outputDir = "D:\\data\\frames\\CSVGF"
    for i in range(start_frame_idx + frames + 1):
        m.clock.frame = i
        m.renderFrame()
        if i >= start_frame_idx:
            m.frameCapture.capture()
            print(f"\rProgress: {i - start_frame_idx}/{frames} frames captured")        
    capture = m.profiler.end_capture()
    m.profiler.enabled = False

    frameCount = capture["frame_count"]
    if not useCSVGF:
        lastFrameTime_reProj = capture["events"]["/onFrameRender/RenderGraphExe::execute()/GradForwardProjPass/gpu_time"]["records"][frameCount - 1]
        meanFrameTime_reProj = capture["events"]["/onFrameRender/RenderGraphExe::execute()/GradForwardProjPass/gpu_time"]["stats"]["mean"]
    else:
        lastFrameTime_reProj = 0
        meanFrameTime_reProj = 0
    lastFrameTime_denoise = capture["events"]["/onFrameRender/RenderGraphExe::execute()/ASVGFPass/gpu_time"]["records"][frameCount - 1]
    meanFrameTime_denoise = capture["events"]["/onFrameRender/RenderGraphExe::execute()/ASVGFPass/gpu_time"]["stats"]["mean"]
    print(f"Frame Count: {frameCount}")
    print(f"Last frame gpu time:\n\t GradReproj {lastFrameTime_reProj} ms \n\tDenoise {lastFrameTime_denoise} ms")
    print(f"Mean frame gpu time:\n\t GradReproj {meanFrameTime_reProj} ms \n\tDenoise {meanFrameTime_denoise} ms")
    if not useCSVGF:
        with open(r"D:\\data\\frames\\ASVGF\\ASVGF.csv", "w") as f:
            f.write("Frame ID, GradReproj Time, Denoise Time\n")
            for i in range(frameCount):
                f.write(f"{i}, {capture['events']['/onFrameRender/RenderGraphExe::execute()/GradForwardProjPass/gpu_time']['records'][i]}, {capture['events']['/onFrameRender/RenderGraphExe::execute()/ASVGFPass/gpu_time']['records'][i]}\n")
    else:
        with open(r"D:\\data\\frames\\CSVGF\\CSVGF.csv", "w") as f:
            f.write("Frame ID, Denoise Time\n")
            for i in range(frameCount):
                f.write(f"{i}, {capture['events']['/onFrameRender/RenderGraphExe::execute()/ASVGFPass/gpu_time']['records'][i]}\n")
    exit()



except NameError: None
except Exception as e:
    print(e)
#C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\scripts\ASVGF.py" -v2 --width=1280 --height=720 --gpu=0
#.\RenderGraphEditor --editor --graph-file "C:\Users\storm\Documents\GitHub\Falcor\scripts\ASVGF.py" --graph-name ASVGF