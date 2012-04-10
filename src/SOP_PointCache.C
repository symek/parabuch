/* 
 * This SOP reads Point cache pc2 format. 
 * It meant to do it faster than existing Python implementation.
 * 
 * skk. 
 * 
 * - H12 : 03-04-2012
 * - init: 25-11-2011
 *
 */

// HDK stuff:
#include <UT/UT_DSOVersion.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <SOP/SOP_Guide.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_Spline.h>
#include <UT/UT_Color.h>
#include <UT/UT_Thread.h>
#include <VM/VM_Math.h>

// Standards:
#include <stdio.h>

// TBB multithreading:
#include <GA/GA_SplittableRange.h>
#include <GA/GA_Range.h>
#include <GA/GA_PageIterator.h>

// Own stuff:
#include "SOP_PointCache.h"



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
    PRM_Name("filename",	   "PC2 File"),
    PRM_Name("interpol",       "Interpolation"),
    PRM_Name("computeNormals", "Compute Normals"),
    PRM_Name("pGroup",           "Primitive Group"),
    PRM_Name("flip",            "Flip Space"),
    PRM_Name("addrest",          "Add Rest"),
};


static PRM_Name         interpolChoices[] =
{
    PRM_Name("0", "None"),
    PRM_Name("1", "Linear"),
    PRM_Name("2", "Catmull-Rom"),
    PRM_Name(0)
};

static PRM_ChoiceList  interpolMenu(PRM_CHOICELIST_SINGLE, interpolChoices);

PRM_Template
SOP_PointCache::myTemplateList[] = {
    PRM_Template(PRM_STRING, 1, &PRMgroupName, 0, &SOP_Node::pointGroupMenu),
    PRM_Template(PRM_FILE,	 1, &names[0], PRMoneDefaults, 0, 0, SOP_PointCache::triggerReallocate),
    PRM_Template(PRM_ORD,    1, &names[1], 0, &interpolMenu, 0, SOP_PointCache::triggerReallocate),
    PRM_Template(PRM_TOGGLE, 1, &names[2]),
    PRM_Template(PRM_TOGGLE, 1, &names[4]),
    PRM_Template(PRM_TOGGLE, 1, &names[5]),
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
    pc2            = NULL;
    points         = NULL;
    dointerpolate  = 0;
    reallocate     = false;
}

SOP_PointCache::~SOP_PointCache() {delete points;}

OP_ERROR
SOP_PointCache::cookInputGroups(OP_Context &context, int alone)
{
    // The SOP_Node::cookInputPointGroups() provides a good default
    // implementation for just handling a point selection.
    return cookInputPointGroups(context, myGroup, myDetailGroupPair, alone);
}


// Read pc2 header storing it in PC2_File->header structure.
// Also validates point cache file.
int
PC2_File::loadFile(UT_String *file)
{
    int success;
    filename->harden(file->buffer());
	FILE * fin = fopen(file->buffer(), "rb");
	if (fin == NULL)  return 0;
	success = fread(buffer, 32, 1, fin);
	fclose(fin);
	if (!success) return 0;
	header = (PC2Header*) buffer;

	if ((UT_String("POINTCACHE2").hash() == UT_String(header->magic).hash()) && \
    (header->version == 1))
	{
	    loaded = 1;
        return 1;
	}
	return 0;
}


