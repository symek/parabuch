// HDK:
#include <CMD/CMD_Args.h>
#include <IMG/IMG_DeepShadow.h>
#include <GU/GU_Detail.h>
#include <UT/UT_BoundingBox.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_PointGrid.h>
#include <GU/GU_Detail.h>

// Own:
#include "pointgrid.h"

static void
usage(const char *me)
{
    cerr << "Usage: " << me << " -[r res] sourcefile.bgeo destfile.rat\n";
    cerr << "Converts a geometry file (only points) into deep camera map." << endl;
    cerr << "DCM can be open in VEX in tiled fashon, making heavy point clouds" << endl;
    cerr << "fast on load, and efficient for RAM." << endl;
    cerr << "\t-r 256 - Resolution of a grid (cubical) anything between 16 and 512 should work." << endl;
    cerr << "\t         Check out for an avarage points per voxel occupation in output." << endl;
    cerr << "\t-v     - Verbose output." << endl;
}

int 
main(int argc, char *argv[])
{
    
    // Init:   
    CMD_Args args;
    args.initialize(argc, argv);
    args.stripOptions("r:v");
    if (args.argc() < 3)
    {
        usage(argv[0]);
        return 1;
    }

    // Options:
    int res     = 256;
    int verbose = 0;
    if(args.found('r')) res     = atoi(args.argp('r'));       
    if(args.found('v')) verbose = 1;
    UT_String dcm_file, gdp_file;
    gdp_file.harden(argv[argc-2]);
    dcm_file.harden(argv[argc-1]);
    
    #if 1
    // Open GDP with samples:
    GU_Detail gdp;
    UT_BoundingBox bbox;
    if (!gdp.load(gdp_file, 0).success())
    {
        cerr << "Cant open " << gdp_file << endl;
        return 1;
    }
   
    // Points arrays and bbox details: 
    gdp.getBBox(&bbox);
    int range = gdp.getNumPoints();  
    UT_Vector3Array         positions(range);
    UT_ValArray<int>        indices(range);
    
    const GEO_Point *ppt;
    const GEO_PointList plist = gdp.points();
    for (int i = 0; i < gdp.getNumPoints(); i++)
    {
        ppt = plist(i);
        UT_Vector3 pos = ppt->getPos3();
        positions.append(pos);
        indices.append(i);
    }

    if (verbose)
        cout << "Points in gdp      : " <<  positions.entries() << endl;

    // Point Grid structures/objects:
    UT_Vector3Point accessor(positions, indices);
    UT_PointGrid<UT_Vector3Point> pointgrid(accessor);

    // Can we build it?
    if (!pointgrid.canBuild(res, res, res))
    {
        cout << "Can't build the grid!" << endl; 
        return 1;
    }
    // Build it:
    pointgrid.build(bbox.minvec(), bbox.size(), res, res, res);

    if (verbose)
    {
        cout << "Point grid res     : " << res << "x" << res << "x" << res << endl;
        cout << "Bounding box size  : " << bbox.size().x() << ", " << bbox.size().y() << ", " << bbox.size().z() << endl;
        cout << "Bounding box center: " << bbox.center().x() << ", " << bbox.center().y() << ", " << bbox.center().z() << endl;
        cout << "Pointgrid mem size : " << pointgrid.getMemoryUsage() << endl;
        cout << "Voxel size is      : " << pointgrid.getVoxelSize() << endl;
        cout << "Total grid points  : " << pointgrid.entries() << endl;
    }
    #endif

    // Open rat (our random access, variable array length storage):
    IMG_DeepShadow dsm;
    dsm.setOption("compression", "0");
    dsm.setOption("zbias", "0.05");
    dsm.setOption("depth_mode", "nearest");
    dsm.setOption("depth_interp", "discrete");
    dsm.open(dcm_file, res*res, res);
    dsm.getTBFOptions()->setOptionV3("bbox:min" , bbox.minvec());
    dsm.getTBFOptions()->setOptionV3("bbox:max" , bbox.maxvec()); 
    
    if (verbose)
        cout << "DCM created res    : " << res*res << "x" << res << endl;
      
    #if 1
    // db_* debug variables...
    int db_index = 0;
    int db_uindex = 0;
    int db_av_iter = 0;

    // Put point into deep pixels:
    Locker locker;
    Timer timer;
    timer.start();
    parallel_fillDCM(res, &dsm, &pointgrid, &positions, &locker);
    cout <<     "Creation time      : " << timer.current() << endl;
    if (verbose)
    {
        cout << "Total voxels       : " << db_index  << endl;
        cout << "Written voxel      : " << db_uindex << endl;
        cout << "Points per voxel   : " << (float)db_av_iter / db_uindex << endl;
    }
    timer.start();
    dsm.close();
    cout <<     "Saving time        : " << timer.current() << endl;
    if (verbose)
        cout << "Deep map closed." << endl;
    return 0;
    #endif
}

