// Irrlicht:
#include <irrlicht.h>

// STD:
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>
#include <math.h>

// Almost own..., 
// but I'm steeling pystring's functions:
#include "picojson.h"
#include "OGL_preview.h"

using namespace irr;
using namespace core;
using namespace scene;
using namespace video;
using namespace io;
using namespace gui;


#ifdef _IRR_WINDOWS_
#pragma comment(lib, "Irrlicht.lib")
#pragma comment(linker, "/subsystem:windows /ENTRY:mainCRTStartup")
#endif



// Basic class to handle camera animation storage
// TODO: Build in json support, extend, switch to std::vectors<> etc
class JSONCamera
{
public:
JSONCamera(int size)
{
    tx = new float[size];
    ty = new float[size];
    tz = new float[size];
    rx = new float[size];
    ry = new float[size];
    rz = new float[size];
 focal = new float[size];
}
~JSONCamera()
{ 
    delete tx; delete ty; delete tz;
    delete rx; delete ry; delete rz;
    delete focal;
}
    float *tx;
    float *ty;
    float *tz;
    float *rx;
    float *ry;
    float *rz;
    float *focal;
};



enum
{
	ID_IsNotPickable = 0,
	IDFlag_IsPickable = 1 << 0,
	IDFlag_IsHighlightable = 1 << 1
};

//http://julien.boucaron.free.fr/wordpress/?p=269
struct IrrlichtDelayFPS {
  int maxFPS;
  int lastFPS;
  int yieldCounter;
  IrrlichtDevice *device;
  IrrlichtDelayFPS() { maxFPS = 50;  yieldCounter = 0;  device = 0; }
  void delay() {
    video::IVideoDriver* driver = device->getVideoDriver();
    int lastFPS = driver->getFPS();
    for( int i = 0 ; i < yieldCounter ; i++ ) {
      device->yield();
    }
    if ( lastFPS >maxFPS ) { yieldCounter++ ; }
    else {
      if (yieldCounter > 0 ) { --yieldCounter; }
      else { yieldCounter = 0 ; }
    }
  }
};


/*
IAnimatedMesh* 
create_mesh(std::string &mesh_file, ISceneManager *smgr, )
{
    IAnimatedMesh* mesh;
    mesh = smgr->getMesh(mesh_file);
	if (mesh)
    	IAnimatedMeshSceneNode* node = smgr->addAnimatedMeshSceneNode(mesh);
}
*/

// JSON support:
inline void copy_json_array(picojson::object::const_iterator &item, float *channel)
{
    // Copy picojson::array into float[] TODO: Replace to std::vector<float>
    int idx = 0;
    const picojson::array& a = (item->second).get<picojson::array>();    
    for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i)
    {
	    channel[idx] = i->get<double>();
        idx++;
    }
}

bool parse_camera_file(const char *filename, JSONCamera &camera, int verbose=0)
{
    // Read in Json object:
    picojson::value json_object;
    std::ifstream json_file;
    json_file.open(filename);
    json_file >> json_object;
    json_file.close();
    // Print content:
    if (verbose)
        std::cout << json_object << std::endl;

    // Parse and load in arrays into JSONCamera:
    if (json_object.is<picojson::object>()) 
    {   
	    const picojson::object& o = json_object.get<picojson::object>();
	    for (picojson::object::const_iterator i = o.begin(); i != o.end(); ++i)
        {
                 if (i->first == std::string("tx"))
                copy_json_array(i, camera.tx);
            else if (i->first == std::string("ty"))
                copy_json_array(i, camera.ty);
            else if (i->first == std::string("tz"))
                copy_json_array(i, camera.tz);
            else if (i->first == std::string("rx"))
                copy_json_array(i, camera.rx);
            else if (i->first == std::string("ry"))
                copy_json_array(i, camera.ry);
            else if (i->first == std::string("rz"))
                copy_json_array(i, camera.rz);
            else if (i->first == std::string("focal"))
                copy_json_array(i, camera.focal);
	    }
        return true;
    }
    else
     return false;
}


