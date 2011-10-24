/*
 * TIIT - Tiny Image Inspection Tool or...:
 * Yet another 'iinfo' utility. The one coming with Houdini is nice, but doesn't do much.
 * Its file integrity check isn't that useful, as most broken renders are correctly closed files.
 * Mantra and many other renderes saves images bucketes after buckets hardly leaving it opened. 
 * Another life of 'iinfo' exists thanks to Lary Gritz's OpenImageIO. It has a couple of nice additions, 
 * like computing hash from pixels, looking for NaN as Inf pixel values, but:
 *  - it also doesn't deal with renders with missing buckets
 *  - it doesn't handle well Mantra's EXR images. Seems to get confused with mixed bitdepth per raster and/or
 *    compression schema. Mantra's EXRs periodically crash OIIO's iinfo...
 * 
 * So, facing these issues I wrote yet another image inspection tool called (yes, right!): iinfo;
 * Our goals:
 *  - reimplement SESI's and OIIO's 'iinfo' functionality. 
 *  - implement some sort of buckets sensitive debugging procedure.
 *  - implement 'similarity' check (to compare/find equal images saved in different files/formats). 
 *  - optionally repair NaNs and infs.
 *  - make possible to read/(!)write arbitrary tags in OpenEXR/(?)JPEG files.
 *  
 *
 * 16.10.2011, skk.
*/

// TODO: 
// 1. hash (touched)
// 2. NaN, Inf (touched)
// 3. Stats (touched)
// 4. Similarity (researching Discrete Wavelet Transform based image comparition).
// 5. write tags (is it possible with HDK, or wee need openEXR headers?)
// 6. Integrity check (?)


//HDK:
#include <IMG/IMG_File.h>
#include <IMG/IMG_Stat.h>
#include <IMG/IMG_FileParms.h>
#include <IMG/IMG_Format.h>
#include <CMD/CMD_Args.h>
#include <UT/UT_PtrArray.h>
#include <PXL/PXL_Raster.h>
#include <IMG/IMG_FileParms.h>
#include <UT/UT_String.h>

// std cmath (to consider for isnan() and isinf())
#include <cmath>
#include<fstream>

// Hash sha-1 stuff:
#include <openssl/sha.h>
#include <string.h>


/// http://en.wikipedia.org/wiki/Discrete_wavelet_transform

class DWT
{
public:
   DWT();
   DWT(float *data, int len);
   ~DWT();
   int    setData(float *data, int len);
   float* getData();
   int    transform();
   float* getSignature();
   int    compareSignature(float*);
private:
    float * data;
    float * signature; 
  
};

// Playground with openssl and hash generation.
/// Add:  -L/usr/lib -lssl to complie with hcustion.
    /*
    SHA256_CTX context;
    unsigned char md[SHA256_DIGEST_LENGTH];

    SHA256_Init(&context);
    SHA256_Update(&context, (unsigned char*)input, length);
    SHA256_Final(md, &context); 
    */

  
void me()
{
	cout << endl;
	cout << "\ttiit: tiny image inspection tool." << endl << endl;
}

void usage()
{
	//me();	
	cout << "Usage: tiit [options] picfile.ext" << endl;
	cout << "options:" << endl;
	cout << "\t -i         check file integrity." << endl;
	cout << "\t -h         print image sha-1 sum." << endl;
	cout << "\t -s         print statistics." << endl;
	cout << "\t -f         fix NaNs and Infs if exist." << endl;
	cout << "\t -p plane   replace working plane (default 'C')." << endl;
	cout << "\t -w         print image wavelet signature." << endl;
	cout << "\t -c file    compare wavelet sig of the image with file." << endl;
	cout << "\t -S         Suppress output (only minimal data sutable for parasing)." << endl;
	cout << "\t -m         Print meta-data." << endl;

}

const char * getDataTypeName(const int type)
{
	const char * typeName = "";
	switch(type)
	{
		case 8: typeName = "32float"; break;
		case 16: typeName = "16float"; break;
		case 1: typeName = "8Int"; break;
		case 2: typeName = "16Int"; break;
	}
	return typeName;
}

/* Prints statistics*/
void printBasicInfo(IMG_File *file, CMD_Args *args)
{
	
	const IMG_Stat &stat = file->getStat();
	const int numPlanes  = stat.getNumPlanes();
	cout <<"Filename  : " << stat.getFilename() << endl;
	printf("Resolution: %ix%i\n", stat.getXres(), stat.getYres());
	printf("Rasters   : ");
	
	for (int i = 0; i < numPlanes; i++)
	{
		IMG_Plane *plane = stat.getPlane(i);
		cout << "";
		printf("%s/%s/", plane->getName(), getDataTypeName(plane->getDataType()));
		for (int j = 0; j < 3; j++) 
		{
			const char *compName = plane->getComponentName(j);
			if (compName) printf("%s", compName );
		}
		cout << ", ";
	}
	printf("\n");
}

