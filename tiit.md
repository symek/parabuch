# TIIT (tiny image inspection tool) #

TIIT is a tool for image files inspection based on Houdini's
HDK image library.

# Dependencies #

  * The goal is to keep a tool dependent only on HDK, but currently I'm using openssl for sha-1 generation. This will be replaced eventually with some header self-contained implementation.

# Instalation #

  * Setup Houdini environment and then:
```
hcustom -L/usr/lib -lssl -s src/tiit.C
```

# Remarks, or what for I need this? #

  * A tool opening all industry formats without problems. _(Even OpenImageIO has difficulties with OpenEXR with variable bir depth per plane)_.
  * Compute check sums per plane _(sha-1)_.
  * Find Nans and Inf, and optionally fix it _(shouldn't do this without supervision though)_.
  * Compute statistics.
  * Compute similarity ranks enabling to group images by contents.
  * Find a way to recognize images with missing buckets. This is tough without renderers hints or log, but in many cases handy.
  * Enable to read/write meta-data.