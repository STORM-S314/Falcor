from falcor import *

def render_graph_ASVGF():
    g = RenderGraph('ASVGF')
    #loadRenderPassLibrary('WireframePass.dll')
    ASVGF = createPass('ASVGF')
    g.addPass(ASVGF, 'ASVGF')
    g.markOutput('ASVGF.output')
    return g

ASVGFPass = render_graph_ASVGF()
try: m.addGraph(ASVGFPass)
except NameError: None