void * printStats(const char *inputName, const char *plane_name)
{
 
    // Load time paramtersL
    IMG_FileParms *parms = new IMG_FileParms();
    parms->readAlphaAsPlane();
    parms->setDataType(IMG_FLOAT);
    
	UT_PtrArray<PXL_Raster *> images;
    IMG_File *inputFile  = IMG_File::open(inputName, parms);
    bool loaded          = inputFile->readImages(images);
    const IMG_Stat &stat = inputFile->getStat();
    int px               = stat.getPlaneIndex(plane_name);
	void *myData;
    if (px != -1)
    {
		float min, max, avr, avg, avb;
		min = max = avr = avg = avb = 0.0;
	    PXL_Raster *raster  = images(px);
	    myData              = raster->getPixels();
	    const float *buff   = (const float*)myData;
		int npix            = raster->getNumPixels();

	    /// Range & avarages:
        raster->getRange(min, max);
        cout << "Min value : " << min << endl;
        cout << "Max value : " << max << endl;

	    for (int i =0; i < npix*3; i+=3)
	    {
		    avr  +=  buff[i];
		    avg  +=  buff[i+1];
		    avb  +=  buff[i+2];
	    }
	    avr /= npix; avg /= npix; avb /= npix;
	    printf("Avarages  : %f, %f, %f\n", avr, avg, avb);

		/// Nans:
		int nans = 0; int infs = 0;
		for (int i = 0; i < npix*3; i++)
	    {
		    nans  +=  std::isnan(buff[i]);
			infs  +=  std::isinf(buff[i]);
	    }
		cout << "Nans found: " << nans << endl;
		cout << "Infs found: " << infs << endl;



	}

	inputFile->close();
	delete inputFile;
	return myData;
}

/*
void *fixBadPixels(void *data, const int height, const int width)
{

	float *pixels   = (float *)data;
	float  newpixel = 0.0;
	
	for (int i=0; i < height*width*3; i++)
	{
		if (std::isnan(pixels[i]) || std::isinf(pixels[i]))
		{
			newpixel += pixels[3*height*(i-1)+i*3];
			newpixel += pixels[3*height*(i+1)+i*3];
			newpixel += pixels[(3*height*i)+(3*(i+1))];
			newpixel += pixels[(3*height*i)+(3*(i-1))];
			pixels[i] = newpixel / 4.0;
		}

	return pixels;

}

*/

