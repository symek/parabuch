#ifndef __SOP_PointWave_h__
#define __SOP_PointWave_h__

#include <SOP/SOP_Node.h>

namespace PC2SOP {

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
    int getPArray(float time, int steps, float *pr);
    UT_String * getFilename() {return filename;}
    PC2Header *header;
    
private:
    const char *buffer[32];
    int loaded;
    UT_String  *filename;
};


class SOP_PointCache : public SOP_Node
{
public:
	SOP_PointCache(OP_Network *net, const char *name, OP_Operator *op);
    virtual ~SOP_PointCache();

    static PRM_Template  myTemplateList[];
    static OP_Node		*myConstructor(OP_Network*, const char *, OP_Operator *);
   
    virtual OP_ERROR     cookInputGroups(OP_Context &context, int alone = 0);
            OP_ERROR     openPC2File(UT_String filename, float t);

protected:
    /// Method to cook geometry for the SOP
    virtual OP_ERROR		 cookMySop(OP_Context &context);

private:
    void	getGroups(UT_String &str){ evalString(str, "group", 0, 0); }
    void    FILENAME(UT_String &str, float t){ return evalString(str, "filename", 0, t);}

    /// This variable is used together with the call to the "checkInputChanged"
    /// routine to notify the handles (if any) if the input has changed.
    GU_DetailGroupPair	 myDetailGroupPair;
    /// This is the group of geometry to be manipulated by this SOP and cooked
    /// by the method "cookInputGroups".
    const GB_PointGroup	*myGroup;
    float               *points;
    PC2_File            *pc2;
};
} // End PC2SOP namespace

#endif
