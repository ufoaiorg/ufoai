==General info==

* Only tga, jpg and png images please
* The filename must be <mapname>_<level>.<extension>
* The dimensions must be the same for every level image
* In case of a random map assembly every map tile needs a radar image.
* Make screenshots by executing "gen_radarmap" and set r_screeshots
  to tga and cut the borders of the screenshot away.
  Alphachannel is allowed here. The images must not be bigger than
  1024x768. Use the day versions of the map.

==How to generate images==

===For static maps===

The image doesn't need to have a fixed or proportional size, but must contain
everything into the map (mapMin, mapMax) and no more.

* Run the game in 1024x1024
	> ufo +set vid_mode -1 +set vid_width 1024 +set vid_height 1024;
* Open the map in skirmish
* Open the console and type:
	> gen_radarmap
* You should use ".jpg" image, on the console type:
	> r_screenshot_format = jpg
* Click on the button "generate all levels" and wait
	It generates all screenshots into your /scrnshot directory
* you can move these screenshots into /base/pics/radars

===For RMA===

There is no easy way. Size of image of RMA tile are normalized.

A tile of 8 cells must have a size near 94 pixels.

For example, "forest" RMA uses at the moment:
* 8 cells -> 93/94 pixels (94x1 explected)
* 16 cells -> 187 pixels (94x2=188 explected)
* 24 cells -> 281 pixels (94x3=282 explected)
