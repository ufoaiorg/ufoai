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

* Run the game in 1024x1024
	> ufo +set vid_mode -1 +set vid_width 1024 +set vid_height 1024;
* Open the map in skirmish
* Open the console and type:
	> gen_radarmap
* You should use ".jpg" image, on the console type:
	> r_screenshot_format = jpg
* Click on the button "generate all levels" and wait
	It generate all screenshots into your /scrnshot directory
* you can move this screenshots into /base/pics/radars