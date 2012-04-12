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


class timer {
	private:
		double begTime;
	public:
		void start() {
			begTime = clock();
		}

		double current() {
            //int threads = UT_Thread::getNumProcessors();
			return (difftime(clock(), begTime) / CLOCKS_PER_SEC);// / (double) threads;
		}

		bool isTimeout(double seconds) {
			return seconds >= current();
		}
};


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
        //cout <<  "Thread: " << UT_Thread::getMySequentialThreadIndex() << endl;
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


// Home brew cubic interpolation: 
inline void 
cubicInterpolate(fpreal32 &re, fpreal32 y0, fpreal32 y1, 
                 fpreal32 y2,  fpreal32 y3, fpreal32 mu)
{
   fpreal32 a0,a1,a2,a3,mu2;

   mu2 = mu*mu;
   a0 = y3 - y2 - y0 + y1;
   a1 = y0 - y1 - a0;
   a2 = y2 - y0;
   a3 = y1;
   re = a0*mu*mu2+a1*mu2+a2*mu+a3;
}


/// Threaded worker class for cubic interpolation.
class op_InterpolateCubic {
public:
    op_InterpolateCubic(GU_Detail *gdp, float delta, float *points, int numPoints, int simd): 
    mygdp(gdp), mydelta(delta), mypoints(points), mynumPoints(numPoints), mySIMD(simd) {};
    // Take a SplittableRange (not a GA_Range)
    void operator()(const GA_SplittableRange &range) const
    {
        //cout <<  "Thread: " << UT_Thread::getMySequentialThreadIndex() << endl;
        GA_RWPageHandleV3   handleP(mygdp->getP());
        UT_Spline *spline  = new UT_Spline();
        spline->setGlobalBasis( UT_SPLINE_MONOTONECUBIC);//UT_SPLINE_CATMULL_ROM);
        spline->setSize(3, 3); 
        // Iterate over pages in the range
        for (GA_PageIterator pit = range.beginPages(); !pit.atEnd(); ++pit)
        {
            GA_Offset start, end;
            handleP.setPage(*pit);
            // iterate over the elements in the page.
            for (GA_Iterator it(pit.begin()); it.blockAdvance(start, end); )
            {
                if (mySIMD == 2)
                {
                    // SIMD cubic interpolation based on SISD code from
                    // http://paulbourke.net/miscellaneous/interpolation
                    // For now I need two temp buffers, but I think it can
                    // be only one...
                    UT_Vector3 p;
                    int n = mynumPoints;
                    int l = 3*(end-start);
                    fpreal32 *y0 =  &mypoints[start*3];
                    fpreal32 *y1 =  &mypoints[(start+n)*3];
                    fpreal32 *y2 =  &mypoints[(start+n*2)*3];
                    fpreal32 *y3 =  &mypoints[(start+n*3)*3];                    
                    fpreal32 *a0 =  new float[l*3];
                    fpreal32 *te =  new float[l*3];
                    fpreal32 *re =  (fpreal32 *)&handleP.value(start);
            
                    //  # Static scalars:
                    //mu2 = mu*mu;  mu3 = mu*mu*mu;
                    fpreal32 mu2 = mydelta*mydelta;
                    fpreal32 mu3 = mu2*mydelta;
          
                    //a0 = y3 - y2;  
                    //y3 = a0 - y0;    
                    //a0 = y3 + y1;    
                    //te = a0 * mu3
                    VM_Math::sub(a0, y3, y2,  l);
                    VM_Math::sub(y3, a0, y0,  l);
                    VM_Math::add(a0, y3, y1,  l);
                    VM_Math::mul(te, a0, mu3, l);

                    //y3 = y0 - y1;
                    //te = y3 - a0;           
                    //re = re + te*mu2;                         
                    VM_Math::sub(y3, y0, y1, l);
                    VM_Math::sub(te, y3, a0, l); // ??? a moze negate(a0) i madd(y3, a0, 1)?
                    VM_Math::madd(re, te, mu2, l);
                   

                    //a0 = y2 - y0;
                    //te = a0*mu + y1;
                    //re = re + te *1;
                    VM_Math::sub(a0, y2, y0, l);
                    VM_Math::scaleoffset(re, te, a0, l);
                    VM_Math::madd(re, te, 1.0f, l);

                    delete a0, te;
                }
                 else if (mySIMD == 1)
                {
                    UT_Vector3 p;
                    int n = mynumPoints;
                    fpreal32 pp[]= {0.0f, 0.0f, 0.0f};

                    for (GA_Offset i = start; i < end; ++i)
                    {
                        fpreal32 v0[] = {mypoints[3*i], mypoints[3*i+1], mypoints[3*i+2]};
                        fpreal32 v1[] = {mypoints[3*(i + n)], mypoints[3*(i + n)+1], mypoints[3*(i + n)+2]};
                        fpreal32 v2[] = {mypoints[3*(i + n*2)], mypoints[3*(i + n*2)+1], mypoints[3*(i + n*2)+2]};
                        fpreal32 v3[] = {mypoints[3*(i + n*2)], mypoints[3*(i + n*2)+1], mypoints[3*(i + n*2)+2]};
                        cubicInterpolate(pp[0], v0[0], v1[0], v2[0], v3[0], mydelta);
                        cubicInterpolate(pp[1], v0[1], v1[1], v2[1], v3[1], mydelta);
                        cubicInterpolate(pp[2], v0[2], v1[2], v2[2], v3[2], mydelta);
                        p.assign(pp[0], pp[1], pp[2]);
                        //PC2SOP::flip_space(p);
                        handleP.set(i, p);
                    }
                   
                //#else
                // Standard loop:
                }
                else
                {
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
                }
                //#endif
            }
        }
        delete spline;
    }

private:
    GU_Detail *mygdp;
    float     mydelta;
    float     *mypoints;
    int       mynumPoints;
    int       mySIMD;

};

