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
#include <UT/UT_Interrupt.h>

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
    PRM_Name("edgelength",	"Min Edge Length"),
    PRM_Name("primarea",	"Min Prim Area"),
    PRM_Name("verbose",	    "Verbose"),
};

static PRM_Default  threashold_01(0.1f);
static PRM_Default  threashold_02(0.2f);
static PRM_Default  group_all(0, "*");

PRM_Template
SOP_IntersectRay::myTemplateList[] = {
    PRM_Template(PRM_STRING,    1, &PRMgroupName, &group_all, &SOP_Node::primGroupMenu),
    PRM_Template(PRM_FLT,	 1, &names[0], &threashold_01),
    PRM_Template(PRM_FLT,	 1, &names[1], &threashold_02), //PRMzeroDefaults
    PRM_Template(PRM_TOGGLE, 1, &names[2], 0),
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
    // TODO: I can't change empy group field into "*". 
    // But emply selection crashes Houdini!
    /*
    UT_String group_name;
    getGroups(group_name);
    const GB_PrimitiveGroup *group;
    if (group_name.length() == 0)
    {    
        group = parsePrimitiveGroups("*");
        myGroup = group;
    }*/
 
    return cookInputPrimitiveGroups(context, myGroup, myDetailGroupPair, alone);   
}

OP_ERROR
SOP_IntersectRay::cookMySop(OP_Context &context)
{
    double t;
    float  edgelength, primarea;
    int    verbose;

    if (lockInputs(context) >= UT_ERROR_ABORT)
	return error();

    t = context.getTime();
    duplicatePointSource(0, context);

    edgelength = EDGELENGTH(t);
    primarea   = PRIMAREA(t);
    verbose    = VERBOSE(t);

    // Normals:
    GEO_AttributeHandle  nH;
    GEO_AttributeHandle  cdH;
    UT_Vector3           n;         
    nH  = gdp->getAttribute(GEO_POINT_DICT, "N");
    cdH = gdp->getAttribute(GEO_POINT_DICT, "Cd");

    // RayInfo parms:
    //float max = 1E18f; // Max specified by edge length...
    float min = 0.0f;
    float tol = 1e-1F;

    // Rayhit objects:
    GU_RayFindType  itype     = GU_FIND_ALL;   
    GU_RayIntersect intersect = GU_RayIntersect(gdp);

    // Profile:
    //Timer timer = Timer();
    //float rayhit_time = 0;
    //float vertex_time = 0;

    // Here we determine which groups we have to work on. 
    if (error() < UT_ERROR_ABORT && cookInputGroups(context) < UT_ERROR_ABORT)
    {
        UT_AutoInterrupt progress("Checking for self-intersections...");
        const GEO_Primitive   *ppr;
        FOR_ALL_GROUP_PRIMITIVES(gdp, myGroup, ppr)
	    {
             // Check if user requested abort
             if (progress.wasInterrupted())
                break;

            // Get rid off primitives smaller than primarea:
            if ( ppr->calcArea() < primarea )
                continue;

            for (int j = 0; j < ppr->getVertexCount() - 1; j++)
            {
                // Get data;
                // TODO: This is extremally inefficent.
                // TODO: Why do we crash with uv vetrex attributes!? 
                const GEO_Vertex ppv1 = ppr->getVertex(j);
                const GEO_Vertex ppv2 = ppr->getVertex(j+1);
                const GEO_Point *ppt1 = ppv1.getPt();
                const GEO_Point *ppt2 = ppv2.getPt();
               
                // Vertices positions:
                UT_Vector3 p1 = ppt1->getPos();
                UT_Vector3 p2 = ppt2->getPos();

                // Ray direction:
                p2   = p2 - p1;
                // Get rid off edges shorter than edgelength:
                if (p2.length() < edgelength)
                    continue;
                // hit info with max distance equal edge length:
                GU_RayInfo  hitinfo = GU_RayInfo(p2.length(), min, itype, tol); 
                p2.normalize();

                // Send ray:
                if (intersect.sendRay(p1, p2, hitinfo))
                {
                    {
                        UT_RefArray<GU_RayInfoHit> hits = *(hitinfo.myHitList);
                        for(int j = 0; j < hits.entries(); j++)
                        {
                            const GEO_Primitive *prim = hits[j].prim;
                            const GEO_PrimPoly  *poly = (const GEO_PrimPoly *) prim; //TODO: Prims only?
                            // We are interested only ff points are not part of prims...:
                            if (poly->find(*ppt1) == -1 && poly->find(*ppt2)== -1)
                            {
                                if (verbose)
                                    printf("Edge: %i-%i intersects with prim:%d \n",ppt1->getNum(), ppt2->getNum(), prim->getNum());

                                cdH.setElement(ppt1);
                                cdH.setV3(UT_Vector3(1.0, 0.0, 0.0));
                                cdH.setElement(ppt2);
                                cdH.setV3(UT_Vector3(1.0, 0.0, 0.0));
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
