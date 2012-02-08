/*
    Big junk of this code comes stright from a great example in HDK: HDK_Samples::FullImageFilter.
    I'm also using the code for countinous wavelet transform from Gimp wavelet decompose plugin  
    copyright by Marco Rossini, which in turn originates in UFRaw project.
    
    01.09.2011
    skk.
*/


#include <UT/UT_DSOVersion.h>
#include <UT/UT_Wavelet.h>

#include <OP/OP_Context.h>
#include <OP/OP_OperatorTable.h>

#include <SYS/SYS_Math.h>
#include <SYS/SYS_Floor.h>

#include <PRM/PRM_Include.h>
#include <PRM/PRM_Parm.h>

#include <TIL/TIL_Region.h>
#include <TIL/TIL_Plane.h>
#include <TIL/TIL_Sequence.h>
#include <TIL/TIL_Tile.h>

#include <COP2/COP2_CookAreaInfo.h>
#include "COP2_Wavelets.h"

using namespace COP_WAVELETS;

COP_MASK_SWITCHER(5, "DWT");

static PRM_Name names[] =
{
    PRM_Name("wlftype", "Transform Type"),
    PRM_Name("wlnum",	 "Number of Wavelets"),
    PRM_Name("ifScale",  "Return Single Scale"),
    PRM_Name("numScale", "Return Scale Number"),
    PRM_Name("numComp",  "Return Component Number ")
};

static PRM_Name wlfTypes[] = {
   PRM_Name("dwt",    "Discreet Wavelet"),
   PRM_Name("iwdt",   "Inverse Discreet Wavelet"),
   PRM_Name("cwt",    "Continous Wavelet"),
   PRM_Name("icwt",   "Inverse Continous Wavelet"),
   PRM_Name(0)
   };
   
static PRM_ChoiceList wlfTypesMenu((PRM_ChoiceListType) (PRM_CHOICELIST_EXCLUSIVE | PRM_CHOICELIST_REPLACE), wlfTypes);

//static PRM_Default sizeDef(10);
//static PRM_Range sizeRange(PRM_RANGE_UI, 0, PRM_RANGE_UI, 100);

static PRM_Default myWlNum  = 4;

PRM_Template COP2_Wavelets::myTemplateList[] =
{
    PRM_Template(PRM_SWITCHER,	3, &PRMswitcherName, switcher),
    PRM_Template(PRM_ORD,  TOOL_PARM,  1, &names[0], PRMzeroDefaults, &wlfTypesMenu), 
    PRM_Template(PRM_INT,   TOOL_PARM, 1, &names[1]),
    PRM_Template(PRM_TOGGLE,   TOOL_PARM, 1, &names[2]),
    PRM_Template(PRM_INT,   TOOL_PARM, 1, &names[3]),
    PRM_Template(PRM_INT,   TOOL_PARM, 1, &names[4]),
    
    //PRM_Template(PRM_FLT_J, TOOL_PARM, 1, &names[0],myWlNum &sizeDef, 0, &sizeRange),
    PRM_Template(),
};



OP_TemplatePair COP2_Wavelets::myTemplatePair(
    COP2_Wavelets::myTemplateList,
    &COP2_MaskOp::myTemplatePair);

OP_VariablePair COP2_Wavelets::myVariablePair(0,
					  &COP2_MaskOp::myVariablePair);

const char *	COP2_Wavelets::myInputLabels[] =
{
    "Image to Filter",
    "Mask Input",
    0
};

OP_Node *
COP2_Wavelets::myConstructor(	OP_Network	*net,
					const char	*name,
					OP_Operator	*op)
{
    return new COP2_Wavelets(net, name, op);
}

COP2_Wavelets::COP2_Wavelets(OP_Network *parent,
					   const char *name,
					   OP_Operator *entry)
    : COP2_MaskOp(parent, name, entry)
{
    // sets the default scope to only affect color and alpha. The global
    // default is 'true, true, "*"', which affects color, alpha and all
    // extra planes.
    setDefaultScope(true, true, 0);
}

COP2_Wavelets::~COP2_Wavelets()
{
    ;
}

// -----------------------------------------------------------------------