int
main(int argc, char *argv[])
{	
 	// No options? Exit:  
	if (argc == 1)
	{
		usage();
		return 0;
	}
  
	/* Read argumets and options:*/
	CMD_Args args;
    args.initialize(argc, argv);
    args.stripOptions("p:c:w:ihsfSm");

	/* File we work on: */
	const char *inputName = argv[argc-1];
	//inputName.harden();
	fstream fin(inputName);
	if (!fin)
	{
		cerr << "Can't find: " << inputName << endl;
		return 1;
	}	
	
    /* At first we just open the file to read stat:*/
    IMG_File *inputFile = NULL;
	inputFile = IMG_File::open(inputName);
	if (!inputFile)
    { 
		cerr  << "Can't open: " << inputName << endl;
		return 1;
	}
	
	/* Print basic info: */
	static const IMG_Stat &stat = inputFile->getStat();
	printBasicInfo(inputFile, &args);


	/* Under this line all routines will require myData  */
	/*-------------------------------------------------*/

	UT_PtrArray<PXL_Raster *> images; // arrays of rasters
	bool       loaded        = false; // did we load the file
	const char *plane_name   = "C"  ; // default raster name
	PXL_Raster *raster       = NULL;  // working raster 
	void       *myData       = NULL;  // data ready to be casted
	int  npix                = 0;          
	
	/* Switch working plane if requested */ 
	if (args.found('p')) 
	{
		plane_name = args.argp('p');
		cout << "Work plane: " << plane_name << endl;
	}

	/* 'Integrity check' basicaly means we try to load */
	/* the file into memory: fail == it's broken.      */
	if (args.found('i') || args.found('h') || args.found('s') ||
	    args.found('c') || args.found('w') || args.found('f'))
	{
		loaded = inputFile->readImages(images);
		if (!loaded)
		{
			cerr << "Integrity : Fail" << endl;
			return 1;
		} 
		else 
		{
			cout << "Integrity : Ok" << endl;
			// TODO: default C could not exists!
			// Look for it, and choose diffrent if neccesery
			int px = stat.getPlaneIndex(plane_name);
		    if (px != -1)
		    {
			    raster  = images(px);
			    myData  = raster->getPixels();
				npix    = raster->getNumPixels();
		    }
		    else
		    {
			    cerr << "No raster :" << plane_name << endl;
			    return 1;
		    }
		}
	}

	/* Sha-1 hash generation */
	if (args.found('h'))
	{
		if (myData)
		{
		    const unsigned char *buff;
		          unsigned char hash[20];
			buff = (const unsigned char*) myData;
			SHA(buff, strlen((const char*)buff), hash);
			cout << "SHA-1  sum: ";
			for (int i = 0; i < 20; i++) printf("%02x", hash[i]);
			cout << endl;			
		} 
		else
		{
		    cerr << "Ups... No sha-1!" << endl;
		}
	}

	/* Print statistics:*/
	if (args.found('s'))
	{
	    // I acctually have to reload the image with 32float,
		// otherwise computing stats will be nightmare
	     myData = printStats(inputName, plane_name); 
	}
		
	/* Fix nans/infs: */
	/*
	if (args.found('f'))
	{
	    myData = fixBadPixels(myData, raster->getXres(), raster->getYres()); 
	}*/

	if (args.found('m'))
	{
		UT_String info = "";
		inputFile->getAdditionalInfo(info);
		cout << info.buffer()  << endl;
	}


	/*
    PXL_Raster *A  = NULL;
    PXL_Raster *C  = NULL;
	
    
    
    // Set parameters for loading an image:
    // Specifically wa want to have an alpha in separate plane
    // for a moment.
    IMG_FileParms *parms = new IMG_FileParms();
    //parms->setInterleaved(IMG_NON_INTERLEAVED);
	parms->readAlphaAsPlane();
	parms->setDataType(IMG_FLOAT);
	//parms->scaleImageTo(512, 512);
    
     
    // TODO: Need to check if an option is a valid path,
    // before accessing it.
    if (args.found('f')) {
        cout << "\tLoading in: " << args.argp('f') << endl;
        file = IMG_File::open(args.argp('f'), parms);
    }
    
    // Load an rasters. We need to decide prior which raster to load.
    // Currently I'm aiming to work on alpha channel solely.
    if (file)
     {
        // Load in statistics and find planes:
        static const IMG_Stat &stat = file->getStat();
        isLoaded = file->readImages(images);
        cout << "\tResolution: " << stat.getXres() << " x " << stat.getYres() << endl; 
          
        if (isLoaded) 
        {
            // Get main plates:
            A = images(stat.getPlaneIndex("A"));
            C = images(stat.getPlaneIndex("C"));
			cout << "\tImg valid?: " << C->isValid() << endl;
			void        *data = C->getPixels();
			const float *buff = (const float*)data;	
            
            // List planes:
            cout << "\tOur Planes: ";
            for (int i = 0; i < stat.getNumPlanes(); i++)
            {
                cout << stat.getPlane(i)->getName() << " / ";
                //cout << IMG_DataType(stat.getPlane(i)->getDataType())<< ", " ;
            
            }
            cout << endl;
            
            // Perform stats:
            if (args.found('s'))
            {
                float min, max, avr, avg, avb = 0.0;
				int   npix;

				// Range, npixels:
                C->getRange(min, max);
                cout << "\tMin  pixel: " << min << endl;
                cout << "\tMax  pixel: " << max << endl;
				npix = C->getNumPixels();
						
				// Avarage pixels values:
				for (int i =0; i < npix*3; i+=3)
				{
					avr  +=  buff[i];
					avg  +=  buff[i+1];
					avb  +=  buff[i+2];
				}
				avr /= npix; avg /= npix; avb /= npix;
				printf("\tAvarage  C: %f, %f, %f\n", avr, avg, avb);				
            }
            
            // Perform hash:
			// TODO: consider freeing openssl dependency:
            if (args.found('h'))
            {
                printHash((const unsigned char*) data);

			cout << "\tSHA-1  key: ";
    for (i = 0; i < 20; i++) 
    {
        printf("%02x", md[i]);
    }
    return hash;
            }
        } 
		else
		{
			cout << "\tCan't load this image: " << args.argp('f') << endl;
		}
    }
    else 
    {
        cout << "\tCant't find this file: " << args.argp('f') << endl;
    }
    
   	cout << endl;
	//IMG_File::copyToFile(args.argp('f'), "/tmp/test.exr", parms);
	*/
	/*
	int lev;
	int lpass;
	int scales = 100;
	unsigned int tmp;
	for (lev = 0; lev < scales; lev++)
    {
      lpass = (1 - (lev & 1));
      tmp = 1 << lev;
      cout << lev << "," << lpass << ", " << tmp << endl;
    }
    
    float *tttt = new float [16];
    for (lev = 0; lev < 16; lev++)
    {
         tttt[lev] = float(lev);
         cout << tttt[lev] << endl;
    }*/

}
