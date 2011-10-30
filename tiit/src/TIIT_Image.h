#ifndef __TIIT_Image_h__
#define __TIIT_Image_h__

#include <IMG/IMG_File.h>
#include <IMG/IMG_Stat.h>
#include <IMG/IMG_FileParms.h>
#include <PXL/PXL_Raster.h>
#include <UT/UT_String.h>

namespace TIIT
{
class TIIT_Image
{
public:
    /// Open image, optionally loading raster to memory,
    /// providing fileparms to control the process.
    int open(const char *filename, 
               const IMG_FileParms *options = 0, 
               const int load = 0, const char *plane = "C");
    
   const IMG_Stat *getStat() const {return stat;}
                      
   bool isloaded()               {return loaded;}
    
    /// Get to the raster:
    const PXL_Raster *getRaster() const {return raster;}
    UT_String *getComponentsInfo(UT_String*);
    TIIT_Image();
    ~TIIT_Image() {delete myfile; delete raster; }
    
    const PXL_Raster   *raster;
    
private:
    const IMG_File *myfile;
    const IMG_Stat  *stat;
          UT_PtrArray<PXL_Raster *> images;
    bool                            loaded;
    float                         *fpixels;
    
};
}//end of TIIT namespace
#endif
