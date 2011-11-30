#! /usr/bin/env python
#
# Following code is distributed under the GNU General Public License version 2
#
'''
A small library to parse and assemble UFO:AI .map files
Python 2.6 required

'''
class Face:
    def __init__(self):
        self.vertexes = [ [0.0, 0.0, 0.0], [0.0, 0.0, 0.0], [0.0, 0.0, 0.0] ]
        self.texture = "tex_common/nodraw"
        self.offset = [0.0, 0.0]
        self.angle = 0.0
        self.scale = [0.0, 0.0]
        self.cont_flags = 0
        self.surf_flags = 0
        self.surf_value = 0

    def to_string(self): #todo: implement __format__
        verts = "( {0:.8g} {1:.8g} {2:.8g} ) ( {3:.8g} {4:.8g} {5:.8g} ) ( {6:.8g} {7:.8g} {8:.8g} )".format(self.vertexes[0][0], self.vertexes[0][1], self.vertexes[0][2], self.vertexes[1][0], self.vertexes[1][1], self.vertexes[1][2], self.vertexes[2][0], self.vertexes[2][1], self.vertexes[2][2])
        tex = "{0:.8g} {1:.8g} {2:.8g} {3:.8g} {4:.8g}".format(self.offset[0], self.offset[1], self.angle, self.scale[0], self.scale[1])
        flags = "{0} {1} {2}".format(self.cont_flags, self.surf_flags, self.surf_value)
        return " ".join((verts, self.texture, tex, flags))

class Brush:
    def __init__(self):
        self.faces = []

    def to_string(self): #todo: implement __format__
        tmp = ["{"]
        for i in self.faces:
            tmp.append(i.to_string())
        tmp.append("}");
        return "\n".join(tmp)

class Entity:
    def __init__(self):
        self.brushes = []
        self.fields = {}
        self.order = []

    def to_string(self):
        tmp = ["{"]
        fields = self.fields.copy()
        for i in self.order:
            if i in fields:
                tmp.append('"{0}" "{1}"'.format(i, fields[i]))
                del fields[i]
        for i in fields:
            tmp.append('"{0}" "{1}"'.format(i, self.fields[i]))
        brushCount = 0
        for i in self.brushes:
            tmp.append("// brush {0}".format(brushCount))
            tmp.append(i.to_string())
            brushCount += 1
        tmp.append("}")
        return "\n".join(tmp)

def parse_ufo_map(s, verbose = False):
    l = s.splitlines()
    if verbose: print len(l), "lines"

    depth = 0 # 1 - entity, 2 - brush level
    ent = Entity()
    brush = Brush()
    ufo_map = []

    for n, i in enumerate(l):
        tokens = i.partition("//")[0].split() #remove comments and tokenize each line
        count = len(tokens)
        if count == 0: continue #no statements in this line
        elif count == 1:
            if tokens[0] == "{":
                depth += 1
                if depth > 2:
                    print "Syntax error - unexpected { in line", n
                    break
            elif tokens[0] == "}":
                depth -= 1
                if depth == 0: #finalize entity and create new one
                    ufo_map.append(ent)
                    #print "entity", ent.fields.get("classname", "")
                    ent = Entity()
                elif depth == 1: #add brush to entity
                    ent.brushes.append(brush)
                    brush = Brush()
                else:
                    print "Syntax error - unexpected } in line", n
                    break
            else:
                print "Syntax error - can't analyze line", n
                break
        elif depth == 1 and tokens[0][0] == '"': #entity field definition
            tokens = i.split('"') #reparse using " as separator
            if len(tokens) != 5:
                print "Syntax error - can't analyze line", n
                break
            ent.fields[tokens[1]] = tokens[3]
            ent.order.append(tokens[1])
        elif depth == 2 and tokens[0][0] == '(': #brush face definition
            face = Face()
            if count > 13 and tokens[0] == "(" and tokens[4] == ")" and tokens[5] == "(" and tokens[9] == ")" and tokens[10] == "(" and tokens[14] == ")":
                face.vertexes[0] = [float(tokens[1]), float(tokens[2]), float(tokens[3])]
                face.vertexes[1] = [float(tokens[6]), float(tokens[7]), float(tokens[8])]
                face.vertexes[2] = [float(tokens[11]), float(tokens[12]), float(tokens[13])]
            else:
                print "Syntax error - can't analyze line", n
                break
            if count > 15: face.texture = tokens[15]
            if count > 17: face.offset = [ float(tokens[16]), float(tokens[17]) ]
            if count > 18: face.angle = float(tokens[18])
            if count > 20: face.scale = [ float(tokens[19]), float(tokens[20]) ]
            if count > 21: face.cont_flags = int(tokens[21])
            if count > 22: face.surf_flags = int(tokens[22])
            if count > 23: face.surf_value = int(tokens[23])
            brush.faces.append(face)
        else:
            print "Syntax error - can't analyze line", n
            break
    if verbose: print len(ufo_map), "entities read"
    return ufo_map

def ufo_map_to_string(ufo_map):
    tmp = [""] #Radiant output starts with empty line for some reason
    entityCount = 0
    for i in ufo_map:
        tmp.append("// entity {0}".format(entityCount))
        tmp.append(i.to_string())
        entityCount += 1
    tmp.append("\n")
    return "\n".join(tmp)

def read_ufo_map(name, verbose = False):
    f = open(name)
    if verbose: print name
    s = f.read()
    f.close()
    return parse_ufo_map(s, verbose)

def write_ufo_map(ufo_map, name):
    f = open(name, "wb")
    f.write(ufo_map_to_string(ufo_map))
    f.close()
