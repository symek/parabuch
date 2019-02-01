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
#include <OP/OP_AutoLockInputs.h>
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
    PRM_Name("filename",	   "PC2 File"),
    PRM_Name("interpol",       "Interpolation"),
    PRM_Name("computeNormals", "Compute Normals"),
    PRM_Name("pGroup",         "Primitive Group"),
    PRM_Name("flip",           "Flip Space"),
    PRM_Name("addrest",        "Add Rest"),
    PRM_Name("relative",       "Relative Offset"),
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
    PRM_Template(PRM_TOGGLE, 1, &names[6]),
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
    points_zero    = NULL;
    dointerpolate  = 0;
    reallocate     = false;
}

SOP_PointCache::~SOP_PointCache() {/*delete points;*/}

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
    double		 t;
    UT_String filename;
    UT_String interpol_str;
    //int       interpol;
    int       flip;
    int       addrest;
    int       computeNormals;
    int       relativeOffset;
    // UT_Spline *spline = NULL;
    /// Info buffers
    char      info_buff[200];
    const char *info;
    /// This indicates 
    int       firstRead = 0;

    /// Make time depend:
    OP_Node::flags().timeDep = 1;
    
    /// Before we do anything, we must lock our inputs. 
    OP_AutoLockInputs inputs(this);
    if (inputs.lock(context) >= UT_ERROR_ABORT)
        return error();
   
    FILENAME(filename, t);

    if (filename.length() == 0) {  
        addWarning(SOP_ERR_FILEGEO, "");
        return error();
     }

    /// Duplicate our incoming geometry 
    duplicatePointSource(0, context);
    /// Eval parms. 
    /// FIXME: interpol!!! (str or int menu?)
    t              = context.getTime();
    flip           = FLIP(t);
    addrest        = ADDREST(t);
    dointerpolate  = INTERPOL(interpol_str, t);
    computeNormals = COMPUTENORMALS(t);
    relativeOffset = RELATIVE(t);

    /// Get frame. FIXME: This should be probably provided by an user.
    float frame = (float) context.getFloatFrame();
    
    /// Create 'rest' attribute if requested:
    if (addrest)
    {
        GA_Attribute * rest = gdp->addRestAttribute(GA_ATTRIB_POINT);
        const GA_Attribute * pos = gdp->getP();
        rest->replace(*pos);
    }
    
    /// Create pc2 file object only once:
    if (!pc2) 
    {
        pc2.reset(new PC2_File(&filename));
        firstRead = 1;
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
        firstRead = 1;  
    }
    
	/// Lets give the user some information:
	sprintf(info_buff, "File   : %s \nPoints : %d \nStart  : %f \nRate   : %f \nSamples: %d\nFrames: %f",
	            filename.buffer(),  pc2->header->numPoints, pc2->header->startFrame, 
	            pc2->header->sampleRate, pc2->header->numSamples, 
	            pc2->header->sampleRate * pc2->header->numSamples );                  
	/// Is it ugly?
	info = &info_buff[0];
	addMessage(SOP_MESSAGE, info);

    /// Force linear interpolation if supersampling is present in a file. 
    /// This was requested by TDs to aviod accidental 'None' value for supersampled
    /// animations causing wrong animation length.
    if (dointerpolate == 0 && pc2->header->sampleRate < 1.0f && firstRead)
    {
        dointerpolate = 1;
        UT_String linear = UT_String("1");
        SETINTERPOL(linear, (float)t);
    }
	
	/// Allocte points' position array, then reallocate in case file changed
    /// Also adjust time steps, i.e.: one step for no interpolation, two for linear,
    /// three (starting backwards + current + next) for cubic. I should use here BRI
    /// from Timeblender project here, instead of cutmull-rom.
    int steps = dointerpolate + 1;
    if (!points || reallocate)
    {
        if (reallocate) {
            points.reset(new float[3*pc2->header->numPoints*steps]);
            reallocate = false;
        }

        if (!points)
        {
            addWarning(SOP_MESSAGE, "Can't allocate storage.");
            return error();
        }   
    }

    /// Additional array for relative offset pc2s:
    if ((!points_zero && relativeOffset) || reallocate)
    {
        if (reallocate) {
            points_zero.reset(new float[3*pc2->header->numPoints]);
        }

        if (!points_zero)
        {
            addWarning(SOP_MESSAGE, "Can't allocate storage for relative offset.");
            return error();
        } 
    }

	/// Abandom if frame exceeds samples range.
	/// In case of supersampling we switch limit to take
	/// sampling rate into account. 
	float frame_limit = (!dointerpolate) ? (pc2->header->numSamples + pc2->header->startFrame) : \
	((pc2->header->numSamples * pc2->header->sampleRate) + pc2->header->startFrame);

	if ((frame < pc2->header->startFrame) || (frame > frame_limit))
	{
        addWarning(SOP_MESSAGE, "Frame exceeds samples range from file.");
        return error();
	}

    // FIXME? Should we take into account start frame shift:
    frame -= pc2->header->startFrame;

	/// Supersampling / Interpolation:
	/// In case of cubic, we shift sample -1, and take 3 steps,
	/// None and linear require 2 steps and sample == current == $F-1.
	/// Delta will be used to drive interpolants (0,1). 
	float sample    = (!dointerpolate) ? frame : frame * (1.0/pc2->header->sampleRate);
	fpreal32 delta  = sample - SYSfloor(sample);
	sample          = SYSfloor(sample);
	
	/// We also create a single spline object.
	if (dointerpolate == PC2_CUBIC)
	{
	    sample -= 1; 
	    sample = SYSclamp(sample, 0.0f, 1.0*pc2->header->numSamples);
	    spline.reset(new UT_Spline()); 
        spline->setGlobalBasis(UT_SPLINE_CATMULL_ROM);
        spline->setSize(3, 3);
        /// Delta needs to be remapped:
        delta = SYSfit(delta, 0.0f, 1.0f, 0.5f, 1.0f);
	}
	
	/// Don't deceive an user if no supersamples found in a file:
	if (dointerpolate && pc2->header->sampleRate==1.0)
	    addWarning(SOP_MESSAGE, "Sample rate is 1.0, excpect poor interpolation.");
	    
	/// Having interpolation turned off while samples rates < 1.0 doesn't play nice eihter:
	if (!dointerpolate && pc2->header->sampleRate<1.0)
	    addWarning(SOP_MESSAGE, "No interpolation selected. Supersampling will cause animation longer (frames == samples)");
	
    /// Load array at 'sample' point 'steps' wide, to 'points[]' array 
    if (!pc2->getPArray((int) sample, steps, points.get()))
    {
        addWarning(SOP_MESSAGE, "Can't load points[] from file.");
        return error();
    }

    //FIXME: This is temporary
    if (relativeOffset)
    {
        if (!pc2->getPArray((int) 0, 1, points_zero.get()))
         {
            addWarning(SOP_MESSAGE, "Can't load zero frame position from file.");
            return error();
        }
    }

    /// Here we determine which groups we have to work on.  We only
    ///	handle point groups.
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
        int ptnum     = 0;
        int numPoints = pc2->header->numPoints;
        GA_Offset ptoff;
        GA_FOR_ALL_GROUP_PTOFF(gdp, myGroup, ptoff)
        // GA_FOR_ALL_OPT_GROUP_POINTS(gdp, myGroup, ppt)
	    {
            const float* rawpoints = points.get();
            const float* rawpoints_zero = points_zero.get();
	        UT_Vector3 p = gdp->getPos3(ptoff);
	        if (dointerpolate == PC2_NONE)
	        {
                //FIXME: This is complete workaround/proof of concept
                // I have to rethink whole loop.
                if (relativeOffset)
                {
                    p.x() -= rawpoints_zero[3*ptnum];
                    p.y() -= rawpoints_zero[3*ptnum+2];
                    p.z() -= -rawpoints_zero[3*ptnum+1];

                    p.x() += rawpoints[3*ptnum];
                    p.y() += rawpoints[3*ptnum+2];
                    p.z() += -rawpoints[3*ptnum+1];
                }
                else
                {
                    p.x() = rawpoints[3*ptnum];
                    p.y() = rawpoints[3*ptnum+1];
                    p.z() = rawpoints[3*ptnum+2];
                }
                
            }
            else if (dointerpolate == PC2_LINEAR)
            {
                /// Array is flat and continous:, 
                /// ax,ay,az,bx,by,bz, then next sample: ax,ay,az,bx...
                p.x() = SYSlerp(rawpoints[3*ptnum  ], rawpoints[3*(ptnum + numPoints)],   delta);
                p.y() = SYSlerp(rawpoints[3*ptnum+1], rawpoints[3*(ptnum + numPoints)+1], delta);
                p.z() = SYSlerp(rawpoints[3*ptnum+2], rawpoints[3*(ptnum + numPoints)+2], delta);
            
            }
            else if (dointerpolate == PC2_CUBIC)
            {
                fpreal32 pp[] = {0.0f, 0.0f, 0.0f};
                fpreal32 v0[] = {rawpoints[3*ptnum], 
                                 rawpoints[3*ptnum+1], 
                                 rawpoints[3*ptnum+2]};
                fpreal32 v1[] = {rawpoints[3*(ptnum + numPoints)], 
                                 rawpoints[3*(ptnum + numPoints)+1], 
                                 rawpoints[3*(ptnum + numPoints)+2]};
                fpreal32 v2[] = {rawpoints[3*(ptnum + numPoints*2)], 
                                 rawpoints[3*(ptnum + numPoints*2)+1], 
                                 rawpoints[3*(ptnum + numPoints*2)+2]};
                
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
            
            gdp->setPos3(ptoff, p);
            ptnum++;
            
            /// We really shouldn't go over this boundary...
            ptnum = SYSclamp(ptnum, 0, numPoints-1);
	    }
	   
    }
    
    ///Update normals:
    if (computeNormals)
        GA_RWAttributeRef ref = gdp->normal();
    
    /// Notify the display cache that we have directly edited
     if (!myGroup || !myGroup->isEmpty())
        gdp->getP()->bumpDataId();

  
    // unlockInputs();
    return error();
}



