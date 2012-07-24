#include <IMG/IMG_DeepShadow.h>
#include <UT/UT_DMatrix4.h>
#include <UT/UT_DMatrix3.h>
#include <UT/UT_Assert.h>
#include <UT/UT_WorkArgs.h>
#include <UT/UT_String.h>
#include <GU/GU_Detail.h>
#include <UT/UT_BoundingBox.h>

#include <SYS/SYS_Math.h>
#include <UT/UT_PointGrid.h>
#include <GU/GU_Detail.h>
#include <UT/UT_Filter.h>
#include <IMG/IMG_File.h>
#include <TIL/TIL_TextureMap.h>
#include <TIL/TIL_Raster.h>
#include <TIL/TIL_Defines.h>
#include <IMG/IMG_FileTTMap.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_LatinSampler.h>
#include <UT/UT_MTwister.h>


int
main()
{
    //int size  = 1;
    int res   = 256;
    const char *dsm_file    = "./../tmp/1.rat";
    const char *geo_file    = "./../tmp/1.bgeo";

    #if 1
    // Open GDP with samples:
    GU_Detail gdp;
    UT_BoundingBox bbox;
    if (!gdp.load(geo_file, 0).success())
    {
        cout << "Cant open " << geo_file << endl;
        return 1;
    }
    
    int range =  gdp.getNumPoints();
    // Points:
    UT_Vector3Array         positions(range);
    UT_ValArray<int>        indexes(range);
    
    const GEO_Point *ppt;
    const GEO_PointList plist = gdp.points();
    for (int i = 0; i < gdp.getNumPoints(); i++)
    {
        ppt = plist(i);
        UT_Vector3 pos = ppt->getPos3();
        positions.append(pos);
        indexes.append(i);
    }

    // Verbose:
    cout << "Positions: " <<  positions.entries() << endl;

    // Point Grid structures/objects:
    typedef  UT_PointGridVector3ArrayAccessor<int, int> UT_Vector3Point;
    typedef  UT_PointGrid<UT_Vector3Point>::queuetype UT_Vector3PointQueue;
    UT_Vector3Point accessor(positions, indexes);
    UT_PointGrid<UT_Vector3Point> pointgrid(accessor);

    // Build a grid
    if (!pointgrid.canBuild(res, res, res))
    {
        cout << "Can't build the grid!" << endl; 
        return 1;
    }
    cout << "Gridpoint resoltion: " << res << "x" << res << "x" << res << endl;
    //const UT_Vector3 bounds(size, size, size);
    gdp.getBBox(&bbox);
    cout << "BBox size  : " << bbox.size().x() << ", " << bbox.size().y() << ", " << bbox.size().z() << endl;
    cout << "BBox center: " << bbox.center().x() << ", " << bbox.center().y() << ", " << bbox.center().z() << endl;
    pointgrid.build(bbox.minvec(), bbox.size(), res, res, res);

    // Grid tests:
    cout << "Pointgrid mem size: "  << pointgrid.getMemoryUsage() << endl;
    cout << "Voxel size is: "       << pointgrid.getVoxelSize() << endl;
    cout << "Voxel 0 position is: " << accessor.getPos(0) << endl;
    cout << "Total points in grid: "<< pointgrid.entries() << endl;
    int ix, iy, iz;
    UT_Vector3 p(bbox.minvec());
    pointgrid.posToIndex(p, ix, iy, iz);
    cout << "At position (" << p << ")  index is: " << ix << ", " << iy << ", " << iz << endl;
    #endif

    // Open rat (our random access, variable array length storage):
    IMG_DeepShadow dsm;
    dsm.setOption("compression", "0");
    dsm.setOption("zbias", "0.05");
    dsm.setOption("depth_mode", "nearest");
    dsm.setOption("depth_interp", "discrete");

    dsm.open(dsm_file, res*res, res);
    // Store bounding box for computing position->voxel:
    dsm.getTBFOptions()->setOptionV3("bbox:min" , bbox.minvec());
    dsm.getTBFOptions()->setOptionV3("bbox:max" , bbox.maxvec()); 
    
    cout << "Deep map opened with res: " << res*res << "x" << res << endl;


    
      
    #if 1
    // Query the grid toolset:
    UT_Vector3PointQueue *queue;
    queue = pointgrid.createQueue();
    UT_PointGridIterator<UT_Vector3Point> iter;

    // db_* debug variables...
    int db_index = 0;
    int db_uindex = 0;
    int db_av_iter = 0;

    // Put point into deep pixels:
    for (int z = 0; z < res; z++)
    {
        for (int y = 0; y < res; y++)
        {
            for (int x = 0; x < res; x++)
            {
                iter = pointgrid.getKeysAt(x, y, z, *queue);
                if (iter.entries() != 0)
                {
                    db_av_iter += iter.entries();
                    int _z = z*res;
                    dsm.pixelStart(_z + x, y);
                    float zdepth = 0;
                    for (;!iter.atEnd(); iter.advance())
                    {
                        int idx = iter.getValue();                        
                        UT_Vector3 pos = positions(idx);
                        float posf[3]; posf[0] = pos.x(); 
                        posf[1] = pos.y(); posf[2] = pos.z();
                        dsm.pixelWriteOrdered(0.1*zdepth, posf, 3);
                        zdepth++;
                    }
                    dsm.pixelClose();
                    db_uindex++;
                }
                db_index++;               
            }        
        }
    }

    cout << "Iterated over: " << db_index << " voxels.  Should be: " << res*res*res << endl;
    cout << "From which " << db_uindex << " were writen to." << endl;
    cout << "Avarage occupation: " << (float)db_av_iter / db_uindex << " per used voxel." << endl;
    dsm.close();
    cout << "Deep map closed." << endl;
    return 0;
    #endif
}

