#include <VEX/VEX_VexOp.h>
#include <UT/UT_Vector3.h>
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Thread.h>
#include <UT/UT_Hash.h>
#include <UT/UT_Lock.h>
#include <UT/UT_VectorTypes.h>
#include <IMG/IMG_DeepShadow.h>
#include <UT/UT_Options.h>
#include <map>
#include "VEX_Pointgrid.h"


using namespace std;

// Static objects:
static Locker locker;
const  char*  filename = "/home/symek/work/pointgrid/tmp/1.rat";
static IMG_DeepShadow dsm;
static IMG_DeepShadowChannel *Of;
static UT_Vector3 bboxmin;
static UT_Vector3 bboxmax;
static int        yres;


static void
pg_completed(void *data)
{
    static int i;
    i++;
}

// Stupid, but I can't find other way of doiong this:
static void 
setupCompletedCallback(void *f)
{
    for (int i=0; i < VEX_VexOp::getNumVexOps(); i++)
    {
        if (VEX_VexOp::getVexOp(i)->getInit() == f)
        {
            VEX_VexOp *me = (VEX_VexOp*) VEX_VexOp::getVexOp(i);
            me->setCompleted(pg_completed);
        }
    }
}

// Init deep shadow object:
static void *
pg_init()
{ 
    locker.lock();
    locker.advance();
    if(locker.get() == 1)
    {
        setupCompletedCallback((void*)pg_init);
        if(dsm.open(filename))
        {
            int xres;
            bboxmax = dsm.getTBFOptions()->getOptionV3("bbox:max");
            bboxmin = dsm.getTBFOptions()->getOptionV3("bbox:min");
            dsm.resolution(xres, yres);
            // Opacity is always second channel(?):
            Of = (IMG_DeepShadowChannel*) dsm.getChannel(1);
        }
        else
        {
            locker.unlock();
            return NULL;
        }
    }
    IMG_DeepPixelReader *pixel = new IMG_DeepPixelReader(dsm);
    locker.pixels[UT_Thread::getMySequentialThreadIndex()] = pixel;
    locker.unlock();
    return pixel;
}

static void
pg_cleanup(void *data)
{
    IMG_DeepPixelReader *pixel = (IMG_DeepPixelReader *) data;
    if (pixel)
        delete pixel;
    cout << "Leaving." << endl; 
}

static void
pointgrid(int, void *argv[], void *data)
{
    UT_Vector3  *result        = (UT_Vector3 *) argv[0];
    UT_Vector3  *pos           = (UT_Vector3 *) argv[1];
    IMG_DeepPixelReader *pixel = (IMG_DeepPixelReader *) data;
    
    if (pixel)
    {
        // Object space pos to volume normalized position:
        UT_Vector3 dsm_pos = (*pos - bboxmin) / (bboxmax - bboxmin);
        // Normalized position to 3d indices:
        dsm_pos *= yres;
        int z = (int)SYSfloor(dsm_pos.z());
        int x = (int)SYSfloor(dsm_pos.x());
        int y = (int)SYSfloor(dsm_pos.y());        
        // x,y,z voxel indices into x,y pixel indices:
        z *= yres;
        x += z;
        pixel->open(x, y);
        //cout << *pos << ": ";
        UT_Vector3 out(0,0,0);
        for (int i = 0; i < pixel->getDepth(); i++)
        {
            UT_Vector3 point(pixel->getData(*Of, i));
            //cout << point << ", ";
            point -= *pos;
            float sigma = SYSabs(point.length());
            if ( sigma <= 0.01)
            //if (*pos == point)
                out = UT_Vector3(1,1,1);
        }
        result[0] = out;
        pixel->close();
        //cout << endl;
    }
}


void
newVEXOp(void *)
{
      new VEX_VexOp("pointgrid@&VV", 
            pointgrid, 
			VEX_ALL_CONTEXT, 
			pg_init,
			pg_cleanup, 
            VEX_OPTIMIZE_0,
            true);
}
