from pathlib import WindowsPath, PosixPath
from falcor import *
import time
def render_graph_SVGF():
    g = RenderGraph('SVGF')
    g.create_pass('TAA', 'TAA', {'alpha': 0.10000000149011612, 'colorBoxSigma': 1.0, 'antiFlicker': True})
    g.create_pass('PathTracer', 'PathTracer', {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'maxDiffuseBounces': 3, 'maxSpecularBounces': 3, 'maxTransmissionBounces': 10, 'sampleGenerator': 0, 'useBSDFSampling': True, 'useRussianRoulette': False, 'useNEE': True, 'useMIS': True, 'misHeuristic': 'Balance', 'misPowerExponent': 2.0, 'emissiveSampler': 'LightBVH', 'lightBVHOptions': {'buildOptions': {'splitHeuristicSelection': 'BinnedSAOH', 'maxTriangleCountPerLeaf': 10, 'binCount': 16, 'volumeEpsilon': 0.0010000000474974513, 'splitAlongLargest': False, 'useVolumeOverSA': False, 'useLeafCreationCost': True, 'createLeavesASAP': True, 'allowRefitting': True, 'usePreintegration': True, 'useLightingCones': True}, 'useBoundingCone': True, 'useLightingCone': True, 'disableNodeFlux': False, 'useUniformTriangleSampling': True, 'solidAngleBoundMethod': 'Sphere'}, 'useRTXDI': False, 'RTXDIOptions': {'mode': 'SpatiotemporalResampling', 'presampledTileCount': 128, 'presampledTileSize': 1024, 'storeCompactLightInfo': True, 'localLightCandidateCount': 24, 'infiniteLightCandidateCount': 8, 'envLightCandidateCount': 8, 'brdfCandidateCount': 1, 'brdfCutoff': 0.0, 'testCandidateVisibility': True, 'biasCorrection': 'Basic', 'depthThreshold': 0.10000000149011612, 'normalThreshold': 0.5, 'samplingRadius': 30.0, 'spatialSampleCount': 1, 'spatialIterations': 5, 'maxHistoryLength': 20, 'boilingFilterStrength': 0.0, 'rayEpsilon': 0.0010000000474974513, 'useEmissiveTextures': False, 'enableVisibilityShortcut': False, 'enablePermutationSampling': False}, 'useAlphaTest': True, 'adjustShadingNormals': False, 'maxNestedMaterials': 2, 'useLightsInDielectricVolumes': False, 'disableCaustics': False, 'specularRoughnessThreshold': 0.25, 'primaryLodMode': 'Mip0', 'lodBias': 0.0, 'useNRDDemodulation': True, 'useSER': False, 'outputSize': 'Default', 'colorFormat': 'LogLuvHDR'})
    g.create_pass('ToneMapper', 'ToneMapper', {'outputSize': 'Default', 'useSceneMetadata': True, 'exposureCompensation': 0.0, 'autoExposure': False, 'filmSpeed': 100.0, 'whiteBalance': False, 'whitePoint': 6500.0, 'operator': 'Aces', 'clamp': True, 'whiteMaxLuminance': 1.0, 'whiteScale': 11.199999809265137, 'fNumber': 1.0, 'shutter': 1.0, 'exposureMode': 'AperturePriority'})
    g.create_pass('GBufferRaster', 'GBufferRaster', {'outputSize': 'Default', 'samplePattern': 'Center', 'sampleCount': 16, 'useAlphaTest': True, 'adjustShadingNormals': True, 'forceCullMode': False, 'cull': 'Back'})
    g.create_pass('MultipleIlluminationModulator', 'MultipleIlluminationModulator', {})
    g.create_pass('SVGFPass', 'SVGFPass', {'Enabled': True, 'Iterations': 4, 'FeedbackTap': 1, 'VarianceEpsilon': 9.999999747378752e-05, 'PhiColor': 10.0, 'PhiNormal': 128.0, 'Alpha': 0.05000000074505806, 'MomentsAlpha': 0.20000000298023224, 'kIsDirectIllumination': False})
    g.create_pass('SVGFPass0', 'SVGFPass', {'Enabled': True, 'Iterations': 4, 'FeedbackTap': 1, 'VarianceEpsilon': 9.999999747378752e-05, 'PhiColor': 10.0, 'PhiNormal': 128.0, 'Alpha': 0.05000000074505806, 'MomentsAlpha': 0.20000000298023224, 'kIsDirectIllumination': True})
    g.add_edge('TAA.colorOut', 'ToneMapper.src')
    g.add_edge('PathTracer.color', 'SVGFPass.Color')
    g.add_edge('PathTracer.albedo', 'SVGFPass.Albedo')
    g.add_edge('GBufferRaster.vbuffer', 'PathTracer.vbuffer')
    g.add_edge('GBufferRaster.mvec', 'PathTracer.mvec')
    g.add_edge('GBufferRaster.viewW', 'PathTracer.viewW')
    g.add_edge('GBufferRaster.posW', 'SVGFPass.WorldPosition')
    g.add_edge('GBufferRaster.normW', 'SVGFPass.WorldNormal')
    g.add_edge('GBufferRaster.linearZ', 'SVGFPass.LinearZ')
    g.add_edge('GBufferRaster.mvec', 'SVGFPass.MotionVec')
    g.add_edge('GBufferRaster.pnFwidth', 'SVGFPass.PositionNormalFwidth')
    g.add_edge('GBufferRaster.mvec', 'TAA.motionVecs')
    g.add_edge('GBufferRaster.emissive', 'SVGFPass.Emission')
    g.add_edge('PathTracer.specularAlbedo', 'SVGFPass.Specular')
    g.add_edge('PathTracer.indirectAlbedo', 'SVGFPass.IndirectAlbedo')
    g.add_edge('PathTracer.color', 'SVGFPass0.Color')
    g.add_edge('PathTracer.specularAlbedo', 'SVGFPass0.Specular')
    g.add_edge('PathTracer.indirectAlbedo', 'SVGFPass0.IndirectAlbedo')
    g.add_edge('PathTracer.albedo', 'SVGFPass0.Albedo')
    g.add_edge('GBufferRaster.emissive', 'SVGFPass0.Emission')
    g.add_edge('GBufferRaster.posW', 'SVGFPass0.WorldPosition')
    g.add_edge('GBufferRaster.linearZ', 'SVGFPass0.LinearZ')
    g.add_edge('GBufferRaster.pnFwidth', 'SVGFPass0.PositionNormalFwidth')
    g.add_edge('GBufferRaster.normW', 'SVGFPass0.WorldNormal')
    g.add_edge('GBufferRaster.mvec', 'SVGFPass0.MotionVec')
    g.add_edge('SVGFPass.Filtered image', 'MultipleIlluminationModulator.Illumination1')
    g.add_edge('SVGFPass0.Filtered image', 'MultipleIlluminationModulator.Illumination2')
    g.add_edge('PathTracer.albedo', 'MultipleIlluminationModulator.Albedo')
    g.add_edge('GBufferRaster.emissive', 'MultipleIlluminationModulator.Emission')
    g.add_edge('PathTracer.specularAlbedo', 'MultipleIlluminationModulator.SpecularAlbedo')
    g.add_edge('MultipleIlluminationModulator.ModulatedImage', 'TAA.colorIn')
    g.mark_output('ToneMapper.dst')
    return g