void
threaded_simd_cubic(const GA_Range &range, GU_Detail *gdp, float delta, 
                    float *points, int numPoints, int simd)
{
    // Create a GA_SplittableRange from the original range
    GA_SplittableRange split_range = GA_SplittableRange(range);
    UTparallelFor(split_range, op_InterpolateCubic(gdp, delta, points, numPoints, simd));
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
    float delta  = 0.5;
    bool  flip   = false;


    // Open BGEO:
    GU_Detail gdp;
    GA_Detail::IOStatus status;
    status = gdp.load(argv[1]);
    timer t;
    t.start();
    if (!status.success())
    {
        cout << "Can't open file" << endl;
        return 0;
    }
    cout << "Opening bgeo: " <<t.current() << endl;



    // Open PC2:
    float *points;
    UT_String filename = UT_String(argv[2]);
    t.start();
    PC2_File *pc2 = new PC2_File(&filename);
    if (!pc2->loadFile(&filename))
    {
        cout << "Can't open pc2" << endl;
        return 0;
    }
   
    cout << "Opening pc2: " <<t.current() << endl;


    // Loading array:
    t.start();
    points= new float[3 * pc2->header->numPoints * steps];
    cout << "Allocating array: " << t.current()<< endl;

    t.start();
    if(!pc2->getPArray(sample, steps, points))
    {
        cout << "Can't load array" << endl;
        return 0;
    }
   
    cout << "Loading pc2: " << t.current()<< endl;



    /////// Benchmark: //////////////////////////

    //No interpolation:
    int numPoints = pc2->header->numPoints;
    int frames    = 1000; //pc2->header->numSamples;

 
    #if 1
    t.start();
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
   
    cout << "No interpolation: " <<t.current()  << endl;
    #endif


    #if 1
    /// Linear single thread:
    t.start();
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
   
    cout << "Linear single thread: " << t.current() << endl;
    #endif

   
    /// Linear multithead:    
    {
        const GA_Range range(gdp.getPointRange());   
        t.start();  
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_linear(range, &gdp, delta, points, numPoints, false);
        }
       
        cout << "Linear mulithread: " << t.current() / UT_Thread::getNumProcessors() << endl;
    }


       UT_Thread::configureMaxThreads(1); 
      /// Linear single simd: 
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_linear(range, &gdp, delta, points, numPoints, true);
        }
       
        cout << "Linear singlethread SIMD: " << t.current() / UT_Thread::getNumProcessors() << endl;
    }


     /// Linear multithead simd: 
     UT_Thread::configureMaxThreads(0); 
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_linear(range, &gdp, delta, points, numPoints, true);
        }
       
        cout << "Linear multithread SIMD: " << t.current() / UT_Thread::getNumProcessors() << endl;
    }

    /// Cubic singlethread:
    {
        int ptnum = 0;
        UT_Vector3 p;
         GEO_Point *ppt;
        UT_Spline *spline = new UT_Spline();
        spline->setGlobalBasis(UT_SPLINE_CATMULL_ROM);
        spline->setSize(3, 3);
        t.start(); 
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
        
         cout << "Cubic singlethread (HDK): " << t.current()  << endl;
    }


    


    /// Cubic multithead HDK:    
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_cubic(range, &gdp, delta, points, numPoints, 0);
        }
       
        cout << "Cubic multithread (HDK): " << t.current() / UT_Thread::getNumProcessors()  << endl;
    }

      /// Cubic singlethread (own):    
     UT_Thread::configureMaxThreads(1);
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_cubic(range, &gdp, delta, points, numPoints, 1);
        }
       
        cout << "Cubic singlethreaded (own): " << t.current() / UT_Thread::getNumProcessors()  << endl;
    }

    
    
    /// Cubic multithead (own):    
     UT_Thread::configureMaxThreads(0);
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_cubic(range, &gdp, delta, points, numPoints, 1);
        }
       
        cout << "Cubic multithreaded (own): " << t.current() / UT_Thread::getNumProcessors()  << endl;
    }


       /// Cubic singlethread SIMD (own): 
          UT_Thread::configureMaxThreads(1);   
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_cubic(range, &gdp, delta, points, numPoints, 2);
        }
       
        cout << "Cubic singlethreaded SIMD: " << t.current() / UT_Thread::getNumProcessors()  << endl;
    }

     /// Cubic multithead SIMD (own):    
       UT_Thread::configureMaxThreads(0); 
    {
        const GA_Range range(gdp.getPointRange());
        t.start();
        for (int i = 0; i < frames; i++)
        {   
            threaded_simd_cubic(range, &gdp, delta, points, numPoints, 2);
        }
       
        cout << "Cubic multithread SIMD: " << t.current() / UT_Thread::getNumProcessors()  << endl;
    }


    delete points;
    delete pc2;
    return 0;
}








