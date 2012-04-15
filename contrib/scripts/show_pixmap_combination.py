#!/usr/bin/env python
# -*- coding: utf-8; tab-width: 4; indent-tabs-mode: nil -*-

import dircache
import os.path
import Image
import sys

UFOAI_ROOT = os.path.realpath(sys.path[0] + '/../..')

class MapComposition(object):

    def __init__(self):
        file_name = os.path.join(UFOAI_ROOT, "base/pics/geoscape/map_earth_population.png")
        self.population = Image.open(file_name)
        file_name = os.path.join(UFOAI_ROOT, "base/pics/geoscape/map_earth_culture.png")
        self.culture = Image.open(file_name)
        file_name = os.path.join(UFOAI_ROOT, "base/pics/geoscape/map_earth_terrain.png")
        self.terrain = Image.open(file_name)
        size = self.population.size
        if size != self.culture.size:
            raise Exception("Image for culture do not have the same size than image for population")
        if size != self.terrain.size:
            raise Exception("Image for terrain do not have the same size than image for population")

    def get_size(self):
        return self.population.size

    def get_count(self):
        return self.population.size[0] * self.population.size[1]

    def get_terrain(self, x, y):
        color = self.terrain.getpixel((x, y))
        color = (color[0], color[1], color[2])
        value = None
        if color == (0, 0, 64):
            value = "water"
        elif color == (128, 255, 0):
            value = "grass"
        elif color == (0, 0, 255):
            value = "cold"
        elif color == (255, 0, 0):
            value = "mountain"
        elif color == (128, 128, 255):
            value = "tropical"
        elif color == (128, 255, 255):
            value = "arctic"
        elif color == (255, 128, 0) or color == (255, 128, 64):
            value = "desert"
        else:
            raise Exception("Terrain color %s unsupported" % str(color))
        return value

    def get_culture(self, x, y):
        color = self.culture.getpixel((x, y))
        color = (color[0], color[1], color[2])
        value = None
        if color == (0, 0, 64):
            value = "water"
        elif color == (255, 0, 0):
            value = "oriental"
        elif color == (128, 128, 255):
            value = "african"
        elif color == (128, 255, 255):
            value = "western"
        elif color == (255, 128, 0) or color == (255, 128, 64):
            value = "eastern"
        else:
            raise Exception("Culture color %s unsupported" % str(color))
        return value

    def get_population(self, x, y):
        color = self.population.getpixel((x, y))
        color = (color[0], color[1], color[2])
        value = None
        if color == (0, 0, 64):
            value = "water"
        elif color == (128, 255, 0):
            value = "nopopulation"
        elif color == (255, 0, 0):
            value = "village"
        elif color == (128, 128, 255):
            value = "rural"
        elif color == (128, 255, 255):
            value = "urban"
        elif color == (255, 128, 0):
            value = "suburban"
        else:
            raise Exception("Population color %s unsupported" % str(color))
        return value

    def get_composition(self, x, y):
        return "%s,%s,%s" % (self.get_terrain(x, y), self.get_culture(x, y), self.get_population(x, y))

    size = property(get_size)
    count = property(get_count)

class TxtOutput:
    def begin(self):
        pass
    def end(self):
        pass
    def value(self, key, pixels, percent, percent_of_land):
        percent = "%6.2f%% of pixels" % percent
        if percent_of_land == None:
            percent_of_land = "n/a"
        else:
            percent_of_land = "%6.2f%% of landable pixels" % percent_of_land
        print "%- 40s %6d %s    %s" % (key, pixels, percent, percent_of_land)

class WikiOutput:
    def begin(self):
        print """{| class="ufotable sortable"
|+ Map combinations
|-
! scope="col" | Terrain
! scope="col" | Culture
! scope="col" | Population
! scope="col" data-sort-type="number" | Nb pixels
! scope="col" data-sort-type="number" | % of pixels
! scope="col" data-sort-type="number" | % of landable pixels"""
    def end(self):
        print "|}"
    def value(self, key, pixels, percent, percent_of_land):
        t, c, p = key.split(",")
        if percent_of_land == None:
            percent_of_land = "n/a"
        else:
            percent_of_land = "%.2f" % percent_of_land
        percent = "%.2f" % percent
        print "|-"
        print "| %s || %s || %s || %d || %s || %s " % (t, c, p, pixels, percent, percent_of_land)

if __name__ == "__main__":

    output = TxtOutput()
    if len(sys.argv) == 2:
        if sys.argv[1] == "wiki":
            output = WikiOutput()

    map_composition = MapComposition()
    size = map_composition.size
    count = map_composition.count
    count_water = 0

    state = {}

    for y in xrange(0, size[1]):
        for x in xrange(0, size[0]):
            composition = map_composition.get_composition(x, y)
            if composition in state:
                state[composition] = state[composition] + 1
            else:
                state[composition] = 1

    keys = state.keys()
    keys.sort(key=lambda k: state[k])
    keys.reverse()

    for k in keys:
        if "water" in k:
            count_water += state[k]

    output.begin()
    for k in keys:
        percent = state[k] * 100.0 / count

        percent_of_land = None
        if not ("water" in k):
            percent_of_land = state[k] * 100.0 / (count - count_water)

        output.value(k, state[k], percent, percent_of_land)
    output.end()
