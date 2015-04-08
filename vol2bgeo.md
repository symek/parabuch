# vol2bgeo (and i3d) #

This small utility converts 3 dimensional tile-able textures provided by
bellow researches into Houdini's bgeo volume or i3d texture format. It can be used after that to texture breakable objects or build dimensional structures with these images (with iso-surfaces).

Authors of the original paper kindy provide a number of synthetized
textures on their website.

http://johanneskopf.de/publications/solid/index.html

```
@article{KFCODLW07,
    author  = {Johannes Kopf and Chi-Wing Fu and Daniel Cohen-Or and 
               Oliver Deussen and Dani Lischinski and Tien-Tsin Wong},
    title   = {Solid Texture Synthesis from 2D Exemplars},
    journal = {ACM Transactions on Graphics (Proceedings of SIGGRAPH 2007)},
    year    = {2007},
    volume  = {26},
    number  = {3},
    pages   = {2:1--2:9},
}
```

There is also a Maya plugin integrating .vol textures directly into Maya:
http://johanneskopf.de/publications/solid/plugin/index.html

# Examples #

Here is an example of the texture and its usage:

![http://imageshack.us/photo/my-images/85/brickwall2exemplar.png](http://imageshack.us/photo/my-images/85/brickwall2exemplar.png)

![http://imageshack.us/photo/my-images/526/bricksbreaked3dtextured.jpg](http://imageshack.us/photo/my-images/526/bricksbreaked3dtextured.jpg)