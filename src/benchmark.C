#include <UT/UT_DSOVersion.h>
#include <GU/GU_Detail.h>
#include <SYS/SYS_Math.h>
#include <UT/UT_Spline.h>
#include <UT/UT_Color.h>
#include <UT/UT_Thread.h>
#include <VM/VM_Math.h>
#include <UT/UT_ValArray.h>


#include <GA/GA_SplittableRange.h>
#include <GA/GA_Range.h>
#include <GA/GA_PageIterator.h>

// Own stuff:
//#include "SOP_PointCache.h"

#include <time.h>


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

PC2_File::~PC2_File(){}

/* Read into (float)*pr of size 3*sizeof(float)*numpoints*steps 
// position from a opened file in (float) time, along with supersampaled (int) steps. 

   The way to check if fread reads correct number of samples before 
   reaching the end of the file could be like this:
   
        result = fread (buffer,1,lSize,pFile);
        if (result != lSize) 
            return 1; // error occured. 
*/
int
PC2_File::getPArray(int sample, int steps, float *pr)
{
    size_t result;
    size_t size = steps*header->numPoints*3;

	FILE * fin = fopen(filename->buffer(), "rb");
	if (fin == NULL) return 0;

	long offset = 32 + (3*sizeof(float)*header->numPoints) * sample;
    fseek(fin, offset, SEEK_SET);
    result = fread(pr, sizeof(float), size, fin);
    fclose(fin);

    if (result != size ) 
        return 0;
    return 1;
}


/// Threaded worker class for linear interpolation.
class op_InterpolateLinear {
public:
    op_InterpolateLinear(GU_Detail *gdp, float delta, float *points, int numPoints, bool simd): 
    mygdp(gdp), mydelta(delta), mypoints(points), mynumPoints(numPoints), mysimd(simd) {};
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
                if (mysimd)
                {
                    // SIMD based linear interpolation.
                    // FIXME: Implement flip:
                    VM_Math::lerp((fpreal32 *)&handleP.value(start), &mypoints[start*3], 
                                &mypoints[(start+mynumPoints)*3], (fpreal32)mydelta, 3*(end - start));
                }
                else
                {
                    // Standard loop:
                    for (GA_Offset i = start; i < end; ++i)
                    {
                        UT_Vector3 p;
                        p.x() = SYSlerp(mypoints[3*i  ], mypoints[3*(i + mynumPoints)],   mydelta);
                        p.y() = SYSlerp(mypoints[3*i+1], mypoints[3*(i + mynumPoints)+1], mydelta);
                        p.z() = SYSlerp(mypoints[3*i+2], mypoints[3*(i + mynumPoints)+2], mydelta);
                        // FIXME: Make conditional flip.
                        //PC2SOP::flip_space(p)
                        handleP.set(i, p);
                    }
                }
            }
        }
    }

private:
    GU_Detail *mygdp;
    float     mydelta;
    float     *mypoints;
    int       mynumPoints;
    bool      mysimd;

};

