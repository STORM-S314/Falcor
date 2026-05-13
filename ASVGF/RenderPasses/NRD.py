from falcor import *
import os

frame_output_path = r'D:/data/' if os.name == 'nt' else os.path.expanduser('~/data/')
denoised_frame_path = frame_output_path + '/Denoised'
nrd_path = denoised_frame_path + '/NRD'
scenes_path = r'D:/data' if os.name == 'nt' else os.path.expanduser('~/data/scenes')

def render_graph_PathTracerNRD():
    g = RenderGraph("PathTracerNRD")
    GBufferRT = createPass("GBufferRT", {'samplePattern': 'Halton', 'sampleCount': 32, 'useAlphaTest': True})
    g.addPass(GBufferRT, "GBufferRT")
    PathTracer = createPass("PathTracer", {'samplesPerPixel': 1, 'maxSurfaceBounces': 10, 'useRussianRoulette': True})
    g.addPass(PathTracer, "PathTracer")

    # Reference path passes
    AccumulatePass = createPass("AccumulatePass", {'enabled': True, 'precisionMode': 'Single'})
    g.addPass(AccumulatePass, "AccumulatePass")
    ToneMapperReference = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapperReference, "ToneMapperReference")

    # NRD path passes
    NRDDiffuseSpecular = createPass("NRD", {'maxIntensity': 250.0})
    g.addPass(NRDDiffuseSpecular, "NRDDiffuseSpecular")
    NRDDeltaReflection = createPass("NRD", {'method': 'RelaxDiffuse', 'maxIntensity': 250.0, 'worldSpaceMotion': False,
                                            'enableReprojectionTestSkippingWithoutMotion': True, 'spatialVarianceEstimationHistoryThreshold': 1})
    g.addPass(NRDDeltaReflection, "NRDDeltaReflection")
    NRDDeltaTransmission = createPass("NRD", {'method': 'RelaxDiffuse', 'maxIntensity': 250.0, 'worldSpaceMotion': False,
                                              'enableReprojectionTestSkippingWithoutMotion': True})
    g.addPass(NRDDeltaTransmission, "NRDDeltaTransmission")
    NRDReflectionMotionVectors = createPass("NRD", {'method': 'SpecularReflectionMv', 'worldSpaceMotion': False})
    g.addPass(NRDReflectionMotionVectors, "NRDReflectionMotionVectors")
    NRDTransmissionMotionVectors = createPass("NRD", {'method': 'SpecularDeltaMv', 'worldSpaceMotion': False})
    g.addPass(NRDTransmissionMotionVectors, "NRDTransmissionMotionVectors")
    ModulateIllumination = createPass("ModulateIllumination", {'useResidualRadiance': False})
    g.addPass(ModulateIllumination, "ModulateIllumination")
    DLSS = createPass("DLSSPass", {'enabled': True, 'profile': 'Balanced', 'motionVectorScale': 'Relative', 'isHDR': True, 'sharpness': 0.0, 'exposure': 0.0})
    g.addPass(DLSS, "DLSS")
    ToneMapperNRD = createPass("ToneMapper", {'autoExposure': False, 'exposureCompensation': 0.0})
    g.addPass(ToneMapperNRD, "ToneMapper")

    g.addEdge("GBufferRT.vbuffer",                                      "PathTracer.vbuffer")
    g.addEdge("GBufferRT.viewW",                                        "PathTracer.viewW")

    # Reference path graph
    g.addEdge("PathTracer.color",                                       "AccumulatePass.input")
    g.addEdge("AccumulatePass.output",                                  "ToneMapperReference.src")

    # NRD path graph
    g.addEdge("PathTracer.nrdDiffuseRadianceHitDist",                   "NRDDiffuseSpecular.diffuseRadianceHitDist")
    g.addEdge("PathTracer.nrdSpecularRadianceHitDist",                  "NRDDiffuseSpecular.specularRadianceHitDist")
    g.addEdge("GBufferRT.mvecW",                                        "NRDDiffuseSpecular.mvec")
    g.addEdge("GBufferRT.normWRoughnessMaterialID",                     "NRDDiffuseSpecular.normWRoughnessMaterialID")
    g.addEdge("GBufferRT.linearZ",                                      "NRDDiffuseSpecular.viewZ")

    g.addEdge("PathTracer.nrdDeltaReflectionHitDist",                   "NRDReflectionMotionVectors.specularHitDist")
    g.addEdge("GBufferRT.linearZ",                                      "NRDReflectionMotionVectors.viewZ")
    g.addEdge("GBufferRT.normWRoughnessMaterialID",                     "NRDReflectionMotionVectors.normWRoughnessMaterialID")
    g.addEdge("GBufferRT.mvec",                                         "NRDReflectionMotionVectors.mvec")

    g.addEdge("PathTracer.nrdDeltaReflectionRadianceHitDist",           "NRDDeltaReflection.diffuseRadianceHitDist")
    g.addEdge("NRDReflectionMotionVectors.reflectionMvec",              "NRDDeltaReflection.mvec")
    g.addEdge("PathTracer.nrdDeltaReflectionNormWRoughMaterialID",      "NRDDeltaReflection.normWRoughnessMaterialID")
    g.addEdge("PathTracer.nrdDeltaReflectionPathLength",                "NRDDeltaReflection.viewZ")

    g.addEdge("GBufferRT.posW",                                         "NRDTransmissionMotionVectors.deltaPrimaryPosW")
    g.addEdge("PathTracer.nrdDeltaTransmissionPosW",                    "NRDTransmissionMotionVectors.deltaSecondaryPosW")
    g.addEdge("GBufferRT.mvec",                                         "NRDTransmissionMotionVectors.mvec")

    g.addEdge("PathTracer.nrdDeltaTransmissionRadianceHitDist",         "NRDDeltaTransmission.diffuseRadianceHitDist")
    g.addEdge("NRDTransmissionMotionVectors.deltaMvec",                 "NRDDeltaTransmission.mvec")
    g.addEdge("PathTracer.nrdDeltaTransmissionNormWRoughMaterialID",    "NRDDeltaTransmission.normWRoughnessMaterialID")
    g.addEdge("PathTracer.nrdDeltaTransmissionPathLength",              "NRDDeltaTransmission.viewZ")

    g.addEdge("PathTracer.nrdEmission",                                 "ModulateIllumination.emission")
    g.addEdge("PathTracer.nrdDiffuseReflectance",                       "ModulateIllumination.diffuseReflectance")
    g.addEdge("NRDDiffuseSpecular.filteredDiffuseRadianceHitDist",      "ModulateIllumination.diffuseRadiance")
    g.addEdge("PathTracer.nrdSpecularReflectance",                      "ModulateIllumination.specularReflectance")
    g.addEdge("NRDDiffuseSpecular.filteredSpecularRadianceHitDist",     "ModulateIllumination.specularRadiance")
    g.addEdge("PathTracer.nrdDeltaReflectionEmission",                  "ModulateIllumination.deltaReflectionEmission")
    g.addEdge("PathTracer.nrdDeltaReflectionReflectance",               "ModulateIllumination.deltaReflectionReflectance")
    g.addEdge("NRDDeltaReflection.filteredDiffuseRadianceHitDist",      "ModulateIllumination.deltaReflectionRadiance")
    g.addEdge("PathTracer.nrdDeltaTransmissionEmission",                "ModulateIllumination.deltaTransmissionEmission")
    g.addEdge("PathTracer.nrdDeltaTransmissionReflectance",             "ModulateIllumination.deltaTransmissionReflectance")
    g.addEdge("NRDDeltaTransmission.filteredDiffuseRadianceHitDist",    "ModulateIllumination.deltaTransmissionRadiance")
    g.addEdge("PathTracer.nrdResidualRadianceHitDist",                  "ModulateIllumination.residualRadiance")

    g.addEdge("GBufferRT.mvec",                                         "DLSS.mvec")
    g.addEdge("GBufferRT.linearZ",                                      "DLSS.depth")
    g.addEdge("ModulateIllumination.output",                            "DLSS.color")

    g.addEdge("DLSS.output",                                            "ToneMapper.src")

    # Outputs
    g.markOutput("ToneMapper.dst")
    #g.markOutput("ToneMapperReference.dst")

    return g

