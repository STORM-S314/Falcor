from falcor import *

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
    scene_path = "D:\\data\\Bistro_v5_2\\BistroExterior.pyscene"
    m.loadScene(scene_path)
    m.addGraph(PathTracerNRD)

    camera = m.scene.camera
    camera.nearPlane = 0.1 
    
    m.clock.pause()
    m.clock.framerate = 60
    frames = 300
    start_frame_idx = 100
    m.profiler.enabled = True
    m.profiler.start_capture()
    # frame capture
    m.frameCapture.outputDir = "D:\\data\\frames\\NRD"
    for i in range(start_frame_idx + frames + 1):
        m.clock.frame = i
        m.renderFrame()
        if i >= start_frame_idx:        
            m.frameCapture.capture()
            print(f"\rProgress: {i - start_frame_idx}/{frames} frames captured")
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
    
    meanFrameTime_denoise = capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDiffuseSpecular/gpu_time"]["stats"]["mean"] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDReflectionMotionVectors/gpu_time"]["stats"]["mean"] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaReflection/gpu_time"]["stats"]["mean"] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDTransmissionMotionVectors/gpu_time"]["stats"]["mean"] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaTransmission/gpu_time"]["stats"]["mean"] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/ModulateIllumination/gpu_time"]["stats"]["mean"] + \
                            capture["events"]["/onFrameRender/RenderGraphExe::execute()/DLSS/gpu_time"]["stats"]["mean"]
    
    print(f"Frame Count: {frameCount}")
    print(f"Last frame gpu time:\n\t Denoise {lastFrameTime_denoise} ms")
    print(f"Mean frame gpu time:\n\t Denoise {meanFrameTime_denoise} ms")
    with open(r"D:\\data\\frames\\NRD\\NRD.csv", "w") as f:
        f.write("Frame ID, Denoise Time\n")
        for i in range(frameCount):
            denoise_time = capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDiffuseSpecular/gpu_time"]["records"][i] + \
                capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDReflectionMotionVectors/gpu_time"]["records"][i] + \
                capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaReflection/gpu_time"]["records"][i] + \
                capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDTransmissionMotionVectors/gpu_time"]["records"][i] + \
                capture["events"]["/onFrameRender/RenderGraphExe::execute()/NRDDeltaTransmission/gpu_time"]["records"][i] + \
                capture["events"]["/onFrameRender/RenderGraphExe::execute()/ModulateIllumination/gpu_time"]["records"][i] + \
                capture["events"]["/onFrameRender/RenderGraphExe::execute()/DLSS/gpu_time"]["records"][i]
            f.write(f"{i}, {denoise_time}\n")
    exit()

except NameError: None

#C:\Users\storm\Documents\GitHub\Falcor\build\windows-vs2022\bin\Release\Mogwai.exe --headless --script="C:\Users\storm\Documents\GitHub\Falcor\scripts\NRD.py" -v2 --width=1280 --height=720 --gpu=0