/* Read into (float)*pr of size 3*sizeof(float)*numpoints*steps 
// position from a opened file in (float) time, along with supersampaled (int) steps. 
*/
int
PC2_File::getPArray(int sample, int steps, float *pr)
{
    int success;
	FILE * fin = fopen(filename->buffer(), "rb");
	if (fin == NULL)  return 0;

	long offset = 32 + (3*sizeof(float)*header->numPoints) * sample;
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
    UT_String interpol_str;
    //int       interpol;
    int       flip;
    int       addrest;
    int       computeNormals;
    UT_Spline *spline = NULL;
    /// Info buffers
    char      info_buff[200];
    const char *info;

    /// Make time depend:
    OP_Node::flags().timeDep = 1;
    
    /// Before we do anything, we must lock our inputs. 
    if (lockInputs(context) >= UT_ERROR_ABORT)
        return error();
   
    /// Duplicate our incoming geometry 
    duplicatePointSource(0, context);
    /// Eval parms. 
    /// FIXME: interpol!!! (str or int menu?)
    t              = context.getTime();
    flip           = FLIP(t);
    addrest        = ADDREST(t);
    dointerpolate  = INTERPOL(interpol_str, t);
    computeNormals = COMPUTENORMALS(t);
    FILENAME(filename, t);

    /// Get frame. FIXME: This should be probably provided by an user.
    float frame = (float) context.getFloatFrame();
    
    /// Create 'rest' attribute if requested:
    if (addrest)
    {
        GA_RWAttributeRef    restattr = gdp->addRestAttribute(GEO_POINT_DICT);
        GA_ROPageHandleV3    Phandle(gdp->getP());
        GA_RWPageHandleV3    Rhandle(gdp, GEO_POINT_DICT, "rest");
        UT_Vector3           Pvalue; 
        
        if (Phandle.isValid() && Rhandle.isValid())
        {
            GA_Offset start, end;
            for (GA_Iterator it(gdp->getPointRange()); it.blockAdvance(start, end);)
            {
                Phandle.setPage(start);
                Rhandle.setPage(start);
                for (GA_Offset pt = start; pt < end; ++pt)
                {
                    Rhandle.value(pt) = Phandle.get(pt);
                }
            }
        }  
    }
    
    /// Create pc2 file object only once:
    if (!pc2) 
    {
        pc2 = new PC2_File(&filename);
        if (!pc2->loadFile(&filename))
        {  
            addWarning(SOP_ERR_FILEGEO, "Can't open this file.");
            return error();
         }
    }
    
    /// Reload header if filename changed from a last time
    /// FIXME: We should also check if inputGeo has changed.
    
    if (pc2->getFilename()->hash() != filename.hash())
    {
        reallocate = true;
        if (!pc2->loadFile(&filename))
        {
            addWarning(SOP_ERR_FILEGEO, "Can't open this file.");
            return error();
        }  
    }
    
	/// Lets give the user some information:
	sprintf(info_buff, "File   : %s \nPoints : %d \nStart  : %f \nRate   : %f \nSamples: %d\nFrames: %f",
	            filename.buffer(),  pc2->header->numPoints, pc2->header->startFrame, 
	            pc2->header->sampleRate, pc2->header->numSamples, 
	            pc2->header->sampleRate * pc2->header->numSamples );                  
	/// Is it ugly?
	info = &info_buff[0];
	addMessage(SOP_MESSAGE, info);
	
	/// Allocte points' position array, then reallocate in case file changed
    /// Also adjust time steps, i.e.: one step for no interpolation, two for linear,
    /// three (starting backwards + current + next) for cubic. I should use here BRI
    /// from Timeblender project here, instead of cutmull-rom.
    int steps = dointerpolate + 1;
    if (!points || reallocate)
    {
        if (reallocate)
            delete points;
        points     = new float[3*pc2->header->numPoints*steps];
        reallocate = false;

        if (!points)
        {
            addWarning(SOP_MESSAGE, "Can't allocate storage.");
            return error();
        }   
    }
	
	/// Abandom if frame exceeds samples range.
	/// In case of supersampling we switch limit to take
	/// sampling rate into account. 
	int frame_limit = (!dointerpolate) ? pc2->header->numSamples : \
	(pc2->header->numSamples * pc2->header->sampleRate);
	if ((frame > frame_limit) || (frame < pc2->header->startFrame))
	{
	    addWarning(SOP_MESSAGE, "Frame exceeds samples range from file.");
        return error();
	}
	
	/// Supersampling / Interpolation:
	/// In case of cubic, we shift sample -1, and take 3 steps,
	/// None and linear require 2 steps and sample == current == $F-1.
	/// Delta will be used to drive interpolants (0,1). 
	float sample    = (!dointerpolate) ? (frame-1) : (frame-1) * (1.0/pc2->header->sampleRate);
	fpreal32 delta  = sample - SYSfloor(sample);
	sample          = SYSfloor(sample);
	
	/// We also create a single spline object.
	if (dointerpolate == PC2_CUBIC)
	{
	    sample -= 1; 
	    sample = SYSclamp(sample, 0.0f, 1.0*pc2->header->numSamples);
	    spline = new UT_Spline(); 
        spline->setGlobalBasis(UT_SPLINE_CATMULL_ROM);
        spline->setSize(3, 3);
        /// Delta needs to be remapped:
        delta = SYSfit(delta, 0.0f, 1.0f, 0.5f, 1.0f);
	}
	
	/// Don't deceive an user if no supersamples found in a file:
	if (dointerpolate && pc2->header->sampleRate==1.0)
	    addWarning(SOP_MESSAGE, "Sample rate is 1.0, excpect poor interpolation!");
	    
	/// Having interpolation turned off and samples rate < 1.0 doesn't play nice eihter:
	if (!dointerpolate && pc2->header->sampleRate<1.0)
	    addWarning(SOP_MESSAGE, "No interpolation selected. Supersampling will cause animation longer (frames == samples)");
	
    /// Load array at 'sample' point 'steps' wide, to 'points[]' array 
    if (!pc2->getPArray((int) sample, steps, points))
    {
        addWarning(SOP_MESSAGE, "Can't load points[] from file.");
        return error();
    }

    /////////////////////////////////////////////////////////////////////////
    //UNDER CONSTRUCTION  DOESNT WORK!! BELLOW CODE IS BORKEN as of 9. April 2012
    /////////////////////////////////////////////////////////////////////////


    /// Here we determine which groups we have to work on.  We only
    ///	handle point groups.
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
       
        int numPoints = pc2->header->numPoints;
        UT_Vector3 p;

        if (dointerpolate == PC2_NONE)
        {
            int ptnum     = 0;
            GA_FOR_ALL_OPT_GROUP_POINTS(gdp, myGroup, ppt)
	        {
	            //UT_Vector3 p;
	            p = ppt->getPos3();
                p.x() = points[3*ptnum];
                p.y() = points[3*ptnum+1];
                p.z() = points[3*ptnum+2];
                ppt->setPos3(p);
                ptnum++;
                ptnum = SYSclamp(ptnum, 0, numPoints-1);
            }
        }
        else if (dointerpolate == PC2_LINEAR)
        {

            // Sends gdp, points, and delta to working threads:            
            const GA_Range      range(*myGroup);    
            threaded_pc2Lerp(range, gdp, delta, points, numPoints);
            
        }
        else if (0!=0) //(dointerpolate == PC2_CUBIC)
        {}
            /*
            fpreal32 pp[] = {0.0f, 0.0f, 0.0f};
            fpreal32 v0[] = {points[3*ptnum], 
                             points[3*ptnum+1], 
                             points[3*ptnum+2]};
            fpreal32 v1[] = {points[3*(ptnum + numPoints)], 
                             points[3*(ptnum + numPoints)+1], 
                             points[3*(ptnum + numPoints)+2]};
            fpreal32 v2[] = {points[3*(ptnum + numPoints*2)], 
                             points[3*(ptnum + numPoints*2)+1], 
                             points[3*(ptnum + numPoints*2)+2]};
            
            spline->setValue(0, v0, 3); 
            spline->setValue(1, v1, 3); 
            spline->setValue(2, v2, 3);
            
            /// Finally eval. spline and assign result to vector
            spline->evaluate(delta, pp, 3, (UT_ColorType)2);
            p.assign(pp[0], pp[1], pp[2]);
           
        }

        /// Flip space for Max compatibile: x, z,-y.
        if (flip)
        {
           
            float z;
            z     = p.z();
            p.z() = -p.y();
            p.y() = z;
        }
        
        ppt->setPos3(p);
        ptnum++;
         */
        /// We really shouldn't go over this boundary...
        //ptnum = SYSclamp(ptnum, 0, numPoints-1);
	   
    }
    
    ///Update normals:
    if (computeNormals) GA_RWAttributeRef ref = gdp->normal();
    /// Notify the display cache that we have directly edited
    gdp->notifyCache(GU_CACHE_ALL);
    if (spline)  delete spline;

    unlockInputs();
    return error();
}



