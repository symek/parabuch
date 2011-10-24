#include <iostream>
#include <stdio.h>
#include <string.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>
#include <GEO/GEO_PrimVolume.h>

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


int main(int argc, char *argv[]) {

	// Any arguments?
	if ( argc < 3 ) 
	{
		cout << "\nUsage: vol2bgeo infile.vol outfile.bgeo\n" << endl; 
		return 1; 
	}

	// Init some vars:                
	int x, y, z, v;
	unsigned char *voxels = NULL;
	int result;
	GU_Detail            gdp;
   
	
	// Open input *.vol file:
	FILE * fin = fopen(argv[1], "rb");
	if (fin == NULL) return 1;
	char buf[4096];
	result = fread(buf, 4096, 1, fin);
	VolumeHeader *header = (VolumeHeader *)buf;
	int volBytes = header->volSize*header->volSize*header->volSize*header->numChannels;
	voxels = new unsigned char[volBytes];
	int size   = header->volSize;
	result = fread(voxels, volBytes, 1, fin);		
	fclose(fin);
	
	// Print some statistics:
	cout << "Input: " << argv[1] << endl;
	cout << "Output: "<< argv[2] << endl;
	cout << "Volume Size (cubic): " << header->volSize << endl;
	cout << "Num of Channels: " << header->numChannels << endl;
	cout << "Bytes per Channel: " << header->bytesPerChannel << endl;
	cout << "Wrap texture: " << header->wrap << endl;


	
	//GU_PrimVolume * volumes[header->numChannels];

	// Create bgeo and helpers for three volumes:
	// I don't know why, but I can't do all fields in one loop.
	// Program hangs if I write to more then one handle (?)
	UT_Matrix3 xform;
    xform.identity();
    xform.scale(.5, .5, .5);

	for (v=0; v<header->numChannels; v++)
	{
		GU_PrimVolume *volume = (GU_PrimVolume *)GU_PrimVolume::build(&gdp);
        volume->getVertex().getPt()->getPos() = UT_Vector3(0.5, 0.5, 0.5);
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
					
	gdp.save(argv[2], 1, 0);
	delete voxels;
	return 0;

}
