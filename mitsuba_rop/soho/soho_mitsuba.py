# This is a Soho module for Mitsuba renderer:
# http://www.mitsuba-renderer.org by Wenzel Jakob

# General imports:
import os, sys, multiprocessing

# Soho imports:
import time
import math
import soho
import sohog
import hou
from soho import SohoParm
from sohog import SohoGeometry

# Add Mitsuba to the path
mitsuba_path = os.getenv("MITSUBA_PATH", None)
if not mitsuba_path: 
    mitsuba_path = '/home/symek/work/mitsuba/mitsuba-0.3.1/dist'
sys.path.append(os.path.join(mitsuba_path, "python"))

# Setup working dir:
mts_proxy_path = os.getenv("MITSUBA_TEMP", "/tmp")
mts_proxy_format = ".obj"
mts_proxy_files = {}
mts_materials = {}

# Mitsuba imports
#from mitsuba.core import *
#from mitsuba.render import Scene
#from mitsuba.render import RenderQueue, RenderJob

# Get a reference to the thread's file resolver
#fileResolver = Thread.getThread().getFileResolver()

# Add the search path needed to load plugins
#3fileResolver.addPath(mitsuba_path)

# Start scheduler:
#scheduler = Scheduler.getInstance()

#Cleanup
#scheduler.stop()
#if scheduler.getWorkerCount() != 0:
#    for i in range(0, multiprocessing.cpu_count()):
#        scheduler.unregisterWorker(LocalWorker('wrk%i' % i))

# We need plugin manager:
#pmgr = PluginManager.getInstance()

# Scene we will be adding objects to:
#scene = Scene()

########################################################
# Now Houdini stuff:

clockstart = time.clock()

# If you look at the parameters below, you'll see 'state:time'.  There
# are a set of special tokens which SOHO recognizes.  To get a list of
# these tokens, you can enable the following code.
#tokens = []
#soho.evalString('state:alltokens', tokens)
#print 'SOHO Tokens:', tokens
#soho.evalString('state:allgeotokens', tokens)
#print 'Geometry Tokens:', tokens

controlParameters = {
    # The time at which the scene is being rendered
    'now'     : SohoParm('state:time',  'real', [0], False,  key='now'),
    'fps'     : SohoParm('state:fps',   'real', [24], False, key='fps'),
    'camera'  : SohoParm('camera', 'string', ['/obj/cam1'], False),
    'picture' : SohoParm('vm_picture', 'string', ['ip'], False),
    'samples' : SohoParm('vm_samples', 'real', [2], False),
    'sampler' : SohoParm('vm_sampler', 'string', ['ldsampler'], False),
    'reflectlimit' : SohoParm('vm_reflectlimit', 'int', [1], False),
    'integrator' : SohoParm('vm_integrator', 'string', ['path'], False),
     'camera' : SohoParm('camera', 'string', ['/obj/cam1'], False),
     'skip_geo_export' : SohoParm('skip_geo_export', 'int', [0], False)}



# Evaluate the control parameters to determine settings for the render
parmlist = soho.evaluate(controlParameters)

# Extract the values from the evaluation
now     = parmlist['now'].Value[0]
camera  = parmlist['camera'].Value[0]
fps     = parmlist['fps'].Value[0]
picture = parmlist['vm_picture'].Value[0]
samples = parmlist['vm_samples'].Value[0]
sampler = parmlist['vm_sampler'].Value[0]
reflectlimit = parmlist['vm_reflectlimit'].Value[0]
integrator   = parmlist['vm_integrator'].Value[0]
camera       = parmlist['camera'].Value[0]
skip_geo_export = parmlist['skip_geo_export'].Value[0]


if not soho.initialize(now, camera):
    soho.error("Unable to initialize rendering module with given camera")

objectSelection = {
    # Candidate object selection
    'vobject'     : SohoParm('vobject', 'string',       ['*'], False),
    'alights'     : SohoParm('alights', 'string',       ['*'], False),
    'vfog'        : SohoParm('vfog',    'string',       ['*'], False),

    'forceobject' : SohoParm('forceobject',     'string',       [''], False),
    'forcelights' : SohoParm('forcelights',     'string',       [''], False),
    'forcefog'    : SohoParm('forcefog',        'string',       [''], False),

    'excludeobject' : SohoParm('excludeobject', 'string',       [''], False),
    'excludelights' : SohoParm('excludelights', 'string',       [''], False),
    'excludefog'    : SohoParm('excludefog',    'string',       [''], False),

    'sololight'     : SohoParm('sololight',     'string',       [''], False),
}

