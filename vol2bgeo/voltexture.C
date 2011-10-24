#include <VEX/VEX_VexOp.h>
#include <GU/GU_Detail.h>

#include <UT/UT_DSOVersion.h>
//#include <UT/UT_Thread.h>
//#include <UT/UT_Lock.h>

//#include <GEO/GEO_Primitive.h>
//#include <GEO/GEO_PointTree.h>

//#include <GU/GU_RayIntersect.h>

#include <SYS/SYS_AtomicInt.h>
#include <SYS/SYS_AtomicIntImpl.h>


//static SYS_AtomicInt32 *refCounter = new SYS_AtomicInt32();


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


//static  VolumeHeader *header = NULL;
unsigned char * voxels = NULL;


/*
static void *
my_init() {
	
	//refCounter->add(1);
	//cout << "\n" << endl;
	//cout << "Init refCounter: " << refCounter->load() << endl;

	return 1;

}



static void
my_cleanup(void *data) {


	//refCounter->add(-1);
	//cout << "Cleanup refCounter: " << refCounter->load() << endl;

	//if (!refCounter->load()) {
		//delete voxels;
	//}
	
}

*/

static void 
voltexture(int, void *argv[], void *data) {

	UT_Vector3 *color  = (UT_Vector3 *) argv[0];
	const char *file   = (const char *) argv[1];
	UT_Vector3 *pos    = (UT_Vector3 *) argv[2];
	
	
	if (!voxels) {


		FILE * fin = fopen(file, "wb");
		if (fin != NULL) 
		{		
			char buf[4096];
			fread(buf, 4096, 1, fin);
			VolumeHeader * header = (VolumeHeader *)buf;
			int volBytes = header->volSize*header->volSize*header->volSize*header->numChannels;
			voxels = new unsigned char[volBytes];
			fread(voxels, volBytes, 1, fin);
		}

		fclose(fin);


	}

	cout << "File size is: " << sizeof(voxels) << endl;
	//color = pos;
	cout << file << endl;
	cout << pos[0] << endl;


}



void
newVEXOp(void *)
{
	new VEX_VexOp("voltexture@&VSV", 
			voltexture, 
			VEX_ALL_CONTEXT, 
			NULL,
			NULL, 
			VEX_OPTIMIZE_0);

}