int main(int argc, char *argv[])
{

    // Variables:
    std::string buffer;
    char *tmp;
    const std::string seperator(",");         
    std::vector<std::string> geos;
    std::vector<std::string> pc2s;
    const char       *camera_file;

    // Parsing Command line argumentes:
    // OBJs:
    tmp = get_argument(argv, argv+argc, "-g");
    if (tmp)
    {
        buffer.assign(tmp);
        split(buffer, geos, seperator, 1000);
    }
    // Pc2s:
    tmp = get_argument(argv, argv+argc, "-p");
    if (tmp)
    {
        buffer.assign(tmp);
        split(buffer, pc2s, seperator, 1000);
    }
    // Camera
    camera_file = get_argument(argv, argv+argc, "-c");
    if (!camera_file)
    {
        std::cout << "No camera specified." << std::endl;
    }

    // Read camera from json file:
    JSONCamera json_camera(200);
    if (!parse_camera_file(camera_file, json_camera))
        std::cout << "Can't read camera" << std::endl;
    

    // Create a device (OpenGL or Software in fall-back case):
    IrrlichtDevice *device = NULL;
    device = createDevice( video::EDT_OPENGL, dimension2d<u32>(1280, 720), 16, false, false, false, 0);
	if (!device)
        device = createDevice( video::EDT_SOFTWARE, dimension2d<u32>(1280, 720), 16, false, false, false, 0);
    if (!device)
		return 1;


    // Driver, scene manager, guienv:
    IVideoDriver* driver    = device->getVideoDriver();
	ISceneManager* smgr     = device->getSceneManager();
	IGUIEnvironment* guienv = device->getGUIEnvironment();
    const u32 max_prims     = driver -> getMaximalPrimitiveCount();


     // Create meshes:
    IAnimatedMeshSceneNode* node;
    std::vector<std::string>::iterator itr;
    for (itr = geos.begin(); itr != geos.end(); ++itr)
    {
        // Load in obj from file:
        SAnimatedMesh* mesh;    
        mesh = (SAnimatedMesh*)smgr->getMesh((*itr).data());
	    if (mesh)
        {   
            // Get to te mesh buffer, create and append new ones:
            std::cout << "Meshes:" << mesh->Meshes.size() << std::endl;
            SMesh *geo_frame = 0;
            SMeshBuffer *geo_buff = 0;
            geo_frame = (SMesh*) mesh->Meshes[0];
            std::cout << "Buffers in geo_frame: " << geo_frame->getMeshBufferCount()  << std::endl;
            //geo_frame->getMeshBufferCount()
            for (int b = 0; b < 1; b++)
            {
                geo_buff  = (SMeshBuffer*) geo_frame->MeshBuffers[2];            
                //std::cout << "Vertices in geo_buffer: "<< geo_buff->getVertexCount() << std::endl;
                for (int i = 0; i < 1; i++)
                {
                    SMeshBuffer  *new_buff = new SMeshBuffer();
                    new_buff->Vertices.set_used(geo_buff->getVertexCount());
                    // Set position per vertex:
                    for (int j = 0; j < geo_buff->getVertexCount(); j++)
                    {
                        vector3df pos = geo_buff->getPosition(j);
                        pos = pos + (pos/10.0);
                        std::cout << "Vertex: " << j << ", index: " << geo_buff->getIndices()[j] << ": " << pos.X << ", " << pos.Y << ", " << pos.Z << ", " << std::endl;
                        S3DVertex& v  = new_buff->Vertices[j];
                        v.Pos.set(pos);
                    }
                    //geo_frame->MeshBuffers.push_back(new_buff);
                   
                }
            }
            mesh->recalculateBoundingBox();
            mesh->setDirty();
            std::cout << "Bufers count:" << mesh->getMeshBufferCount() << std::endl;
        	node = smgr->addAnimatedMeshSceneNode(mesh);
        }
    }


    // Add camera:
    ICameraSceneNode *camera = smgr->addCameraSceneNodeMaya(node, -100, 200, 100, -1, true);
    //ICameraSceneNode *camera = smgr->addCameraSceneNodeFPS(0, 1, 1);
    //ICameraSceneNode *camera = smgr->addCameraSceneNode(0, vector3df(0,0,0), vector3df(0,0,0), true);
    camera->bindTargetAndRotation(true);
    //camera->setNearValue(0.1f);
    //camera->setNearValue(1000.0f);
    //camera->setInputReceiverEnabled(0);
    //camera->setPosition(core::vector3df(json_camera.tz[0], json_camera.ty[0], json_camera.tx[0]));
     // camera->setPosition(core::vector3df(,2,123));
    //camera->setRotation(core::vector3df(json_camera.rx[0], json_camera.ry[0], json_camera.rz[0]));
    float fov = 2 * atan( (24.0/2.0) / 40.0 );
    camera->setFOV(fov);
    //smgr->setActiveCamera(camera);


    std::cout << "Translate:";
    std::cout << json_camera.tx[0] << ", " << json_camera.ty[0] << ", " << json_camera.tz[0] << std::endl;
    std::cout << "Rotation:";
    std::cout << json_camera.rx[0] << ", " << json_camera.ry[0] << ", " << json_camera.rz[0] << std::endl;

    // Lights:     
	smgr->addLightSceneNode(camera, core::vector3df(300,100,-300), video::SColorf(1.0f,1.0f,1.0f),300);
	smgr->setAmbientLight(video::SColorf(0.3f,0.3f,0.3f));

  
    // Capition:
    //const char *title = sprintf("Animation preview. Maximum primitives: %i", max_prims);
    device->setWindowCaption(L"Animation preview.");
    // On screen massage:
    guienv->addStaticText(L"This is hardware render_preview test", rect<s32>(10,10,260,44), true);

    IrrlichtDelayFPS delayFPS;
    delayFPS.maxFPS = 25; //set max FPS
    delayFPS.device = device; //set device

    // Main rendering loop:
    while(device->run())
	{
        // SColor(255,100,101,140)
		driver->beginScene(true, true, SColor(110,110,110,110));
		smgr->drawAll();
		guienv->drawAll();
		driver->endScene();
        delayFPS.delay();
	}

    // Ending up:
	device->drop();
	return 0;


}



