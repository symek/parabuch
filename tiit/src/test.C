#include <IMG/IMG_FileParms.h>
#include <IMG/IMG_Stat.h>

#include "TIIT_Image.h"


using namespace TIIT;


int
main(int argc, char *argv[])
{	
 	/// No options? Exit:  
	if (argc == 1) return 0;

	/// File we work on:
	const char *input_file = argv[argc-1];
	IMG_FileParms *parms = new IMG_FileParms();
    parms->readAlphaAsPlane();
    parms->setDataType(IMG_FLOAT);
    const IMG_Stat  *stat;
    
	TIIT_Image *tiit = new TIIT_Image();
	tiit->open(input_file, 0, 1);
	if (tiit->isloaded())
	{
	    UT_String *cinfo = new UT_String();
	    stat = tiit->getStat();
	    cout << tiit->raster->getXres() << endl;
	    tiit->getComponentsInfo(cinfo);
	    //cout << i << endl;
	    
	 }
	

delete tiit;
return 0;   	    
}

