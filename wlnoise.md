# VEX Wavelet Noise #
i.e. Wavelet Noise by Robert L. Cook Tony DeRose. Here is a [paper](http://graphics.pixar.com/library/WaveletNoise/paper.pdf)

## Installation ##

  * Set up Houdini environment and then:
```
hcustom wlnoise/src/wlnoise.C
```
to install in default location ($HOME/houdinix.x/dso/).
  * Then:

```
 vcc -l $HIH/otls/waveletnoise.otl wlnoise/vex/waveletnoise.vfl
```
to compile an example shader and place in a path (use '-m' flag instead of '-l' to compile to Vop type).

  * Finally you need a VEXdso file (for example copied from $HH/vex), with a single line naming dso added.

## Usage ##
DSO includes three versions of the function:
  * `float wlnoise(vector p)` - single band wavelet noise
  * `float wlpnoise(vector p, vector n)` - single band N-projected noise
  * `float wlmnoise(vector p,  float s, vector N, int f, int n, vector w)` multi band projected noise with custom weights per band.
  * example shader defines basic turbulance-like loop for multi band noise, but I think this screw up band limited signal.