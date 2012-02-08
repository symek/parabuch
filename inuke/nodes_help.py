# -*- coding: iso-8859-1 -*-
Gamma="""
Knobs: value=float, channels=string, mix=float,
Shorts: value,mix
Exampes: 
--Gamma 2.2
--Gamma value=2.2:mix=0.5
-g 2.2,0.5
"""

Crop="""
Knobs: box=tuple
Shorts: box,box,box,box
Examples:
--Crop box=0,0,720,576
-c 0,0,0,0,0.5"""

default="""See the Nuke Node's help for info:
Example:
--Node knob=value:knob=value,...
or simplified (in case shortcuts were defined):
--Transform 0,10,60 (i.e. --Transform translate=0,10:rotate=60)
"""

Reformat="""
Knobs: type=string, scale=float
Shorts: type,scale,black_outside
Examples:
--Reformat type=scale:scale=0.5
--Reformat scale,0.5,
--Reformat to_format,HD,1
"""

Grade="""
Knobs: blackpoints,whitepoints,lift,gain,multipy,offset,gamma=floats
Shorts: gain,multiply,offset,gamma
Examples:
--Grade 0,1,0,1,1.5,0,2.2
etc
"""

ColorCorrect="""
Knobs: [shadows,midtones,highlights] *.saturation, *.contrast, *.gamma, *.gain, *.offset=float
Examples:
--ColorCorrect shadows.saturation=1.4:midtones.gamma=0.95
"""

Merge="""
Knobs: operation=string [over, under, screen, plus, atop], input, input
Shorts: operation,input,input
Examples:
--Merge over,A,B
"""

Blur="""
Knobs: channels=string, size,size=floats, quality=float, mix=float
Shorts: size,size
Examples:
--Blur 20
--Blur 20,0
--Blur size=20,20
"""

Transform="""
Knobs: translate, translate,  rotate, scale, scale,
Exmamples:
--Transform 0,0,90,1,1
--Transform translate=10,30:rotate=90
"""

Log2Lin="""
Knobs: operation [lin2log, log2lin]
Shorts: operation
Exampes:
--Lin2Log lin2log
"""

GENERAL = """

	icomp v01 alpha 21.05.2010
	(Nuke 6.0v4, Python2.6)
	
	<<icomp>> is a commend line image compositing utility which runs Nuke under the hood.
	Type: icomp -h for full descrption.
	___________
	
	EXAMPLES:
	
	a) $ icomp ./render.0001.exr --Gamma 2.2 ./moj_render.%04d.jpg -irm
	
		(Local rendering (-r), Single frame (-i), display in mplay (-m)
	
	
	b) $ icomp ./render --Grade gain=1.5 ./render2.0001.tif --Merge over,0 ./outout.###.exr -s
	
		(Compositing, grading, (partial name of input!). Render on farm* (-s))
		
	c) Other examples:
	   --Remove keep,rgba
	   --Reformat format=HD (PAL)
	   --Write file=./plate.###.exr:autocrop=1
	   --ColorCorrect midtones.saturation=2.0
	   --Log2Lin log2lin
	   --Crop 0,0,720,576
	   --Blur size=20,40:type="gauss":mix=0.5
	   --Write file=./plate.mov:file_type=ffmpeg:bitrate=6000000 -f --slots 1 # it's a MUST!
	   --globals 0,250,24
	   --Read ./render_HD.%04d.dpx,1,1
	   
	
	As you see "--Read" and "--Write" are optional, but they allow to access all node's knobs.
	__________
	
	KNOBS:
	  Parameters follow names from Nuke:
		--Blur size=20,40:type="gauss":mix=0.5
		
	  For simplicity you can use shortcuts (they are defined for popular nodes, see -h "shorts")
	  instead of --Transform translate=100,30:rotate=90:scale=1.4,2
	  you could: --Transform 100,30,90,1.4,2
	    or less: --Transform 100,30
	_______________
	
	COMPOSITING:
	  Compositing nodes connect by default with previously specified files. Second layer we add with a paramter:
		$ icomp file0 file1 --Merge screen,0 equals: $ icomp file0 file1 --Merge screen,1,0

	"""











