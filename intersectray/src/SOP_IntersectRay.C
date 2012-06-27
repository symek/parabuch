// HDK:
#include <UT/UT_DSOVersion.h>
#include <UT/UT_Math.h>
#include <UT/UT_Matrix3.h>
#include <UT/UT_Matrix4.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimPoly.h>
#include <PRM/PRM_Include.h>
#include <OP/OP_Operator.h>
#include <OP/OP_OperatorTable.h>
#include <SOP/SOP_Guide.h>
#include <GU/GU_RayIntersect.h>

// Own:
#include "SOP_IntersectRay.h"

// Std:
#include <time.h>

using namespace INTERSECT_RAY;

void
newSopOperator(OP_OperatorTable *table)
{
     table->addOperator(new OP_Operator("intersectRay",
					"Intersect Ray",
					 SOP_IntersectRay::myConstructor,
					 SOP_IntersectRay::myTemplateList,
					 1,
					 1,
					 0));
}

static PRM_Name        names[] = {
    PRM_Name("amp",	"Amplitude"),
    PRM_Name("phase",	"Phase"),
    PRM_Name("period",	"Period"),
};

PRM_Template
SOP_IntersectRay::myTemplateList[] = {
    PRM_Template(PRM_STRING,    1, &PRMgroupName, 0, &SOP_Node::pointGroupMenu),
    PRM_Template(PRM_FLT_J,	1, &names[0], PRMoneDefaults, 0,
				   &PRMscaleRange),
    PRM_Template(PRM_FLT_J,	1, &names[1], PRMzeroDefaults),
    PRM_Template(PRM_FLT_J,	1, &names[2], PRMoneDefaults),
    PRM_Template(),
};


OP_Node *
 SOP_IntersectRay::myConstructor(OP_Network *net, const char *name, OP_Operator *op)
{
    return new  SOP_IntersectRay(net, name, op);
}

SOP_IntersectRay:: SOP_IntersectRay(OP_Network *net, const char *name, OP_Operator *op)
	: SOP_Node(net, name, op), myGroup(0)
{
}

SOP_IntersectRay::~SOP_IntersectRay() {}

OP_ERROR
SOP_IntersectRay::cookInputGroups(OP_Context &context, int alone)
{
    // The SOP_Node::cookInputPointGroups() provides a good default
    // implementation for just handling a point selection.
    return cookInputPointGroups(context, myGroup, myDetailGroupPair, alone);
}

OP_ERROR
SOP_IntersectRay::cookMySop(OP_Context &context)
{
    double		 t;
    //float		 amp, period, phase;

    if (lockInputs(context) >= UT_ERROR_ABORT)
	return error();

    t = context.getTime();
    duplicatePointSource(0, context);

    //phase = PHASE(t);
    //amp = AMP(t);
    //period = PERIOD(t);

    // Normals:
    GEO_AttributeHandle  Nhandle;
    GEO_AttributeHandle  Cdhandle;
    UT_Vector3           n;         
    Nhandle  = gdp->getAttribute(GEO_POINT_DICT, "N");
    Cdhandle = gdp->getAttribute(GEO_POINT_DICT, "Cd");

    // RayInfo parms:
    //float max = 1E18f; 
    float min = 0.0f;
    float tol = 1e-1F;

    // Rayhit objects:
     GU_RayFindType  itype     = GU_FIND_ALL;   
     GU_RayIntersect intersect = GU_RayIntersect(gdp);      

    // Profile:
    //Timer timer = Timer();
    //float rayhit_time = 0;
    //float vertex_time = 0;

    // Here we determine which groups we have to work on.  We only
    //	handle point groups.
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
     	for (int i = 0; i < gdp->primitives().entries(); i++)
	    {
            
            const GEO_Primitive   *ppr;
            ppr = gdp->primitives()(i);
            for (int j = 0; j < ppr->getVertexCount() - 1; j++)
            {
                const GEO_Vertex ppv1 = ppr->getVertex(j);
                const GEO_Vertex ppv2 = ppr->getVertex(j+1);
                const GEO_Point *ppt1;
                const GEO_Point *ppt2;
                      UT_Vector3 p1;
                      UT_Vector3 p2;

                ppt1 = ppv1.getPt();
                p1   = ppt1->getPos();
                ppt2 = ppv2.getPt();
                p2   = ppt2->getPos();
                p2   = p2 - p1;

                // hit info wiht max distance equal edge length:
                GU_RayInfo  hitinfo = GU_RayInfo(p2.length(), min, itype, tol); 
                p2.normalize();
                if (intersect.sendRay(p1, p2, hitinfo))
                {
                    {
                        UT_RefArray<GU_RayInfoHit> hits = *(hitinfo.myHitList);
                        for(int j = 0; j < hits.entries(); j++)
                        {
                            const GEO_Primitive *prim = hits[j].prim;
                            // TODO: Prim only?
                            const GEO_PrimPoly  *poly = (const GEO_PrimPoly *) prim;
                            if (poly->find(*ppt1) == -1 && poly->find(*ppt2)== -1)
                            {
                                printf("Edge %i-%i intersects with prim:%d \n",ppt1->getNum(), ppt2->getNum(), prim->getNum());
                                Cdhandle.setElement(ppt1);
                                Cdhandle.setV3(UT_Vector3(1.0, 0.0, 0.0));
                                Cdhandle.setElement(ppt2);
                                Cdhandle.setV3(UT_Vector3(1.0, 0.0, 0.0));
                            }
                        }
                    }
                }
            }
	    } 
    }

    gdp->notifyCache(GU_CACHE_ALL);
    unlockInputs();
    
    return error();
}