for cam in soho.objectList('objlist:camera'):
    break
else:
    soho.error("Unable to find viewing camera for render")
    
objparms = cam.evaluate(objectSelection, now)

stdobject = objparms['vobject'].Value[0]
stdlights = objparms['alights'].Value[0]
stdfog = objparms['vfog'].Value[0]
forceobject = objparms['forceobject'].Value[0]
forcelights = objparms['forcelights'].Value[0]
forcefog = objparms['forcefog'].Value[0]
excludeobject = objparms['excludeobject'].Value[0]
excludelights = objparms['excludelights'].Value[0]
excludefog = objparms['excludefog'].Value[0]
sololight = objparms['sololight'].Value[0]
forcelightsparm = 'forcelights'

if sololight:
    stdlights = excludelights = ''
    forcelights = sololight
    forcelightsparm = 'sololight'

# First, we add objects based on their display flags or dimmer values
soho.addObjects(now, stdobject, stdlights, stdfog, True,
    geo_parm='vobject', light_parm='alights', fog_parm='vfog')
soho.addObjects(now, forceobject, forcelights, forcefog, False,
    geo_parm='forceobject', light_parm=forcelightsparm, fog_parm='forcefog')
soho.removeObjects(now, excludeobject, excludelights, excludefog,
    geo_parm='excludeobject', light_parm='excludelights', fog_parm='excludefog')

# Lock off the objects we've selected
soho.lockObjects(now)

############################## Camara stuff ###################
# Parameters we evaluate on the camera to determine the viewing
# frustum
camParms = {
    'space:world':SohoParm('space:world',       'real', [], False),
    'focal'      :SohoParm('focal',             'real', [0.050], False),
    'aperture'   :SohoParm('aperture',          'real', [0.0414214], False),
    'orthowidth' :SohoParm('orthowidth',        'real', [2], False),
    'near'       :SohoParm('near',              'real', [0.001], False),
    'far'        :SohoParm('far',               'real', [1000], False),
    'res'        :SohoParm('res',               'int', [640,480], False),
}


#TODO: Remove to mitusba_api.py
def mts_matrix(hmat, asString=True):
    """This is probably wrong, as it can return 
    both hou.Matrix3 or string object"""
    rot = hou.hmath.buildRotate(0,0,180).transposed()
    scl = hou.hmath.buildScale(-1, -1, -1).transposed()
    hmat = hmat.transposed() * scl * rot
    if asString:
        hmat = [str(x) for x in hmat.asTuple()]
        hmat = " ".join(hmat)
    return hmat

 
################## Camera ######################################
# Camera details (transposed for mitsuba)
plist = cam.evaluate(camParms, now)
res   = plist['res'].Value
camera_xform = plist['space:world'].Value
camera_xform = hou.Matrix4(camera_xform)
camera_xform = mts_matrix(camera_xform)
# FOV (in radians):
fovx = 2*math.atan(( plist['aperture'].Value[0] / 2 ) / plist['focal'].Value[0])



################## Geometry stuff ###############################
def renderGeometry(obj, geo, now):
    splits = {}
    parts = geo.partition('geo:partlist', groupPrimitives(geo))
    if parts.has_key('ClosedPoly'):
        drawPolygons(parts['ClosedPoly'])
    if parts.has_key('OpenPoly'):
        drawLines(parts['OpenPoly'])
    if parts.has_key('NonPoly'):
        g = parts['NonPoly']
        tparms = obj.evaluate(tesselateParms, now)
        g = g.tesselate(
                { 'tess:style' : tparms['ogl_convert'].Value[0],
                  'tess:ulod'  : tparms['ogl_lod'].Value[0],
                  'tess:vlod'  : tparms['ogl_lod'].Value[1],
                  'tess:udiv'  : tparms['ogl_divs'].Value[0],
                  'tess:vdiv'  : tparms['ogl_divs'].Value[1]
                }
            )
        drawPolygons(g)

objectParms = {
    'space:world'       :SohoParm('space:world', 'real', [], False),
    'gl_diffuse'        :SohoParm('gl_diffuse',  'real', [.5,.5,.5], False),
    'gl_specular'       :SohoParm('gl_specular', 'real', [.5,.5,.5], False)
}


def saveGeometry(soppath, now, path):
    '''Construct a path and save geometry 
    in mts_proxy_format. Can skip export according 
    to skip_geometry_export flag.'''
    node = hou.node(soppath)
    path = os.path.join(path, soppath.replace("/","_") +"." +str(now) + mts_proxy_format)
    # warning: This disables export according to global flag:
    if not skip_geo_export:
        node.geometry().saveToFile(path)
    return path


