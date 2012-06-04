// TODO: 
// 1. hash (touched)
// 2. NaN, Inf (touched)
// 3. Stats (touched)
// 4. Similarity (researching Discrete Wavelet Transform based image comparition).
// 5. write tags (is it possible with HDK, or wee need openEXR headers?)
// 6. Integrity check (?)
// 7. remove openssl dependency.


//HDK:
#include <IMG/IMG_File.h>
#include <IMG/IMG_Stat.h>
#include <IMG/IMG_FileParms.h>
#include <IMG/IMG_Format.h>
#include <CMD/CMD_Args.h>
#include <UT/UT_PtrArray.h>
#include <PXL/PXL_Raster.h>
#include <UT/UT_String.h>
//#include <UT/UT_Wavelet.h>

// std cmath (to consider for isnan() and isinf())
#include <cmath>
#include<fstream>

// Hash sha-1:
#include <openssl/sha.h>

// std:
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
	cout << "\t -c file    compare wavelet sig of the image with a file." << endl;
	cout << "\t -S         Suppress output (only minimal data sutable for parasing)." << endl;
	cout << "\t -m         Print meta-data." << endl;
    cout << "\t -L lut     LUT to be applied on input." << endl;
    cout << "\t -o         Output file." << endl;

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

float *fixBadPixels(float *pixels, const int height, const int width)
{
	float  newpixel = 0.0;
	
	for (int i=0; i < height*width*3; i++)
	{
		if (std::isnan(pixels[i]) || std::isinf(pixels[i]))
		{
			newpixel += pixels[i-3];
			newpixel += pixels[i+3];
			newpixel += pixels[i-(width*3)];
			newpixel += pixels[i+(width*3)];
			//cout << "Old: " << pixels[i] << ", new: " << newpixel / 4.0 << endl;  
			pixels[i] = newpixel / 4.0;
			
		}
    }

	return pixels;

}

void * printStats(const char *inputName, const char *plane_name, const int fixit)
{
 
    // Load time paramters:
    IMG_FileParms *parms = new IMG_FileParms();
    parms->readAlphaAsPlane();
    parms->setDataType(IMG_FLOAT);
    
    UT_PtrArray<PXL_Raster *> images;
    IMG_File *inputFile  = IMG_File::open(inputName, parms);
    bool loaded          = inputFile->readImages(images);
    const IMG_Stat &stat = inputFile->getStat();
    int px               = stat.getPlaneIndex(plane_name);
    float min, max, avr, avg, avb;
	min = max = avr = avg = avb = 0.0;
	void *myData;
	
    if (px != -1)
    {
	    PXL_Raster *raster  = images(px);
	    myData              = raster->getPixels();
	    float *buff         = (float*)myData;
		int npix            = raster->getNumPixels();
		cout << "Packing: " << raster->getPacking() << endl;

	    /// Range & avarages: FIXME: how many channels we have here?
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

		/// Nans/Infs:
		int nans = 0; int infs = 0;
		for (int i = 0; i < npix*3; i++)
	    {
		    nans  +=  std::isnan(buff[i]);
			infs  +=  std::isinf(buff[i]);
	    }
		cout << "Nans found: " << nans << endl;
		cout << "Infs found: " << infs << endl;
		if (fixit && (nans || infs))
	    {
	        buff = fixBadPixels(buff, raster->getYres()-1, raster->getXres()-1);
	    }

	}

	inputFile->close();
	delete inputFile;
	return myData;
}
/*
const char *waveletSig(void *myData, float *weights, const int xsize, const int ysize)
{
    float *pixels = (float*) myData;
    UT_VoxelArrayF *input  = new UT_VoxelArrayF();
    UT_VoxelArrayF *output = new UT_VoxelArrayF();
    input->size(xsize, ysize, 1);
    output->size(xsize, ysize, 1);
           
            
    /// Copy from image to voxels:
    for (int y = 0; y < ysize; y++)
        for (int x = 0; x < xsize; x++)
            input->setValue(x, y, 0, pixels[(xsize*y)+x]); 
            
    /// Make DWT:     
    UT_Wavelet::transformOrdered(UT_Wavelet::WAVELET_HAAR, *input, *output, 8);
    cout << "Wavelets done." << endl;
    
    return "wavelets";
}

*/


int
main(int argc, char *argv[])
{	
 	/// No options? Exit:  
	if (argc == 1)
	{
		usage();
		return 0;
	}
  
	/// Read argumets and options:
	CMD_Args args;
    args.initialize(argc, argv);
    args.stripOptions("p:c:w:L:o:ihsfSm");

	/// File we work on:/
	const char *inputName = argv[argc-1];
    const char *lut_file   = NULL;
	//inputName.harden();
	//fstream fin(inputName);
	//if (!fin)
	//{
	//	cerr << "Can't find: " << inputName << endl;
	//	return 1;
	//}

	// Optional read parameters:
	IMG_FileParms *parms = new IMG_FileParms();
    if (args.found('L'))
    {
        lut_file = args.argp('L');
        if (lut_file)
             parms->applyLUT(lut_file, "C");
    }

    /// At first we just open the file to read stat:
    IMG_File *inputFile = NULL;
   
    /// Read file:
	inputFile = IMG_File::open(inputName, parms);
	if (!inputFile)
    { 
		cerr  << "Can't open: " << inputName << endl;
		return 1;
	}
	
	/// Print basic info:
	static const IMG_Stat &stat = inputFile->getStat();
	printBasicInfo(inputFile, &args);

	/// Under this line all routines will require myData
	UT_PtrArray<PXL_Raster *> images; // arrays of rasters
	bool       loaded        = false; // did we load the file
	const char *plane_name   = "C"  ; // default raster name
	PXL_Raster *raster       = NULL;  // working raster 
	void       *myData       = NULL;  // data ready to be casted
	int         npix         = 0;          
	
	/// Switch working plane if requested 
	if (args.found('p')) 
	{
		plane_name = args.argp('p');
		cout << "Work plane: " << plane_name << endl;
	}

	/// 'Integrity check' basicaly means we try to load 
	/// the file into memory: fail == it's broken.      
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

	/// Sha-1 hash generation 
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

	/// Print statistics and optionally fix nans/infs: 
	/// FIXME: fix doesn't work yet.
	if (args.found('s') || args.found('f'))
	{
	    /// I'm acctually reloading the image with 32float,
	    //float *fpixels;
	    myData = printStats(inputName, plane_name,  args.found('f'));
	}
	
	/// Meta data:
	if (args.found('m'))
	{
		UT_String info = "";
		inputFile->getAdditionalInfo(info);
		cout << info.buffer()  << endl;
	}

    if (args.found('o'))
    {
        const char *outputName = NULL;
        outputName = args.argp('o');
        if (outputName)
            inputFile->copyToFile(inputName, outputName, parms);

    }

    /*
    if (args.found('w'))
    {
        if (myData)
        {
            const char *sig;
            float weights[3] = {1, 1, 1};
            sig = waveletSig(myData, weights, raster->getXres(), raster->getYres());
        }
    }

    */
}
