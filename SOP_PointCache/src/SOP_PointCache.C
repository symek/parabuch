/* 
 * This SOP reads Point cache pc2 format. 
 * It meant to do it faster than existing Python implementation.
 * 
 * skk. 
 * - 
 * - init: 25-11-2011
 *
 */

#include <UT/UT_DSOVersion.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <SOP/SOP_Guide.h>
#include "SOP_PointCache.h"

#include <stdio.h>


using namespace PC2SOP;
using namespace std;

void
newSopOperator(OP_OperatorTable *table)
{
     table->addOperator(new OP_Operator("PointCache",
					"Point Cache",
					 SOP_PointCache::myConstructor,
					 SOP_PointCache::myTemplateList,
					 1,
					 1,
					 0));
}

static PRM_Name names[] = {
    PRM_Name("filename",	"Filename"),
};

PRM_Template
SOP_PointCache::myTemplateList[] = {
    PRM_Template(PRM_STRING,    1, &PRMgroupName, 0, &SOP_Node::pointGroupMenu),
    PRM_Template(PRM_FILE,	1, &names[0], PRMoneDefaults, 0),
    PRM_Template(),
};


OP_Node *
SOP_PointCache::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new SOP_PointCache(net, name, op);
}

SOP_PointCache::SOP_PointCache(OP_Network *net, const char *name, OP_Operator *op)
	: SOP_Node(net, name, op), myGroup(0)
{
    pc2 = NULL;
    points = NULL;
}

SOP_PointCache::~SOP_PointCache() {delete points;}

OP_ERROR
SOP_PointCache::cookInputGroups(OP_Context &context, int alone)
{
    // The SOP_Node::cookInputPointGroups() provides a good default
    // implementation for just handling a point selection.
    return cookInputPointGroups(context, myGroup, myDetailGroupPair, alone);
}


/// Read pc2 header storing it in PC2_File->header structure.
/// Also validates point cache file.
int
PC2_File::loadFile(UT_String *file)
{
    int success;
    filename->harden(file->buffer());
	FILE * fin = fopen(file->buffer(), "rb");
	if (fin == NULL) 
	    return 0;
	success = fread(buffer, 32, 1, fin);
	if (!success) 
	    return 0;
	header = (PC2Header*) buffer;
	if (UT_String("POINTCACHE2").hash() == file->hash() && header->version == 1)
	    return 1;
	
	fclose(fin);
	loaded = 1;
	return 1;
}


/// Read into (float)*pr of size 3*sizeof(float)*numpoints*steps 
/// position from a opened file in (float) time, along with supersampaled (int) steps. 
int
PC2_File::getPArray(float time, int steps, float *pr)
{
    int success;
	FILE * fin = fopen(filename->buffer(), "rb");
	if (fin == NULL) 
	    return 0;
	long offset = 32 + (3*sizeof(float)*header->numPoints) * time;
    fseek(fin, offset, SEEK_SET);
    success = fread(pr, sizeof(float), steps*header->numPoints*3, fin);
    fclose(fin);
    if (!success) return 0;
    return 1;
}


OP_ERROR
SOP_PointCache::cookMySop(OP_Context &context)
{
    GEO_Point *ppt;
    double		 t;
 
    UT_String filename;
    /// Info buffers
    char info_buff[200];
    const char *info;
    /// Flag to indicate new point
    /// array needs to be allocated
    bool reallocate = false;
    
    /// This is placeholder for time-steps variable
    int steps = 1;

    /// Make time depend:
    OP_Node::flags().timeDep = 1;
    
    /// Before we do anything, we must lock our inputs. 
    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();
   
    /// Duplicate our incoming geometry 
    duplicatePointSource(0, context);
    t = context.getTime();
    FILENAME(filename, t);
    
    /// FIXME: Currently using int frame instead of float frame:
    float frame = (float) context.getFrame() - 1.0;
    
    /// Create pc2 file object only once:
    if (!pc2) 
    {
        pc2 = new PC2_File(&filename);
        if (!pc2->loadFile(&filename))
        {  
            addWarning(SOP_ERR_FILEGEO, "Can't open file.");
            return error();
         }
    }
    
    /// Reload header if filename changed from a last time
    if (pc2->getFilename()->hash() != filename.hash())
    {
        reallocate = true;
        if (!pc2->loadFile(&filename))
        {
            addWarning(SOP_ERR_FILEGEO, "Can't open file.");
            return error();
        }  
    }
   
	/// Lets give an user some information:
	sprintf(info_buff,"File   : %s \nPoints : %d \nStart  : %f \nRate   : %f \nSamples: %d", 
	            filename.buffer(),  pc2->header->numPoints, pc2->header->startFrame, 
	            pc2->header->sampleRate, pc2->header->numSamples);
	                       
	/// Is it ugly?
	info = &info_buff[0];
	addMessage(SOP_MESSAGE, info);
	
	/// Allocte points' position array, then...
    /// (reallocate in case file changed)
    if (!points || reallocate)
    {
        if (reallocate) 
            delete points;
        points = new float[3*pc2->header->numPoints*steps];
    }
	
	/// ...abandom if frame exceeds samples range or...
	if ((frame > pc2->header->numSamples) || (frame < pc2->header->startFrame))
	{
	    addWarning(SOP_MESSAGE, "Frame exceeds samples' range in file.");
        return error();
	}
	
    ///... get array at current 'time', 'steps' wide to 'points' array 
    if (!pc2->getPArray(frame, steps, points))
    {
        addWarning(SOP_MESSAGE, "Can't load points data.");
        return error();
    }
    
    /// Here we determine which groups we have to work on.  We only
    ///	handle point groups.
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
        int ptnum = 0;
        FOR_ALL_OPT_GROUP_POINTS(gdp, myGroup, ppt)
	    {
	        UT_Vector4 p;
	        p = ppt->getPos();
            p.x() = points[3*ptnum];
            p.y() = points[3*ptnum+1];
            p.z() = points[3*ptnum+2];
            ppt->getPos() = p;
            ptnum++;
	    }
    }

    // Notify the display cache that we have directly edited
    gdp->notifyCache(GU_CACHE_ALL);
    unlockInputs();
    return error();
}