SVGF = render_graph_SVGF()
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
    m.addGraph(SVGF)
    camera = m.scene.camera
    camera.nearPlane = 0.1 
    
    # m.clock.pause()
    # m.clock.framerate = 120
    # m.clock.frame = 0
    # frame_count = 2401
    # # frame capture
    # m.frameCapture.outputDir = "D:\\data\\frames\\SVGF"
    # for frame_id in range(frame_count):
    #     m.renderFrame()
    #     m.clock.step(1)
    #     # m.frameCapture.capture()
    #     # if (frame_id + 1% 10 == 0) or frame_id == frame_count - 1:
    #     #     print(f"\rProgress: {frame_id + 1}/{frame_count} frames captured")
    #     #     time.sleep(0.01)
    # # exit()

    # # camera = m.scene.camera
    # # start_time = time.time()
    # # while True:
    # #     current_time = time.time()
    # #     elapsed_time = current_time - start_time
    # #     amplitude = 1
    # #     frequency = 1
    # #     x = amplitude * math.sin(frequency * elapsed_time) - 1.0
    # #     z = -amplitude * math.sin(frequency * elapsed_time) + 2.0
    # #     camera.position = float3(x, 2.28, z)
    # #     # camera.target = float3(x, 1.4863656759262086, z)
    # #     renderFrame()
    
    
except NameError: None

#C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\scripts\SVGF.py" -v2 --width=1280 --height=720 --gpu=0
#.\RenderGraphEditor --editor --graph-file "C:\Users\storm\Documents\GitHub\Falcor\scripts\SVGF.py" --graph-name SVGF