def mts_objects(obj, now, mat):
    """Add objects to mts_proxy... dict:
    dict = {'/obj/geo1/facet1': 
    (string_matrix4x4, proxy_file_path), 
     ...}
    """
    soppath = []
    if not obj.evalString('object:soppath', now, soppath):
        return
    geo = SohoGeometry(soppath[0], now)
    if geo.Handle < 0:
        return       
    plist = obj.evaluate(objectParms, now)
    xform = plist['space:world'].Value
    
    if len(xform) != 16:
        return  
    xform = hou.Matrix4(xform)
    xform = mts_matrix(xform)
    mts_proxy_files[soppath[0]]  = (xform, saveGeometry(soppath[0], now, mts_proxy_path), mat)



#TODO: Remove to mitusba_api.py
def mts_parse_material(mixture):
    """Return bsdfs connected to mixture with its evaluated parms
    and compute weights for them"""
    bsdfs = {}
    # Evalute weights. Bsdfs take
    # always the same spot in 0-1 spectrum
    weights = range(len(mixture.inputs()))
    wparm = mixture.parm("weight").eval()
    for w in weights:
        weights[w] = wparm.lookup(1.0*w/len(mixture.inputs()))     
        
    for mat in mixture.inputs(): 
        bsdf = mat.type().name()[4:]
        bsdfs[mat.name()] = {'bsdf':bsdf}
        # Shorcut if there is no connections:
        if not mat.inputs():
            for parm in mat.parmTuples():
                name, typee = parm.name().split("_")
                bsdfs[mat.name()][name] = (typee, parm.eval())
        else:
            # Make inputs parms' long:
            inputs = list(mat.inputs())
            inputs += [None] * (len(mat.parmTuples()) - len(inputs))
            
            for n in range(len(mat.parmTuples())):
                parm = mat.parmTuples()[n]
                name, typee = parm.name().split("_")
                # if current parm doesn't have an input
                # just eval, else eval input of the same number:
                if not inputs[n]:
                    bsdfs[mat.name()][name] = (typee, parm.eval())
                else:
                    input = inputs[n]
                    if input.parm("parmname"):
                        pname  = input.parm("parmname").eval()
                        bsdfs[mat.name()][name] = \
                        (typee, mixture.parent().parmTuple(pname).eval())
                                     
    return weights, bsdfs
        
    
        
 
# Compose Mitsuba materials:
def find_mts_materials(path):
    """Parse material network for Mitsuba specific nodes"""
    node = hou.node(path)
    if node:
        matname = node.name()
        for child in node.children():
            if child.type().name() == "mts_mixture":
                node = child
                break
    if node: 
         weights, mix = mts_parse_material(node)
         return (matname, weights, mix)
    return None, None, None
        


# Proceed with objects
# 1) Find Object level materials
# 2) Parse materials
# 3) Save objects
for obj in soho.objectList('objlist:instance'):
    # Find shaders:
    material_path = []
    obj.evalString("shop_materialpath", now, material_path)
    # MaterialSHOP name, (w,w,w...), actual mixture dict
    matname, weights, mix = find_mts_materials(material_path[0])
    if matname:
        mts_materials[matname] = (weights, mix)
    # Save OBJ to mts_proxy_path
    mts_objects(obj, now, matname)


#print mts_proxy_files
#print mts_materials
#print mts_proxy_files
###################### XML GENERATION ######################
# Different approach, stright to xml:

# Header:
print '<?xml version="1.0" encoding="utf-8"?>'
print '<scene version="0.3.0">'

print """<!-- Houdini, version, hip name, user, data, Rop Name -->"""
# Integrator:
print """<!--Integrator -->"""
print """   <integrator type="%s">
        <integer name="maxDepth" value="%s"/>
    </integrator>""" % (integrator, int(samples))

# Camera, file, sampler: 
print """<!--Camera, sampler and file -->"""
print """   <camera type="perspective">
        <transform name="toWorld">
          <matrix value="%s"/>
        </transform>
        <float name="fov" value="%s"/>""" % (camera_xform, 
                                             hou.hmath.radToDeg(fovx))
print """   <sampler type="%s">
        <integer name="sampleCount" value="%s"/>
    </sampler>""" % (sampler, int(samples))
    
print """   <film type="exrfilm">
        <integer name="width" value="%s"/>
        <integer name="height" value="%s"/>
    </film>
    </camera>""" % (res[0], res[1])