PathTracerNRD = render_graph_PathTracerNRD()
try:
    print("==================CAPUTRE======================")
    # scene_path = scenes_path + '/Bistro_v5_2/BistroExterior.pyscene'
    # scene_path = scenes_path + '/Bistro_v5_2/BistroInterior_Wine.pyscene'
    # scene_path = scenes_path + "/SunTemple_v4/SunTemple/SunTemple.pyscene"
    # scene_path = scenes_path + "/EmeraldSquare_v4_1/EmeraldSquare_Day.pyscene"
    # scene_path = scenes_path + "/EmeraldSquare_v4_1/EmeraldSquare_Dusk.pyscene"
    # scene_path = scenes_path + "/ZeroDay_v1/ZeroDay_One.pyscene"
    scene_path = scenes_path + "/ZeroDay_v1/ZeroDay_Seven.pyscene"
    m.loadScene(scene_path)
    m.addGraph(PathTracerNRD)

    camera = m.scene.camera
    camera.nearPlane = 0.1 
    
    m.clock.pause()
    m.clock.framerate = 15
    frames = 100
    start_frame_idx = 100
    end_frame_idx = start_frame_idx + frames
    step = 1
    m.profiler.enabled = True
    m.profiler.start_capture()
    # frame capture
    nrd_path += '/' + scene_path.split('/')[-1].split('.')[0] + f'/{m.clock.framerate}FPS'
    if not os.path.exists(nrd_path):
        os.makedirs(nrd_path)
    m.frameCapture.outputDir = nrd_path
    for i in range(end_frame_idx):
        m.clock.frame = i
        m.renderFrame()
        if i>=start_frame_idx  and i% step == 0:  
            m.frameCapture.capture()
            print(f"\rProgress: {i + 1 - start_frame_idx}/{end_frame_idx - start_frame_idx} frames captured")
    capture = m.profiler.end_capture()
    m.profiler.enabled = False

    frameCount = capture["frame_count"]    
    lastFrameTime_denoise = capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDiffuseSpecular/gpu_time"]["records"][frameCount - 1] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDReflectionMotionVectors/gpu_time"]["records"][frameCount - 1] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaReflection/gpu_time"]["records"][frameCount - 1] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDTransmissionMotionVectors/gpu_time"]["records"][frameCount - 1] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaTransmission/gpu_time"]["records"][frameCount - 1] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/ModulateIllumination/gpu_time"]["records"][frameCount - 1] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/DLSS/gpu_time"]["records"][frameCount - 1]
    
    meanFrameTime_denoise = 0
    
    print(f"Frame Count: {frameCount}")
    print(f"Last frame gpu time:\n\t Denoise {lastFrameTime_denoise} ms")
    
    with open(nrd_path +"/NRD.csv", "w") as f:
        f.write("Frame ID, Denoise Time\n")
        for i in range(frameCount):
            if i >=start_frame_idx and i%step == 0:
                denoise_time = capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDiffuseSpecular/gpu_time"]["records"][i] + \
                    capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDReflectionMotionVectors/gpu_time"]["records"][i] + \
                    capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaReflection/gpu_time"]["records"][i] + \
                    capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDTransmissionMotionVectors/gpu_time"]["records"][i] + \
                    capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaTransmission/gpu_time"]["records"][i] + \
                    capture["events"]["/onFrameRender/RenderGraphExe::execute()/ModulateIllumination/gpu_time"]["records"][i] + \
                    capture["events"]["/onFrameRender/RenderGraphExe::execute()/DLSS/gpu_time"]["records"][i]
                meanFrameTime_denoise += denoise_time
                f.write(f"{i}, {denoise_time}\n")
    meanFrameTime_denoise /= frames
    print(f"Mean frame gpu time:\n\t Denoise {meanFrameTime_denoise} ms")
    exit()

except NameError: None

#C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\ASVGF\RenderPasses\NRD.py" -v2 --width=1280 --height=720 --gpu=0