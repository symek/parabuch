# Point Cache SOP #

**PointCache SOP** is a Houdini operator for applying point
deformations stored in popular PC2 format.

  * Point groups for selective application.
  * x40 faster from usual Python SOPs.
  * Interpolates lineary or cubicly point's positions.
  * Recomputes normals.
  * Adds rest attribute.
  * Python script for merging multiply pc2 into a single file.

# Installation #

Source Houdini's environment:
```
> cd houdini/path
> source hooudini_setup_bash
```

and then:
```
> cd pointcache
> hcustom src/SOP_PointCache.C
```

It should place SOP\_PointCache.so to $HOME/houdiniX.X/dso
ready to be used.