# Materials:
print """<!-- Here comes materials -->"""
for mat in mts_materials:
    print """   <bsdf type="mixturebsdf" id="%s">""" % mat
    weights, mat = mts_materials[mat]
    weights = ", ".join([str(x) for x in weights])
    print """\t\t<string name="weights" value="%s" />""" % weights
    for bsdf in mat:
        print '\t\t<bsdf type="%s">' % mat[bsdf]['bsdf']
        del mat[bsdf]['bsdf']  
        for parm in mat[bsdf]:
            typee = mat[bsdf][parm][0]
            values = " ".join([str(x) for x in mat[bsdf][parm][1]])
            if typee == "boolean":
                if values == "0": values = "false"
                else: values = "true"
            print '\t\t\t<%s name="%s" value="%s" />' % (typee, parm, values)
        print '\t\t</bsdf>'
    print """</bsdf>"""
    
    
print """<!--Lights -->"""
# Lights:
print 
"""   <luminaire type="point" id="light0">
		<rgb name="intensity" value="10.000000, 10.000000, 10.000000"/>
		<transform name="toWorld">
			<translate x="5" y="5" z="0"/>
		</transform>
	</luminaire>"""

 
# Objects:
print """<!--Objects -->"""
for obj in mts_proxy_files:
    if mts_proxy_files[obj][2]:
        mat =  '<ref id="%s" />' % mts_proxy_files[obj][2]
    else:
        mat = '<bsdf type="diffuse"/>'
    print """   <shape type="obj">
        <string name="filename" value="%s"/>
        %s
        <transform name="toWorld">
            <matrix value="%s"/>
        </transform>
    </shape>""" % (mts_proxy_files[obj][1], mat,  mts_proxy_files[obj][0])
    

    
    
print '</scene>'






#command = os.path.join(mitsuba_path, "mitsuba")
#command += " -o /tmp/renderedResult2.exr /tmp/test.xml"
#result = os.popen(command)
#sys.stderr.write(result.read())
sys.exit()
















"""

# ######################## Mitsuba setup: #############################################
# Create a camera, film & sample generator
scene.addChild(pmgr.create({'type' : 'perspective', 
                            'toWorld' : "",
                            'fov'     : hou.hmath.radToDeg(fovx),
                            'film' : {'type' : 'exrfilm', 'width' : res[0], 'height' : res[1]},
                            'sampler' : { 'type' : sampler, 'sampleCount' : int(samples) } }))
                            
                            
# Set the integrator
scene.addChild(pmgr.create({ 'type' : integrator, 'maxDepth': reflectlimit }))

# Add a light source
scene.addChild(pmgr.create({'type' : 'point', 'position' : Point(5, 10, -10), 'intensity' : Spectrum(500)}))

# Add a shape
#scene.addChild(pmgr.create({'type' : 'sphere','center' : Point(-4, 0, 0),
#                            'radius' : 2.0,
#                             'bsdf' : { 'type' : 'diffuse', 'reflectance' : Spectrum(0.4)} }))

#mat = [{ 'type': 'diffuse', 'reflectance' : Spectrum(0.4)}, 
#       {'type' : 'roughplastic', 'reflectance' : Spectrum(0.4)}]


scene.addChild(pmgr.create({'id'   : 'redplastic', 
                            'type' : 'roughplastic',
                            'reflectance' : Spectrum(0.4)}))

counter = 0
s = Spectrum(0)
s.fromLinearRGB(0.9, 0.1, 0.1)

# Try interate over proxies:
for obj in mts_proxy_files.keys():
    matrix = float2Matrix(mts_proxy_files[obj][0])
    scene.addChild(pmgr.create({'type' : 'obj', 
                                'bsdf': {'type': 'roughplastic', 
                                         'reflectance': s},
                                'toWorld': Transform(matrix),
                                'filename': mts_proxy_files[obj][1]}))
    counter += 1

# End up with adding objects:                             
scene.configure()


# Start up the scheduling system with one worker per local core
if scheduler.getWorkerCount() == 0:
    for i in range(0, multiprocessing.cpu_count()):
        scheduler.registerWorker(LocalWorker('wrk%i' % i))

# Start the scheduler:    
scheduler.start()

# Create a queue for tracking render jobs
queue = RenderQueue()
scene.setDestinationFile(picture)

# Create a render job and insert it into the queue
job = RenderJob('myRenderJob', scene, queue)

# Start rendering:
job.start()
#job.stop()

# Wait for all jobs to finish and release resources
#queue.waitLeft(0)
#scheduler.stop()
queue.join()

# Print some statistics about the rendering process
print(Statistics.getInstance().getStats())
#scheduler.stop()
"""
