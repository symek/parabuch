#include "TIIT_Image.h"

using namespace TIIT;

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
TIIT_Image::TIIT_Image() {;}

int TIIT_Image::open(const char *filename, 
                 const IMG_FileParms *options, 
                 const int load , 
                 const char *plane)
{  
    static IMG_File *file = IMG_File::open(filename, options);
    const  IMG_Stat &s    = file->getStat();
    stat = &(s); 
    myfile = file;
      
    if (load)
    {
        loaded = file->readImages(images);
        if (loaded)
        {
            int px = stat->getPlaneIndex(plane);
            if (px != -1)
            {
                raster  = images(px);
                fpixels  = (float*)raster->getPixels();
            }
        }
        else
        {
            loaded = false;
            return 0;  
        }
    }  
    return 1;
}

void
TIIT_Image::printCompInfo(UT_String *info)
{
    int nplanes  = stat->getNumPlanes();
    for (int i = 0; i < nplanes; i++)
	    {
		    IMG_Plane *plane = stat->getPlane(i);
		    printf("%s/%s/", plane->getName(), getDataTypeName(plane->getDataType()));
		    info->insert(info->length(), i);
		    for (int j = 0; j < 3; j++) 
		    {
			    //const char *compName = plane->getComponentName(j);
			    //if (compName) 
			        //info->append(*compName);
		    }
		}
	info->harden();	
    return info;
}