COP2_ContextData *
COP2_Wavelets::newContextData(const TIL_Plane * /*plane*/,
				     int /*arrayindex*/,
				     float t, int xres, int /*yres*/,
				     int /*thread*/,  int /*maxthreads*/)
{
    // This method evaluates and stashes parms and any other data that
    // needs to be setup. Parms cannot be evaluated concurently in separate
    // threads. This function is guarenteed to be single threaded.
    cop2_FullImageFilterData *sdata = new cop2_FullImageFilterData();
    //int index = mySequence.getImageIndex(t);

    // xres may not be the full image res (if cooked at 1/2 or 1/4). Because
    // we're dealing with a size, scale down the size based on our res.
    // getXScaleFactor will return (xres / full_xres). 
    sdata->myWlfType  = WLFTYPE(t); //* getXScaleFactor(xres)*getFrameScopeEffect(index);
    sdata->myWlNum    = WLNUM(t);
    sdata->myIfScale  = IFSCALE(t);
    sdata->myNumScale = NUMSCALE(t);
	sdata->myNumComp  = NUMCOMP(t);
    return sdata;
}


void
COP2_Wavelets::computeImageBounds(COP2_Context &context)
{
    // if your algorthim increases the image bounds (like blurring or
    // transforming) you can set the bounds here.

    // if you need to access your context data for some information to
    // compute the bounds (like blur size), you can do it like:
    //   cop2_FullImageFilterData *sdata =
    //                (cop2_FullImageFilterData *) context.data();

    // SAMPLES:
    
    // expands or contracts the bounds to the visible image resolution
    context.setImageBounds(0,0, context.myXres-1, context.myYres-1);

    // just copies the input bounds (ie this node don't modify it)
    //copyInputBounds(0, context);

    // expands the input bounds by 5 pixels in each direction.
    //   copyInputBounds(0, context);
    //   int x1,y1,x2,y2;
    //   context.getImageBounds(x1,y1,x2,y2);
    //   context.setImageBounds(x1-5, y1-5, x2+5, y2+5);
    
}

void
COP2_Wavelets::getInputDependenciesForOutputArea(
    COP2_CookAreaInfo		&output_area,
    const COP2_CookAreaList	&input_areas,
    COP2_CookAreaList		&needed_areas)
{
    COP2_CookAreaInfo	*area;

    // for a given output area and plane, set up which input planes and areas
    // it is dependent on. Basically, if you call inputTile or inputRegion in
    // the cook, for each call you need to make a dependency here.

    // this makes a dependency on the input plane corresponding to the output
    // area's plane. 
    area = makeOutputAreaDependOnInputPlane(0,
					    output_area.getPlane().getName(),
					    output_area.getArrayIndex(),
					    output_area.getTime(),
					    input_areas, needed_areas);
    
    // Always check for null before setting the bounds of the input area.
    // in this case, all of the input area is required.
    if (area)
	area->enlargeNeededAreaToBounds();

    // If the node depends on its input conterpart PLUS another plane,
    // we need to add a dependency on that plane as well. In this case, we
    // add an extra dependency on alpha (same input, same time).

    area = makeOutputAreaDependOnInputPlane(0,
					    getAlphaPlaneName(), 0,
					    output_area.getTime(),
					    input_areas, needed_areas);
    // again, we'll use all of the area.
    if (area)
	area->enlargeNeededAreaToBounds();

    getMaskDependency(output_area, input_areas, needed_areas);
    
}

OP_ERROR
COP2_Wavelets::doCookMyTile(COP2_Context &context, TIL_TileList *tiles)
{
    // normally, this is where you would process your tile. However,
    // cookFullImage() is a convenience function which assembles a full image
    // and does all the proper locking for you, then calls your filter
    // function.
    
    cop2_FullImageFilterData *sdata = 
        static_cast<cop2_FullImageFilterData *>(context.data());

    return cookFullImage(context, tiles, &COP2_Wavelets::filter,
			 sdata->myLock, true);
}

OP_ERROR
COP2_Wavelets::filter(COP2_Context &context,
			     const TIL_Region *input,
			     TIL_Region *output,
			     COP2_Node  *me)
{
    // since I don't like typing me-> constantly, just call a member function
    // from this static function. 
    return ((COP2_Wavelets*)me)->filterImage(context, input, output);

}

