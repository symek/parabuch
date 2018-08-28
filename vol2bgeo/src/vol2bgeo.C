#include <iostream>
#include <stdio.h>
#include <string.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <GEO/GEO_PrimVolume.h>
#include <UT/UT_String.h>
#include <IMG3D/IMG3D_Manager.h>
#include <UT/UT_Vector3.h>
#include <UT/UT_BoundingBox.h>

using namespace std;

// Header structure of *.vol file
struct VolumeHeader
{
    char magic[4];
    int version;
    char texName[256];
    bool wrap;
    int volSize;
    int numChannels;
    int bytesPerChannel;
};



inline void endian_swap(unsigned int& x)
{
    x = (x>>24) | 
        ((x<<8) & 0x00FF0000) |
        ((x>>8) & 0x0000FF00) |
        (x<<24);
}

int createBgeo(GU_Detail *gdp, unsigned char *voxels, int numChannels, int size)
{
    //GU_PrimVolume * volumes[header->numChannels];

    // Create bgeo and helpers for three volumes:
    // I don't know why, but I can't do all fields in one loop.
    // Program hangs if I write to more then one handle (?)
    int x, y, z, v;
    UT_Matrix3 xform;
    xform.identity();
    xform.scale(.5, .5, .5);

    for (v=0; v<numChannels; v++)
    {
        GU_PrimVolume *volume = (GU_PrimVolume *)GU_PrimVolume::build(gdp);
        volume->getDetail().setPos3(volume->getPointOffset(0), UT_Vector3(0.5, 0.5, 0.5));
        volume->setTransform(xform);
        volume->setCompressionTolerance(.1);
        UT_VoxelArrayWriteHandleF   handle = volume->getVoxelWriteHandle();
        handle->size(size, size, size);

    // and iterate over interleased fields:
        {
        for (z=0; z < size; z++)
            for (y=0; y < size; y++)
                for (x=0; x < size; x++) 
                {
                    int value = voxels[v+(x+y*size+z*size*size)*3];
                    handle->setValue(z, y, x, (float)value * (1.0/256));
                }   
        }
        volume->recompress();
    }
    return 1;
} 




int main(int argc, char *argv[]) {

    // Any arguments?
    if ( argc < 3 ) 
    {
        cout << "\nUsage: vol2bgeo infile.vol outfile.bgeo\n" << endl; 
        return 1; 
    }
    UT_String inputname, outputname;

    inputname.harden(argv[1]);
    outputname.harden(argv[2]);

    // Init some vars:                
    unsigned char *voxels = NULL;
    int success;
    char buf[4096];
   
    
    /// Open input *.vol file:
    FILE * fin = fopen(inputname.buffer(), "rb");
    if (fin == NULL) 
    {
        cerr << "Can't open file file " << inputname << endl;
        return 1;
    }
    
    success = fread(buf, 4096, 1, fin);
    
    if (!success)
    {
        cerr << "Couldn't load a header from "<< inputname << endl;
        return 1;
    }
    
    VolumeHeader *header = (VolumeHeader *)buf;
    int volBytes = header->volSize*header->volSize*header->volSize*header->numChannels;
    voxels = new unsigned char[volBytes];
    int size   = header->volSize;
    success = fread(voxels, volBytes, 1, fin);      
    fclose(fin);
    
    if (!success)
    {
        cerr << "Can't load voxels from" << inputname << endl;
        return 1;
    }
    
    /// Print some statistics:
    cout << "Input: " << inputname << endl;
    cout << "Output: "<< outputname << endl;
    cout << "Volume Size (cubic): " << size << endl;
    cout << "Num of Channels: " << header->numChannels << endl;
    cout << "Bytes per Channel: " << header->bytesPerChannel << endl;
    cout << "Wrap texture: " << header->wrap << endl;
    
     /// Create Volume primitives from raw voxels:
    if (!strcmp(outputname.fileExtension(), ".bgeo"))
    {
        GU_Detail *gdp = new GU_Detail();
        success = createBgeo(gdp, voxels, header->numChannels, size);
        delete[] voxels;
        if (success)
        {
            gdp->save(outputname.buffer(), nullptr);
            delete gdp;
            return 0;
        }       
    }
    /// Create i3d texture out of raw voxels:
    if (!strcmp(outputname.fileExtension(), ".i3d"))
    {
        IMG3D_Manager       i3dmanager;
        UT_BoundingBox      bounds;
        const char          *chnames[1] = { "color" };
        int                  chsizes[1] = { 3 };
        bounds.initBounds(-1, -1, -1);
        bounds.enlargeBounds(1, 1, 1);
        
        float *fvoxels = new float[size*size*size*header->numChannels];
        const float *varray[1] = {fvoxels};
        for (int x = 0; x < size*size*size*header->numChannels; x++)
            fvoxels[x] = (float)voxels[x] * (1.0f/256);
       
        success = i3dmanager.createTexture(outputname.buffer(), bounds, size, size, size);
        success = i3dmanager.fillUntiledTexture(1, chnames, chsizes, varray, 4);
        success = i3dmanager.closeTexture();
        if (success)
            cout << "I3d saved."<< endl;
        
        delete[] voxels;
        delete[] fvoxels;
        return 0;
    }
    
    return 1;

}
