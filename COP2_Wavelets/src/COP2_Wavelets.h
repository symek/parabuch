
#ifndef _COP2_WAVELETS_H_
#define _COP2_WAVELETS_H_

#include <UT/UT_Lock.h>
#include <COP2/COP2_MaskOp.h>

namespace COP_WAVELETS {

/* code copied from UFRaw (which originates from dcraw) */
static void
hat_transform (float * temp, float * base, int st, int size, int sc)
{
  int i;
  for (i = 0; i < sc; i++)
    temp[i] = 2 * base[st * i] + base[st * (sc - i)] + base[st * (i + sc)];
  for (; i + sc < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)] + base[st * (i + sc)];
  for (; i < size; i++)
    temp[i] = 2 * base[st * i] + base[st * (i - sc)]
      + base[st * (2 * size - 2 - (i + sc))];
}


class COP2_Wavelets : public COP2_MaskOp
{
public:
    // Normal op stuff...
    static OP_Node		*myConstructor(OP_Network*, const char *,
					       OP_Operator *);
    static OP_TemplatePair	 myTemplatePair;
    static OP_VariablePair	 myVariablePair;
    static PRM_Template		 myTemplateList[];
    static CH_LocalVariable	 myVariableList[];
    static const char		*myInputLabels[]; 
    

    // static cookFullImage callback.
    static OP_ERROR	filter(COP2_Context &context,
			       const TIL_Region *input,
			       TIL_Region *output,
			       COP2_Node  *me);
    // non static version, called by filter.
    OP_ERROR		filterImage(COP2_Context &context,
				    const TIL_Region *input,
				    TIL_Region *output);

    // since this is single threaded per-plane, hint this to the scheduler.
    // You can restrict threading so that:
    //    maxp  - a maximum of 'maxp' threads cooking the same plane in this
    //            node at once.
    //    maxn  - a maximum of 'maxn' threads can be cooking in a single
    //            instance of this node at once.
    //    op    - a maximum of 'op' threads can be cooking in instances of
    //            this operator at once.
    
    // This basically says that only 1 thread may cook a given plane at a time,
    // but several threads may be in this node cooking different planes.
    virtual void getMaxNumThreadsInCook(COP2_Context &,
					int &maxp, int &maxn, int &op) const
				{ maxp = 1; maxn =  op = TIL_MAX_THREADS; }

    // For the output area (an area of a plane belonging to this node)
    // and a set of input areas, determine which input areas and which
    // parts of these areas are needed to cook the output area.
    virtual void	 getInputDependenciesForOutputArea(
			    COP2_CookAreaInfo &output_area,
			    const COP2_CookAreaList &input_areas,
			    COP2_CookAreaList &needed_areas);

protected:
    virtual ~COP2_Wavelets();
    virtual COP2_ContextData	*newContextData(const TIL_Plane *p,
						int array_index,
						float t,
						int xres, int yres,
						int thread,
						int max_threads);
    
    virtual OP_ERROR	doCookMyTile(COP2_Context &context,
				   TIL_TileList *tiles);

    // if we expand or change the image bounds, we need to override this method
    // and set the new bounds.
    virtual void	computeImageBounds(COP2_Context &context);

private:
		 COP2_Wavelets(OP_Network *parent, const char *name,
				OP_Operator *entry);

    int	WLFTYPE  (int t) { return evalInt("wlftype", 0, t); }
    int	WLNUM    (int t) { return evalInt("wlnum", 0, t); }
    int	IFSCALE  (int t) { return evalInt("ifScale", 0, t); }
    int	NUMSCALE (int t) { return evalInt("numScale", 0, t); }
    int	NUMCOMP  (int t) { return evalInt("numComp", 0, t); }
};

class cop2_FullImageFilterData : public COP2_ContextData
{
public:
		 cop2_FullImageFilterData() : myWlfType(0), myWlNum(4) {}
    virtual	~cop2_FullImageFilterData() {}

    int 	 myWlfType;
    int          myWlNum;
    int          myIfScale;
    int          myNumScale;
    int          myNumComp;
    UT_Lock	 myLock;
};

} // End HDK_Sample namespace

#endif

