// Irrlicht:
#include <irrlicht.h>

// STD:
#include <algorithm>
#include <iostream>
#include <string>
#include <vector>
#include <fstream>

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

typedef std::vector<std::vector<float> > cameraParms;


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
    int idx = 0;
    const picojson::array& a = (item->second).get<picojson::array>();    
    for (picojson::array::const_iterator i = a.begin(); i != a.end(); ++i)
    {
	    channel[idx] = i->get<double>();
        idx++;
    }
}

bool parse_camera_file(std::string camera_file, Camera &camera)
{
    picojson::value json;
    std::ifstream file_json;
    file_json.open(camera_file.data());
    file_json >> json;
    file_json.close();
    //std::cout << json << std::endl;

    if (json.is<picojson::object>()) 
    {   
	    const picojson::object& o = json.get<picojson::object>();
	    for (picojson::object::const_iterator i = o.begin(); i != o.end(); ++i)
        {
                 if (i->first == std::string("tx"))
                copy_json_array(i, camera.tx);
            else if (i->first == std::string("ty"))
                copy_json_array(i, camera[1]);
            else if (i->first == std::string("tz"))
                copy_json_array(i, camera.ty]);
            else if (i->first == std::string("rx"))
                copy_json_array(i, camera.tz);
            else if (i->first == std::string("ry"))
                copy_json_array(i, camera.rx);
            else if (i->first == std::string("rz"))
                copy_json_array(i, camera.ry);
            else if (i->first == std::string("focal"))
                copy_json_array(i, camera.rz);
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
    std::string       camera_file;

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
    tmp = get_argument(argv, argv+argc, "-c");
    if (tmp)
    {
        camera_file.assign(tmp);
    }

    // Read camera from json file:
    JSONCamera json_camera(200);
    if (!parse_camera_file(camera_file, json_camera))
        std::cout << "Can't read camera" << std::endl;
    else
        for (int i = 0; i < 20; i++)
        {
            std::cout << json_camera.tx[i] << ", ";
            std::cout << json_camera.ty[i] << ", ";
            std::cout << json_camera.tz[i] << ", ";
        }
    return 0;

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
    std::vector<std::string>::iterator itr;
    for (itr = geos.begin(); itr != geos.end(); ++itr)
    {
        IAnimatedMesh* mesh;
        mesh = smgr->getMesh((*itr).data());
	    if (mesh)
        	IAnimatedMeshSceneNode* node = smgr->addAnimatedMeshSceneNode(mesh);
    }


    // Capition:
    //const char *title = sprintf("Animation preview. Maximum primitives: %i", max_prims);
    device->setWindowCaption(L"Animation preview.");

    // On screen massage:
    guienv->addStaticText(L"This is hardware render_preview test", rect<s32>(10,10,260,22), true);


    // Main rendering loop:
    while(device->run())
	{
        // SColor(255,100,101,140)
		driver->beginScene(true, true, SColor(11,11,11,11));
		smgr->drawAll();
		guienv->drawAll();
		driver->endScene();
	}

    // Ending up:
	device->drop();
	return 0;


}