void
threaded_simd_linear(const GA_Range &range, GU_Detail *gdp, float delta, 
                    float *points, int numPoints, bool simd)
{
    // Create a GA_SplittableRange from the original range
    GA_SplittableRange split_range = GA_SplittableRange(range);
    UTparallelFor(split_range, op_InterpolateLinear(gdp, delta, points, numPoints, simd));
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
                    //flip_space(p);
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

double diffclock(clock_t clock1,clock_t clock2)
{
    double diffticks=clock1-clock2;
    double diffms=(diffticks)/(CLOCKS_PER_SEC/1000);
    return diffms;
}




int
main(int argc, char *argv[])
{	
 	// No options? Exit:  
	if (argc < 3)
	{
        cout << "Usage: ./benchmark geometry.bgeo cache.pc2" << endl;
		return 0;
	}

    
    // global settings.
    int   steps  = 5;
    float sample = 1;
    float delta  = 0;
    bool  flip   = false;


    // Open BGEO:
    GU_Detail gdp;
    GA_Detail::IOStatus status;
    status = gdp.load(argv[1]);
    clock_t start = clock();
    if (!status.success())
    {
        cout << "Can't open file" << endl;
        return 0;
    }
    clock_t end = clock();
    cout << "Opening bgeo: " << diffclock(end, start) << endl;



    // Open PC2:
    float *points;
    UT_String filename = UT_String(argv[2]);
    //start = clock();
    PC2_File *pc2 = new PC2_File(&filename);
    if (!pc2->loadFile(&filename))
    {
        cout << "Can't open pc2" << endl;
        return 0;
    }
    end  = clock();
    cout << "Opening pc2: " << diffclock(end, start) << endl;


    // Loading array:
    //start = clock();
    points= new float[3*pc2->header->numPoints*steps];
    if(!pc2->getPArray(sample, steps, points))
    {
        cout << "Can't load array" << endl;
        return 0;
    }
    end  = clock();
    cout << "Loading pc2: " <<diffclock(end, start)<< endl;

    /////// Benchmark:
    //No interpolation:
    int numPoints = pc2->header->numPoints;
    int frames    = pc2->header->numSamples;

 
    #if 1
    //start = clock();
    for (int i = 0; i < frames; i++)
    {
        int ptnum = 0;
        GEO_Point *ppt;
        GA_FOR_ALL_GPOINTS(&gdp, ppt)
        {
            UT_Vector3 p;
            p = ppt->getPos3();
            p.x() = points[3*ptnum];
            p.y() = points[3*ptnum+1];
            p.z() = points[3*ptnum+2];
            //if (flip) 
            //    flip_space(p);
            ppt->setPos3(p);
            ptnum++;
            ptnum = SYSclamp(ptnum, 0, numPoints-1);
        }

    }
    end  = clock();
    cout << "No interpolation: " << diffclock(end, start)  << endl;
    #endif


    #if 1
    /// Linear single thread:
    //start  = clock();
    //sample = sample -1;
    for (int i = 0; i < frames; i++)
    {
        GEO_Point *ppt;
        int ptnum = 0;
        UT_Vector3 p;
        GA_FOR_ALL_GPOINTS(&gdp, ppt)
        {
            delta =0.5;
            p.x() = SYSlerp(points[3*ptnum  ], points[3*(ptnum + numPoints)],   delta);
            p.y() = SYSlerp(points[3*ptnum+1], points[3*(ptnum + numPoints)+1], delta);
            p.z() = SYSlerp(points[3*ptnum+2], points[3*(ptnum + numPoints)+2], delta);
            //if (flip) 
            //    flip_space(p);	        
            ppt->setPos3(p);
            ptnum++;
            ptnum = SYSclamp(ptnum, 0, numPoints-1);
        }
    }
    end  = clock();
    cout << "Linear single thread: " <<diffclock(end, start) << endl;
    #endif

   
    /// Linear multithead:    
    {
        const GA_Range range(gdp.getPointRange());   
        //start  = clock();  
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_linear(range, &gdp, delta, points, numPoints, false);
        }
        end  = clock();
        cout << "Linear mulithread: " << diffclock(end, start)  << endl;
    }

     /// Linear multithead:    
    {
        const GA_Range range(gdp.getPointRange());
        //start  = clock();     
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_linear(range, &gdp, delta, points, numPoints, true);
        }
        end  = clock();
        cout << "Linear MT SIMD: " << diffclock(end, start)  << endl;
    }

    /// Cubic singlethread:
    {
        int ptnum = 0;
        UT_Vector3 p;
         GEO_Point *ppt;
        UT_Spline *spline = new UT_Spline();
        spline->setGlobalBasis(UT_SPLINE_CATMULL_ROM);
        spline->setSize(3, 3);
        //start  = clock(); 
        for (int i = 0; i < frames; i++)
        {   
            
            GA_FOR_ALL_GPOINTS(&gdp, ppt)
            {
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
                ppt->setPos3(p);
                ptnum++;
                ptnum = SYSclamp(ptnum, 0, numPoints-1);
            }
        }
         end  = clock();
         cout << "Cubic single thread: " << diffclock(end, start)  << endl;
    }


    


      /// Cubic multithead:    
    {
        const GA_Range range(gdp.getPointRange());
        //start  = clock();     
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_cubic(range, &gdp, delta, points, numPoints);
        }
        end  = clock();
        cout << "Cubic multithread: " << diffclock(end, start)  << endl;
    }


    


    delete points;
    delete pc2;
    return 0;
}








