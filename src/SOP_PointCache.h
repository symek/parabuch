#ifndef __SOP_PointCache_h__
#define __SOP_PointCache_h__

#include <SOP/SOP_Node.h>

namespace PC2SOP {

/// PC2 format header:
/// http://www.footools.com/plugins/docs/PointCache2_README.html#pc2format
struct PC2Header
{
	char magic[12];
	int version;
	int numPoints;
	float startFrame;
	float sampleRate;
	int numSamples;
};

/// Interpolation types
enum PC2_Interpol_Type
{
    PC2_NONE,
    PC2_LINEAR,
    PC2_CUBIC,
};

/// Flip vectors to match 3DSMax coordinate system.
inline void flip_space(UT_Vector3 &p)
{
    float z;
    z     = p.z();
    p.z() = -p.y();
    p.y() = z;      
}

/// PC2 handler class
class PC2_File
{
public: 
    PC2_File();
    PC2_File(UT_String *file, int load = 0)
    {
        filename = new UT_String(file->buffer());
        filename->harden();        
        if (file && load)
        {
            int success = 0;
            success = loadFile(file); 
            loaded = success; 
         }
    }
    ~PC2_File();
    int loadFile(UT_String *file);
    int loadFile() { return loadFile(filename);}
    int isLoaded() { return loaded;}
    int setUnload() {loaded = 0; return 1;} 
    int getPArray(int sample, int steps, float *pr);
    UT_String * getFilename() {return filename;}
    PC2Header *header;
    
private:
    const char *buffer[32];
    int loaded;
    UT_String  *filename;
};

/// Main class it was modeled after SOP_PointWave example from HDK.
class SOP_PointCache : public SOP_Node
{
public:
    // Usual stuff:
	SOP_PointCache(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_PointCache();
    static  PRM_Template myTemplateList[];
    static  OP_Node		 *myConstructor(OP_Network*, const char *, OP_Operator *);
    virtual OP_ERROR     cookInputGroups(OP_Context &context, int alone = 0);

    // This triggers reallocation of points[] array only for cases filename has changed.
    static  int          triggerReallocate(void *data, int, fpreal, const PRM_Template *)
    {  
        SOP_PointCache *node = (SOP_PointCache*) data;
        node->reallocate = true;
        return 1;
    };

protected:
    // Method to cook geometry for the SOP
    virtual OP_ERROR		 cookMySop(OP_Context &context);

private:
    // Parameters evaluation:
    void	getGroups(UT_String &str){ evalString(str, "group", 0, 0); }
    void    PGROUP(UT_String &str)   { evalString(str, "pGroup", 0, 0); }
    void    FILENAME(UT_String &str, float t){ return evalString(str, "filename", 0, t);}
    int     COMPUTENORMALS(float t)  { return evalInt("computeNormals", 0, t);}
    int     FLIP(float t)            { return evalInt("flip", 0, t);}
    int     ADDREST(float t)         { return evalInt("addrest", 0, t);}
    
    /// I don't know how to make int based list...
    int INTERPOL(UT_String &str, float t)
        {   
            evalString(str, "interpol", 0, t);
            return SYSatoi(str.buffer());
        }

    /// This variable is used together with the call to the "checkInputChanged"
    /// routine to notify the handles (if any) if the input has changed.
    GU_DetailGroupPair	 myDetailGroupPair;

    /// This is the group of geometry to be manipulated by this SOP and cooked
    /// by the method "cookInputGroups".
    const GA_PointGroup	*myGroup;
    float               *points;
    int                 dointerpolate;
    bool                reallocate;
    PC2_File            *pc2;
};

/// Threaded worker class for linear interpolation.
class op_InterpolateLinear {
public:
    op_InterpolateLinear(GU_Detail *gdp, float delta, float *points, int numPoints): 
    mygdp(gdp), mydelta(delta), mypoints(points), mynumPoints(numPoints) {};
    // Take a SplittableRange (not a GA_Range)
    void operator()(const GA_SplittableRange &range) const
    {
        GA_RWPageHandleV3   handleP(mygdp->getP());
        // Iterate over pages in the range
        for (GA_PageIterator pit = range.beginPages(); !pit.atEnd(); ++pit)
        {
            GA_Offset start, end;
            handleP.setPage(*pit);
       
            // iterate over the elements in the page.
            for (GA_Iterator it(pit.begin()); it.blockAdvance(start, end); )
            {
                #if 1
                // SIMD based linear interpolation.
                // FIXME: Implement flip:
                VM_Math::lerp((fpreal32 *)&handleP.value(start), &mypoints[start*3], 
                            &mypoints[(start+mynumPoints)*3], (fpreal32)mydelta, 3*(end - start));
                #else
                // Standard loop:
                for (GA_Offset i = start; i < end; ++i)
                {
                    UT_Vector3 p;
                    p.x() = SYSlerp(mypoints[3*i  ], mypoints[3*(i + mynumPoints)],   mydelta);
                    p.y() = SYSlerp(mypoints[3*i+1], mypoints[3*(i + mynumPoints)+1], mydelta);
                    p.z() = SYSlerp(mypoints[3*i+2], mypoints[3*(i + mynumPoints)+2], mydelta);
                    // FIXME: Make conditional flip.
                    PC2SOP::flip_space(p)
                    handleP.set(i, p);
                }
                #endif
            }
        }
    }

private:
    GU_Detail *mygdp;
    float     mydelta;
    float     *mypoints;
    int       mynumPoints;

};

void
threaded_simd_lerp(const GA_Range &range, GU_Detail *gdp, float delta, 
                    float *points, int numPoints)
{
    // Create a GA_SplittableRange from the original range
    GA_SplittableRange split_range = GA_SplittableRange(range);
    UTparallelFor(split_range, op_InterpolateLinear(gdp, delta, points, numPoints));
}



/// Threaded worker class for cubic interpolation.
class op_InterpolateCubic {
public:
    op_InterpolateCubic(GU_Detail *gdp, float delta, float *points, int numPoints): 
    mygdp(gdp), mydelta(delta), mypoints(points), mynumPoints(numPoints) {};
    // Take a SplittableRange (not a GA_Range)
    void operator()(const GA_SplittableRange &range) const
    {
        GA_RWPageHandleV3   handleP(mygdp->getP());
        // Iterate over pages in the range
        for (GA_PageIterator pit = range.beginPages(); !pit.atEnd(); ++pit)
        {
            GA_Offset start, end;
            handleP.setPage(*pit);
            UT_Spline *spline  = new UT_Spline();
            spline->setGlobalBasis(UT_SPLINE_CATMULL_ROM);
            spline->setSize(3, 3); 
            // iterate over the elements in the page.
            for (GA_Iterator it(pit.begin()); it.blockAdvance(start, end); )
            {
                #if 0
                // TODO: SIMD based cubic interpolation.
                // FIXME: Implement flip:
                VM_Math::lerp((fpreal32 *)&handleP.value(start), &mypoints[start*3], 
                            &mypoints[(start+mynumPoints)*3], (fpreal32)mydelta, 3*(end - start));
                #else
                // Standard loop:
               
                UT_Vector3 p;
                int n = mynumPoints;
                fpreal32 pp[] = {0.0f, 0.0f, 0.0f};
                for (GA_Offset i = start; i < end; ++i)
                {    
                    fpreal32 v0[] = {mypoints[3*i], mypoints[3*i+1], mypoints[3*i+2]};
                    fpreal32 v1[] = {mypoints[3*(i + n)], mypoints[3*(i + n)+1], mypoints[3*(i + n)+2]};
                    fpreal32 v2[] = {mypoints[3*(i + n*2)], mypoints[3*(i + n*2)+1], mypoints[3*(i + n*2)+2]};
                    spline->setValue(0, v0, 3); 
                    spline->setValue(1, v1, 3); 
                    spline->setValue(2, v2, 3);
                    /// Finally eval. spline and assign result to vector
                    spline->evaluate(mydelta, pp, 3, (UT_ColorType)2);
                    p.assign(pp[0], pp[1], pp[2]);
                    PC2SOP::flip_space(p);
                    handleP.set(i, p);
                }
                #endif
            }
            delete spline;
        }
    }

private:
    GU_Detail *mygdp;
    float     mydelta;
    float     *mypoints;
    int       mynumPoints;

};

void
threaded_simd_cubic(const GA_Range &range, GU_Detail *gdp, float delta, 
                    float *points, int numPoints)
{
    // Create a GA_SplittableRange from the original range
    GA_SplittableRange split_range = GA_SplittableRange(range);
    UTparallelFor(split_range, op_InterpolateCubic(gdp, delta, points, numPoints));
}


} // End PC2SOP namespace

#endif
