#include <stdio.h>
#include <iostream>
#include <CMD/CMD_Args.h>
#include <UT/UT_Assert.h>
#include <GEO/GEO_AttributeHandle.h>
#include <GU/GU_Detail.h>
#include <GU/GU_PrimVolume.h>

static void
usage(const char *program)
{
    cerr << "Usage: " << program << " sourcefile dstfile\n";
    cerr << "The extension of the source/dest will be used to determine" << endl;
    cerr << "how the conversion is done.  Supported extensions are .voxel" << endl;
    cerr << "and .bgeo" << endl;
}


bool
voxelLoad(UT_IStream &is, GU_Detail *gdp)
{
    // Check our magic token
    if (!is.checkToken("VOXELS"))
        return false;

    GEO_AttributeHandle         name_gah;
    int                         def = -1;

    gdp->addPrimAttrib("name", sizeof(int), GB_ATTRIB_INDEX, &def);
    name_gah = gdp->getPrimAttribute("name");

    while (is.checkToken("VOLUME"))
    {
        UT_String               name;
        UT_WorkBuffer           buf;

        is.getWord(buf);
        name.harden(buf.buffer());

        int                     rx, ry, rz;

        is.read(&rx); is.read(&ry); is.read(&rz);

        // Center and size
        float                   tx, ty, tz, sx, sy, sz;

        is.read(&tx); is.read(&ty); is.read(&tz);
        is.read(&sx); is.read(&sy); is.read(&sz);

        GU_PrimVolume           *vol;

        vol = (GU_PrimVolume *)GU_PrimVolume::build(gdp);

        // Set the name of the primitive
        name_gah.setElement(vol);
        name_gah.setString(name);

        // Set the center of the volume
        vol->getVertex().getPt()->getPos() = UT_Vector3(tx, ty, tz);

        UT_Matrix3              xform;

        // The GEO_PrimVolume treats the voxel array as a -1 to 1 cube
        // so its size is 2, so we scale by 0.5 here.
        xform.identity();
        xform.scale(sx/2, sy/2, sz/2);

        vol->setTransform(xform);

        UT_VoxelArrayWriteHandleF       handle = vol->getVoxelWriteHandle();

        // Resize the array.
        handle->size(rx, ry, rz);

        if (!is.checkToken("{"))
            return false;

        for (int z = 0; z < rz; z++)
        {
            for (int y = 0; y < ry; y++)
            {
                for (int x = 0; x < rx; x++)
                {
                    float               v;

                    is.read(&v);

                    handle->setValue(x, y, z, v);
                }
            }
        }

        if (!is.checkToken("}"))
            return false;

        // Proceed to the next volume.
    }

    // All done successfully
    return true;
}

bool
voxelLoad(const char *fname, GU_Detail *gdp)
{
    // The UT_IFStream is a specialization of istream which has a lot
    // of useful utlity methods and some corrections on the behaviour.
    UT_IFStream is(fname, UT_ISTREAM_ASCII);

    return voxelLoad(is, gdp);
}

bool
voxelSave(ostream &os, const GU_Detail *gdp)
{
    // Write our magic token.
    os << "VOXELS" << endl;

    // Now, for each volume in our gdp...
    const GEO_Primitive         *prim;
    GEO_AttributeHandle                  name_gah;
    UT_String                            name;
    UT_WorkBuffer                        buf;

    name_gah = gdp->getPrimAttribute("name");
    FOR_ALL_PRIMITIVES(gdp, prim)
    {
        if (prim->getPrimitiveId() == GEOPRIMVOLUME)
        {
            // Default name
            buf.sprintf("volume_%d", prim->getNum());
            name.harden(buf.buffer());

            // Which is overridden by any name attribute.
            if (name_gah.isAttributeValid())
            {
                name_gah.setElement(prim);
                name_gah.getString(name);
            }

            os << "VOLUME " << name << endl;
            const GEO_PrimVolume        *vol = (GEO_PrimVolume *) prim;

            int         resx, resy, resz;

            // Save resolution
            vol->getRes(resx, resy, resz);
            os << resx << " " << resy << " " << resz << endl;

            // Save the center and approximate size.
            // Calculating the size is complicated as we could be rotated
            // or sheared.  We lose all these because the .voxel format
            // only supports aligned arrays.
            UT_Vector3          p1, p2;

            os << vol->getVertex().getPt()->getPos().x() << " "
               << vol->getVertex().getPt()->getPos().y() << " "
               << vol->getVertex().getPt()->getPos().z() << endl;

            vol->indexToPos(0, 0, 0, p1);
            vol->indexToPos(1, 0, 0, p2);
            os << resx * (p1 - p2).length() << " ";
            vol->indexToPos(0, 1, 0, p2);
            os << resy * (p1 - p2).length() << " ";
            vol->indexToPos(0, 0, 1, p2);
            os << resz * (p1 - p2).length() << endl;

            UT_VoxelArrayReadHandleF handle = vol->getVoxelHandle();

            // Enough of a header, dump the data.
            os << "{" << endl;
            for (int z = 0; z < resz; z++)
            {
                for (int y = 0; y < resy; y++)
                {
                    os << "    ";
                    for (int x = 0; x < resx; x++)
                    {
                        os << (*handle)(x, y, z) << " ";
                    }
                    os << endl;
                }
            }
            os << "}" << endl;
            os << endl;
        }
    }

    return true;
}

bool
voxelSave(const char *fname, const GU_Detail *gdp)
{
    ofstream    os(fname);

    // Default output precision of 6 will not reproduce our floats
    // exactly on load, this define has the value that will ensure
    // our reads match our writes.
    os.precision(SYS_FLT_DIG);

    return voxelSave(os, gdp);
}


// Convert a volume into a toy ascii voxel format.
//
// Build using:
//      hcustom -s geo2voxel.C
//
// Example usage:
//      geo2voxel input.bgeo output.voxel
//      geo2voxel input.voxel output.bgeo
//
// You can add support for the .voxel format in Houdini by editing
// your GEOio table file and adding the line
// .voxel "geo2voxel %s stdout.bgeo" "geo2voxel stdin.bgeo %s"
//
int
main(int argc, char *argv[])
{
    CMD_Args             args;
    GU_Detail            gdp;

    args.initialize(argc, argv);

    if (args.argc() != 3)
    {
        usage(argv[0]);
        return 1;
    }

    // Check if we are converting from .voxel.  If the source extension
    // is .voxel, we are converting from.  Otherwise we convert to.
    // By being liberal with our accepted extensions we will support
    // a lot more than just .bgeo since the built in gdp.load() and save()
    // will handle the issues for us.

    UT_String           inputname, outputname;

    inputname.harden(argv[1]);
    outputname.harden(argv[2]);

    if (!strcmp(inputname.fileExtension(), ".voxel"))
    {
        // Convert from voxel
        voxelLoad(inputname, &gdp);

        // Save our result.
        gdp.save((const char *) outputname, 0, 0);
    }
    else
    {
        // Convert to voxel.
        gdp.load((const char *) inputname, 0);

        voxelSave(outputname, &gdp);
    }
    return 0;
}