OP_ERROR
COP2_Wavelets::filterImage(COP2_Context &context,
				  const TIL_Region *input,
				  TIL_Region *output)
{
    // retrive my context data information (built in newContextData).
    cop2_FullImageFilterData *sdata =
	(cop2_FullImageFilterData *) context.data();

    // currently we have a blank output region, and an input region filled with
    // whatever plane we've been told to cook. Both are in the same format, as
    // this node didn't alter the data formats of any planes.

    
    // we need the alpha plane, so grab it (generally, you'd want to check if
    // context.myPlane->isAlphaPlane() first, and then just use the 'input'
    // region if we were cooking alpha, but for simplicity's sake we won't
    // bother). Oh, and we'll grab it as floating point.

    // make a copy of the alpha plane & set it to FP format.
    TIL_Plane alphaplane(*mySequence.getPlane(getAlphaPlaneName()));
    alphaplane.setFormat(TILE_FLOAT32);
    alphaplane.setScoped(1);
    
    TIL_Region *alpha = inputRegion(0, context,    // input 0
				    &alphaplane,0, // FP alpha plane.
				    context.myTime, // at current cook time
				    0, 0, // lower left corner
				    context.myXsize-1, context.myYsize-1); //UR
    if(!alpha)
    {
	// something bad happened, possibly error, possibly user interruption.
	return UT_ERROR_ABORT;
    }

    int comp;
    //int x,y;
    char *idata, *odata;
    //float *adata;

    // Get data:

    //adata = (float *) alpha->getImageData(0);
	
    // go component by component. PLANE_MAX_VECTOR_SIZE = 4.
    for(comp = 0; comp < PLANE_MAX_VECTOR_SIZE; comp++)
    {
	    idata = (char *) input->getImageData(comp);
	    odata = (char *) output->getImageData(comp);
	
	    if(odata)
	    {
	        // since we aren't guarenteed to write to every pixel with this
	        // 'algorithm', the output data array needs to be zeroed. 
	        memset(odata, 0, context.myXsize*context.myYsize * sizeof(float));
	    }

	    if(idata && odata)
	    {
	        // myXsize & myYsize are the actual sizes of the large canvas,
	        // which may be different from the resolution (myXres, myYres).
	        // Cast to float arrays.
	        float *pix = (float *) idata;
            float *out = (float *) odata;
            const unsigned int size = context.myXsize * context.myYsize;
            

       
            // Arrays:
            UT_VoxelArrayF *input  = new UT_VoxelArrayF();
            UT_VoxelArrayF *output = new UT_VoxelArrayF();
            input->size(context.myXsize, context.myYsize, 1);
            output->size(context.myXsize, context.myYsize, 1);
           
            
            /// Copy from image to voxels:
            for (int y = 0; y < context.myYsize; y++)
                for (int x = 0; x < context.myXsize; x++)
                    input->setValue(x, y, 0, pix[(context.myXsize*y)+x]); 
            
            /// Make DW transform:
            if (sdata->myWlfType < 2)
            {
                if (sdata->myWlfType == 0)
                    UT_Wavelet::transformOrdered(UT_Wavelet::WAVELET_HAAR, *input, *output, sdata->myWlNum);
                else
                    UT_Wavelet::inverseTransformOrdered(UT_Wavelet::WAVELET_HAAR, *input, *output, sdata->myWlNum);
            }
            else
            {
                cout << "type: " << sdata->myWlfType << endl;
                cout << "Entering CWT" << endl;
                /// Now continous wavelet transform:
                unsigned int lev, lpass, hpass,col, row;

                 /* image buffers & * temporary storage */
                 float *buffer[2];  
                 float *temp;
                 //int myRows = 0;
                 buffer[0] = pix;
                 buffer[1] = new float[size];
                 temp      = new float[SYSmax(context.myXsize, context.myYsize)];
                 cout << "Size of buffer and temp: " << sizeof(buffer[1]) << " " << sizeof(temp) << endl;
                 
                 // Now we can interate over scales:
                 /* iterate over wavelet scales */
                 lpass = 1;
                 hpass = 0;
                 for (lev = 0; lev < sdata->myWlNum; lev++)
                 {
                    cout << "Computing level: " << lev << endl;
                    lpass = (1 - (lev & 1));
                    cout << "Lpass value: " << lpass << endl;
                    for (row = 0; row < context.myYsize; row++)
                    {
                        //myRows++;
                        hat_transform(temp, buffer[hpass] + row * context.myXsize, 1, context.myXsize, 1 << lev);
                        for (col = 0; col < context.myXsize; col++)
	                    {
	                        buffer[lpass][row * context.myXsize + col] = temp[col];
	                    }
                    }
                    for (col = 0; col < context.myXsize; col++)
                    {
                        hat_transform(temp, buffer[lpass] + col, context.myXsize, context.myYsize, 1 << lev);
                        for (row = 0; row < context.myYsize; row++)
	                    {
	                        buffer[lpass][row * context.myXsize + col] = temp[row];
	                    }
                    }
                    
                    for (int i = 0; i < size; i++)
                    {
                      // rounding errors introduced here (division by 16) 
                      buffer[lpass][i] =  buffer[lpass][i] / 16.0;
                      buffer[hpass][i] -= buffer[lpass][i];
                      /*if (buffer[hpass][c][i] > 255)
	                {
	                  buffer[hpass][c][i] = 255;
	                  clip++;
	                }
                      else if (buffer[hpass][c][i] < 0)
	                {
	                  buffer[hpass][c][i] = 0;
	                  clip++;
	                }*/
                    }
                

                  /* TRANSLATORS: This is a technical term which must be correct. It must
                     not be longer than 255 bytes including the integer number. */
                  printf("Wavelet scale %i\n", lev + 1);
                  //add_layer (image, layer, buffer[hpass], str, settings.layer_modes);
                  hpass = lpass;
                  cout << "My hpass at the end: " << lpass << endl;
                }
                
                // Copy data back to image buffer:
                for (int y = 0; y < context.myYsize; y++)
                {
                    for (int x = 0; x < context.myXsize; x++)
                    {
                        //cout << buffer[hpass][(context.myXsize*y)+x] << endl;
                        out[(context.myXsize*y)+x] = buffer[0][(context.myXsize*y)+x];
                    }
                 }
                delete temp;
                delete buffer[1];
                
            }   
               
               
               
               
            /*   
               
            if (sdata->myIfScale)
            {
                UT_Wavelet::extractComponent(*output, *input, sdata->myNumScale, sdata->myNumComp);
                // Copy data back to image buffer (in reverted bufferes because of 'extractComponent()'):
                for (int y = 0; y < context.myYsize; y++)
                    for (int x = 0; x < context.myXsize; x++)
                        out[(context.myXsize*y)+x] = input->getValue(x, y, 0);
            }
            else
            {
                // Copy data back to image buffer:
                for (int y = 0; y < context.myYsize; y++)
                    for (int x = 0; x < context.myXsize; x++)
                        out[(context.myXsize*y)+x] = output->getValue(x, y, 0);
            }*/
            
            // Get rid of voxels:
            delete input;
            delete output;
            
	                    
	    }
    }
    

    // It is important to release regions and tiles you request with
    // inputRegion & inputTile, otherwise they will just sit around until the
    // end of the cook taking up memory. If someone puts down many of your
    // nodes in a network, this could be problematic.
    releaseRegion(alpha);

    // input and output are allocated & released by cookFullImage, so don't
    // release them.
    
    return error();
}



//int cwaveletTransform(float *pix, float *out, int width, int heigth, int levs )
//{
    //hat(temp, buffer[hpass][c] + row * width, 1, width, 1 << lev);

//}



void
newCop2Operator(OP_OperatorTable *table)
{
    table->addOperator(new OP_Operator("waveletFilter",
				       "Wavelet Filter",
				       COP2_Wavelets::myConstructor,
				       &COP2_Wavelets::myTemplatePair,
				       1, 
				       2, // optional mask input.
				       &COP2_Wavelets::myVariablePair,
				       0, // not generator
				       COP2_Wavelets::myInputLabels));
}

