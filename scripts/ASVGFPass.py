from falcor import *

def render_graph_ASVGF():
    g = RenderGraph('ASVGFPass')
    #loadRenderPassLibrary('WireframePass.dll')
    ASVGF = createPass('ASVGFPass')
    g.addPass(ASVGF, 'ASVGFPass')
    g.markOutput('ASVGFPass.output')
    return g

ASVGFPass = render_graph_ASVGF()
try: m.addGraph(ASVGFPass)
except NameError: None